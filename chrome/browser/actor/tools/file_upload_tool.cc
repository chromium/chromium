// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/file_picker_interceptor.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/file_system/in_memory_file_registry.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace actor {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("actor_file_upload_tool", R"(
      semantics {
        sender: "Actor File Upload Tool"
        description:
          "While carrying out a task on the user's behalf, Gemini in Chrome "
          "may need to upload files to a web page. Chrome downloads the "
          "files from the URLs provided as part of the task and hands the "
          "contents to the page in response to its file chooser request, "
          "instead of showing a file picker dialog."
        trigger:
          "An actor task performs a file upload action naming URLs of the "
          "files to attach."
        data:
          "A GET request for the files named by the task. Files the user "
          "uploaded to Gemini are only served to that user, so requests to "
          "the hosts serving them include the user's cookies. Requests to any "
          "other host are sent without cookies."
        destination: WEBSITE
        internal { contacts { owners: "//components/actor/OWNERS" } }
        user_data { type: ACCESS_TOKEN }
        last_reviewed: "2026-10-27"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting:
          "This feature is off unless the user is using Gemini in Chrome to "
          "perform a task on their behalf."
        chrome_policy {
          GeminiSettings {
            GeminiSettings: 1
          }
        }
      })");

// Fallback name used when a name cannot be derived from the source URL.
constexpr char kDefaultFileName[] = "upload";

// Whether `url` is on a host whose files are downloaded with the user's
// credentials. Downloaded contents are handed to the page, so credentials must
// never be attached for arbitrary URLs: that would let a page read any resource
// the user is signed in to.
bool IsCredentialedHost(const GURL& url) {
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  const std::string hosts = kGlicActorFileUploadToolCredentialedHosts.Get();
  for (std::string_view host : base::SplitStringPiece(
           hosts, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(url.host(), host)) {
      return true;
    }
  }
  return false;
}

std::string DisplayNameForSource(const FileUploadSource& source) {
  switch (source.type) {
    case FileUploadSource::Type::kUrl:
      // GenerateFileName sanitizes the result, so it is safe to use as a file
      // name and as the name shown to the page. `file_name` is only a hint: it
      // is ignored if it cannot be sanitized into a usable name, in which case
      // a name is derived from the URL instead.
      return net::GenerateFileName(source.url,    /*content_disposition=*/
                                   std::string(), /*referrer_charset=*/
                                   std::string(), source.file_name,
                                   /*mime_type=*/std::string(),
                                   kDefaultFileName)
          .AsUTF8Unsafe();
  }
}

// Returns the frame whose document holds `target`, which is the only document
// the staged files are handed to, or null if there is none.
content::RenderFrameHost* FindTargetFrame(
    content::WebContents& web_contents,
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& target) {
  if (const DomNode* node = std::get_if<DomNode>(&target)) {
    return optimization_guide::GetRenderFrameForDocumentIdentifier(
        web_contents, node->document_identifier);
  }
  // Coordinates are resolved to the local root under the point, which is where
  // the click is sent. A file input in a same-process subframe is therefore
  // not matched, and its file chooser request takes the regular path.
  return FindTargetLocalRootFrame(actor_surface_handle, target);
}

// Upper bound on the size of a single staged file. Since the contents are held
// in browser process memory, this also bounds how much memory one upload can
// use. DownloadToString refuses to be given a limit above its bounded string
// size, so the configured value is a ceiling rather than a guarantee.
size_t MaxFileSize() {
  return std::min(base::checked_cast<size_t>(std::max(
                      0, kGlicActorFileUploadToolMaxFileSizeBytes.Get())),
                  network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

mojom::ActionResultPtr OnValidatedAllUrls(
    std::vector<mojom::ActionResultPtr> results) {
  for (auto& result : results) {
    // In the case of multiple errors, arbitrarily return the first.
    if (!IsOk(result->code)) {
      return std::move(result);
    }
  }
  return MakeOkResult(/*requires_page_stabilization=*/false);
}

void RunValidationCallback(ToolCallback callback,
                           std::vector<mojom::ActionResultPtr> results) {
  PostResponseTask(std::move(callback), OnValidatedAllUrls(std::move(results)));
}

}  // namespace

FileUploadTool::PendingFile::PendingFile() = default;
FileUploadTool::PendingFile::PendingFile(PendingFile&&) = default;
FileUploadTool::PendingFile& FileUploadTool::PendingFile::operator=(
    PendingFile&&) = default;
FileUploadTool::PendingFile::~PendingFile() = default;

FileUploadTool::FileUploadTool(TaskId task_id,
                               ToolDelegate& tool_delegate,
                               ActorSurface& actor_surface,
                               PageTarget target,
                               std::vector<FileUploadSource> files)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      target_(std::move(target)),
      files_(std::move(files)) {}

FileUploadTool::~FileUploadTool() = default;

void FileUploadTool::Validate(ToolCallback callback) {
  if (!base::FeatureList::IsEnabled(kGlicActorFileUploadTool)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kToolUnknown,
                                /*requires_page_stabilization=*/false,
                                "The file upload tool is not enabled."));
    return;
  }

  if (files_.empty()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kFileUploadEmptyFileList,
                   /*requires_page_stabilization=*/false,
                   "No files provided for file upload"));
    return;
  }

  // Each URL the browser is asked to fetch is subject to the same checks as a
  // navigation destination.
  base::RepeatingCallback<void(mojom::ActionResultPtr)> per_url_callback =
      base::BarrierCallback<mojom::ActionResultPtr>(
          files_.size(),
          base::BindOnce(&RunValidationCallback, std::move(callback)));

  for (const FileUploadSource& file : files_) {
    ValidateUrlIsAcceptableNavigationDestination(file.url, tool_delegate(),
                                                 per_url_callback);
  }
}

mojom::ActionResultPtr FileUploadTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  if (!actor_surface_handle_.Get()) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false, "Tab went away");
  }

  // The click that opens the file chooser is validated by the click tool
  // itself; here only check that the target still resolves in the most recent
  // observation, so a stale node id fails before any file is downloaded.
  if (last_observation && std::holds_alternative<DomNode>(target_) &&
      !FindLastObservedNodeForActionTarget(last_observation, target_,
                                           actor_surface_handle_)) {
    return MakeResult(mojom::ActionResultCode::kInvalidDomNodeId,
                      /*requires_page_stabilization=*/false,
                      "The target was not found in the last observation.");
  }

  return MakeOkResult(/*requires_page_stabilization=*/false);
}

void FileUploadTool::Invoke(ToolCallback callback) {
  CHECK(!invoke_callback_);

  ActorSurface* const actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kTabWentAway,
                   /*requires_page_stabilization=*/false, "Tab went away"));
    return;
  }

  invoke_callback_ = std::move(callback);

  journal().Log(JournalURL(), task_id(), "FileUploadTool::Invoke",
                JournalDetailsBuilder()
                    .Add("file_count", static_cast<int>(files_.size()))
                    .Add("files", files_)
                    .Build());

  StartDownloads();
}

void FileUploadTool::StartDownloads() {
  downloaded_contents_.resize(files_.size());
  pending_downloads_ = files_.size();

  if (pending_downloads_ == 0) {
    OnAllDownloadsComplete();
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      tool_delegate()
          .GetProfile()
          .GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  const size_t max_file_size = MaxFileSize();

  for (size_t i = 0; i < files_.size(); ++i) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = files_[i].url;
    resource_request->method = "GET";
    // Redirects are not followed: only the URL checked by Validate() may be
    // requested, and a credentialed request must not carry the user's
    // credentials to another host. A redirect fails the download.
    // TODO(b/553665882): Support redirects by validating each redirected
    // URL, e.g. via SimpleURLLoader::SetOnRedirectCallback().
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
    if (IsCredentialedHost(files_[i].url)) {
      // The host only serves a file to the user who uploaded it, so the request
      // carries the user's cookies. Making it same-site with the file's host
      // also sends SameSite cookies, as a visit to the URL would.
      resource_request->credentials_mode =
          network::mojom::CredentialsMode::kInclude;
      resource_request->site_for_cookies =
          net::SiteForCookies::FromUrl(files_[i].url);
    } else {
      resource_request->credentials_mode =
          network::mojom::CredentialsMode::kOmit;
    }

    std::unique_ptr<network::SimpleURLLoader> loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         kTrafficAnnotation);
    // Destroying the loader cancels the request and the callback is not run,
    // so `this` cannot be used after destruction.
    loader->DownloadToString(url_loader_factory.get(),
                             base::BindOnce(&FileUploadTool::OnDownloadComplete,
                                            weak_ptr_factory_.GetWeakPtr(), i),
                             max_file_size);
    loaders_.push_back(std::move(loader));
  }
}

void FileUploadTool::OnDownloadComplete(size_t index,
                                        std::optional<std::string> body) {
  CHECK_LT(index, downloaded_contents_.size());
  CHECK_LT(index, loaders_.size());
  CHECK_GT(pending_downloads_, 0u);

  const network::SimpleURLLoader& loader = *loaders_[index];

  if (body) {
    downloaded_contents_[index] = std::move(body);
  } else {
    download_failed_ = true;
    const network::mojom::URLResponseHead* response = loader.ResponseInfo();
    journal().Log(
        JournalURL(), task_id(), "FileUploadTool::DownloadFailed",
        JournalDetailsBuilder()
            .Add("file_index", static_cast<int>(index))
            .Add("credentialed", IsCredentialedHost(files_[index].url))
            .Add("net_error", net::ErrorToShortString(loader.NetError()))
            .Add("http_status", response && response->headers
                                    ? response->headers->response_code()
                                    : 0)
            .Build());
  }

  if (--pending_downloads_ > 0) {
    return;
  }

  loaders_.clear();
  OnAllDownloadsComplete();
}

void FileUploadTool::OnAllDownloadsComplete() {
  if (download_failed_) {
    FailInvoke(mojom::ActionResultCode::kFileUploadDownloadFailed,
               "Failed to download one or more files.");
    return;
  }

  std::vector<PendingFile> pending;
  pending.reserve(files_.size());
  for (size_t i = 0; i < files_.size(); ++i) {
    PendingFile file;
    file.display_name = DisplayNameForSource(files_[i]);
    CHECK(downloaded_contents_[i].has_value());
    file.contents = std::move(*downloaded_contents_[i]);
    pending.push_back(std::move(file));
  }
  downloaded_contents_.clear();

  StageFilesAndClick(std::move(pending));
}

void FileUploadTool::StageFilesAndClick(std::vector<PendingFile> files) {
  ActorSurface* const actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    FailInvoke(mojom::ActionResultCode::kTabWentAway, "Tab went away");
    return;
  }

  content::RenderFrameHost* const target_frame = FindTargetFrame(
      *actor_surface->GetWebContents(), actor_surface_handle_, target_);
  if (!target_frame) {
    FailInvoke(mojom::ActionResultCode::kFrameWentAway,
               "The frame holding the target went away.");
    return;
  }

  // The staged files only ever exist in browser process memory, so there is no
  // temporary file to clean up if the browser exits unexpectedly.
  const base::Time modification_time = base::Time::Now();
  std::vector<FilePickerInterceptor::StagedFile> staged_files;
  staged_files.reserve(files.size());
  for (PendingFile& file : files) {
    FilePickerInterceptor::StagedFile staged =
        storage::InMemoryFileRegistry::GetInstance()->Register(
            file.display_name, std::move(file.contents), modification_time);
    if (!staged.is_valid()) {
      FailInvoke(
          mojom::ActionResultCode::kFileUploadFileSystemRegistrationFailed,
          "Failed to expose the files to the renderer.");
      return;
    }
    staged_files.push_back(std::move(staged));
  }

  FilePickerInterceptor::Install(*target_frame, std::move(staged_files),
                                 kGlicActorFileUploadToolTimeout.Get());

  // Clicking the target is what makes the page open its file chooser, which
  // the interceptor installed above then answers.
  tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
      actor_surface_handle_, target_, mojom::ClickType::kLeft,
      mojom::ClickCount::kSingle));

  PostResponseTask(std::move(invoke_callback_),
                   MakeOkResult(/*requires_page_stabilization=*/false));
}

void FileUploadTool::FailInvoke(mojom::ActionResultCode code,
                                std::string_view message) {
  journal().Log(JournalURL(), task_id(), "FileUploadTool::Failed",
                JournalDetailsBuilder().AddError(message).Build());
  PostResponseTask(
      std::move(invoke_callback_),
      MakeResult(code, /*requires_page_stabilization=*/false, message));
}

void FileUploadTool::Cancel() {
  // Cancels any in-flight downloads.
  loaders_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (ActorSurface* const actor_surface = actor_surface_handle_.Get()) {
    if (content::WebContents* const contents =
            actor_surface->GetWebContents()) {
      FilePickerInterceptor::Remove(*contents);
    }
  }

  if (invoke_callback_) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kInvokeCanceled,
                                /*requires_page_stabilization=*/false));
  }
}

std::string FileUploadTool::DebugString() const {
  return base::StringPrintf("FileUploadTool(files_count=%zu)", files_.size());
}

GURL FileUploadTool::JournalURL() const {
  ActorSurface* const actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    return GURL::EmptyGURL();
  }
  return actor_surface->GetWebContents()->GetLastCommittedURL();
}

std::string FileUploadTool::JournalEvent() const {
  return "FileUpload";
}

std::unique_ptr<ObservationDelayController>
FileUploadTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  ActorSurface* const actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    return nullptr;
  }
  content::RenderFrameHost* const rfh =
      actor_surface->GetWebContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

void FileUploadTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                            ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle FileUploadTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
