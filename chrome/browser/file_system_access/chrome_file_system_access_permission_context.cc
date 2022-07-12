// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/installable/installable_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/file_system_access_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif

namespace features {
const base::Feature kFileSystemAccessPersistentPermissions{
    "kFileSystemAccessPersistentPermissions",
    base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features

namespace {

using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using blink::mojom::PermissionStatus;
using permissions::PermissionAction;

// This long after the last top-level tab or window for an origin is closed (or
// is navigated to another origin), all the permissions for that origin will be
// revoked.
constexpr base::TimeDelta kPermissionRevocationTimeout = base::Seconds(5);

// Interval at which to periodically sweep persisted permissions to revoke
// expired grants and renew those with corresponding active grants.
constexpr base::TimeDelta kPersistentPermissionSweepInterval = base::Hours(3);

// Dictionary keys for the FILE_SYSTEM_ACCESS_CHOOSER_DATA setting.
const char kPermissionPathKey[] = "path";
const char kPermissionIsDirectoryKey[] = "is-directory";
const char kPermissionWritableKey[] = "writable";
const char kPermissionReadableKey[] = "readable";
const char kPermissionLastUsedTimeKey[] = "time";

// Dictionary keys for the FILE_SYSTEM_LAST_PICKED_DIRECTORY website setting.
// Schema (per origin):
// {
//  ...
//   {
//     "default-id" : { "path" : <path> , "path-type" : <type>}
//     "custom-id-fruit" : { "path" : <path> , "path-type" : <type> }
//     "custom-id-flower" : { "path" : <path> , "path-type" : <type> }
//     ...
//   }
//  ...
// }
const char kDefaultLastPickedDirectoryKey[] = "default-id";
const char kCustomLastPickedDirectoryKey[] = "custom-id";
const char kPathKey[] = "path";
const char kPathTypeKey[] = "path-type";
const char kTimestampKey[] = "timestamp";

void ShowFileSystemAccessRestrictedDirectoryDialogOnUIThread(
    content::GlobalRenderFrameHostId frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    base::OnceCallback<
        void(ChromeFileSystemAccessPermissionContext::SensitiveDirectoryResult)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
  if (!rfh || !rfh->IsActive()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(ChromeFileSystemAccessPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(ChromeFileSystemAccessPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  ShowFileSystemAccessRestrictedDirectoryDialog(
      origin, path, handle_type, std::move(callback), web_contents);
}

// Sentinel used to indicate that no PathService key is specified for a path in
// the struct below.
constexpr const int kNoBasePathKey = -1;

enum BlockType {
  kBlockAllChildren,
  kBlockNestedDirectories,
  kDontBlockChildren
};

const struct {
  // base::BasePathKey value (or one of the platform specific extensions to it)
  // for a path that should be blocked. Specify kNoBasePathKey if |path| should
  // be used instead.
  int base_path_key;

  // Explicit path to block instead of using |base_path_key|. Set to nullptr to
  // use |base_path_key| on its own. If both |base_path_key| and |path| are set,
  // |path| is treated relative to the path |base_path_key| resolves to.
  const base::FilePath::CharType* path;

  // If this is set to kDontBlockChildren, only the given path and its parents
  // are blocked. If this is set to kBlockAllChildren, all children of the given
  // path are blocked as well. Finally if this is set to kBlockNestedDirectories
  // access is allowed to individual files in the directory, but nested
  // directories are still blocked.
  // The BlockType of the nearest ancestor of a path to check is what ultimately
  // determines if a path is blocked or not. If a blocked path is a descendent
  // of another blocked path, then it may override the child-blocking policy of
  // its ancestor. For example, if /home blocks all children, but
  // /home/downloads does not, then /home/downloads/file.ext will *not* be
  // blocked.
  BlockType type;
} kBlockedPaths[] = {
    // Don't allow users to share their entire home directory, entire desktop or
    // entire documents folder, but do allow sharing anything inside those
    // directories not otherwise blocked.
    {base::DIR_HOME, nullptr, kDontBlockChildren},
    {base::DIR_USER_DESKTOP, nullptr, kDontBlockChildren},
    {chrome::DIR_USER_DOCUMENTS, nullptr, kDontBlockChildren},
    // Similar restrictions for the downloads directory.
    {chrome::DIR_DEFAULT_DOWNLOADS, nullptr, kDontBlockChildren},
    {chrome::DIR_DEFAULT_DOWNLOADS_SAFE, nullptr, kDontBlockChildren},
    // The Chrome installation itself should not be modified by the web.
    {base::DIR_EXE, nullptr, kBlockAllChildren},
#if !BUILDFLAG(IS_FUCHSIA)
    {base::DIR_MODULE, nullptr, kBlockAllChildren},
#endif
    {base::DIR_ASSETS, nullptr, kBlockAllChildren},
    // And neither should the configuration of at least the currently running
    // Chrome instance (note that this does not take --user-data-dir command
    // line overrides into account).
    {chrome::DIR_USER_DATA, nullptr, kBlockAllChildren},
    // ~/.ssh is pretty sensitive on all platforms, so block access to that.
    {base::DIR_HOME, FILE_PATH_LITERAL(".ssh"), kBlockAllChildren},
    // And limit access to ~/.gnupg as well.
    {base::DIR_HOME, FILE_PATH_LITERAL(".gnupg"), kBlockAllChildren},
#if BUILDFLAG(IS_WIN)
    // Some Windows specific directories to block, basically all apps, the
    // operating system itself, as well as configuration data for apps.
    {base::DIR_PROGRAM_FILES, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILESX86, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILES6432, nullptr, kBlockAllChildren},
    {base::DIR_WINDOWS, nullptr, kBlockAllChildren},
    {base::DIR_ROAMING_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_LOCAL_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_COMMON_APP_DATA, nullptr, kBlockAllChildren},
    // Opening a file from an MTP device, such as a smartphone or a camera, is
    // implemented by Windows as opening a file in the temporary internet files
    // directory. To support that, allow opening files in that directory, but
    // not whole directories.
    {base::DIR_IE_INTERNET_CACHE, nullptr, kBlockNestedDirectories},
#endif
#if BUILDFLAG(IS_MAC)
    // Similar Mac specific blocks.
    {base::DIR_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_HOME, FILE_PATH_LITERAL("Library"), kBlockAllChildren},
    // Allow access to iCloud files.
    {base::DIR_HOME, FILE_PATH_LITERAL("Library/Mobile Documents"),
     kDontBlockChildren},
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // On Linux also block access to devices via /dev, as well as security
    // sensitive data in /sys and /proc.
    {kNoBasePathKey, FILE_PATH_LITERAL("/dev"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/sys"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/proc"), kBlockAllChildren},
    // And block all of ~/.config, matching the similar restrictions on mac
    // and windows.
    {base::DIR_HOME, FILE_PATH_LITERAL(".config"), kBlockAllChildren},
    // Block ~/.dbus as well, just in case, although there probably isn't much a
    // website can do with access to that directory and its contents.
    {base::DIR_HOME, FILE_PATH_LITERAL(".dbus"), kBlockAllChildren},
#endif
    // TODO(https://crbug.com/984641): Refine this list, for example add
    // XDG_CONFIG_HOME when it is not set ~/.config?
};

bool ShouldBlockAccessToPath(const base::FilePath& check_path,
                             HandleType handle_type) {
  DCHECK(!check_path.empty());
  DCHECK(check_path.IsAbsolute());

  base::FilePath nearest_ancestor;
  int nearest_ancestor_path_key = kNoBasePathKey;
  BlockType nearest_ancestor_block_type = kDontBlockChildren;
  for (const auto& block : kBlockedPaths) {
    base::FilePath blocked_path;
    if (block.base_path_key != kNoBasePathKey) {
      if (!base::PathService::Get(block.base_path_key, &blocked_path))
        continue;
      if (block.path)
        blocked_path = blocked_path.Append(block.path);
    } else {
      DCHECK(block.path);
      blocked_path = base::FilePath(block.path);
    }

    if (check_path == blocked_path || check_path.IsParent(blocked_path)) {
      VLOG(1) << "Blocking access to " << check_path
              << " because it is a parent of " << blocked_path << " ("
              << block.base_path_key << ")";
      return true;
    }

    if (blocked_path.IsParent(check_path) &&
        (nearest_ancestor.empty() || nearest_ancestor.IsParent(blocked_path))) {
      nearest_ancestor = blocked_path;
      nearest_ancestor_path_key = block.base_path_key;
      nearest_ancestor_block_type = block.type;
    }
  }

  // The path we're checking is not in a potentially blocked directory, or the
  // nearest ancestor does not block access to its children. Grant access.
  if (nearest_ancestor.empty() ||
      nearest_ancestor_block_type == kDontBlockChildren) {
    return false;
  }

  // The path we're checking is a file, and the nearest ancestor only blocks
  // access to directories. Grant access.
  if (handle_type == HandleType::kFile &&
      nearest_ancestor_block_type == kBlockNestedDirectories) {
    return false;
  }

  // The nearest ancestor blocks access to its children, so block access.
  VLOG(1) << "Blocking access to " << check_path << " because it is inside "
          << nearest_ancestor << " (" << nearest_ancestor_path_key << ")";
  return true;
}

// Returns a callback that calls the passed in |callback| by posting a task to
// the current sequenced task runner.
template <typename... ResultTypes>
base::OnceCallback<void(ResultTypes... results)>
BindResultCallbackToCurrentSequence(
    base::OnceCallback<void(ResultTypes... results)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(ResultTypes... results)> callback,
         ResultTypes... results) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), results...));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

void DoSafeBrowsingCheckOnUIThread(
    content::GlobalRenderFrameHostId frame_id,
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    safe_browsing::CheckDownloadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Download Protection Service is not supported on Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service || !sb_service->download_protection_service() ||
      !sb_service->download_protection_service()->enabled()) {
    std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
    return;
  }

  if (!item->browser_context) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(frame_id.child_id);
    if (!rph) {
      std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
      return;
    }
    item->browser_context = rph->GetBrowserContext();
  }

  if (!item->web_contents) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (rfh) {
      DCHECK_NE(rfh->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kPrerendering);
      item->web_contents = content::WebContents::FromRenderFrameHost(rfh);
    }
  }

  sb_service->download_protection_service()->CheckFileSystemAccessWrite(
      std::move(item), std::move(callback));
#endif
}

ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult
InterpretSafeBrowsingResult(safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    // Only allow downloads that are marked as SAFE or UNKNOWN by SafeBrowsing.
    // All other types are going to be blocked. UNKNOWN could be the result of a
    // failed safe browsing ping.
    case Result::UNKNOWN:
    case Result::SAFE:
    case Result::ALLOWLISTED_BY_POLICY:
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kAllow;

    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::BLOCKED_UNSUPPORTED_FILE_TYPE:
    case Result::DANGEROUS_ACCOUNT_COMPROMISE:
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kBlock;

    // This shouldn't be returned for File System Access write checks.
    case Result::ASYNC_SCANNING:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
      NOTREACHED();
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kAllow;
  }
  NOTREACHED();
  return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::kBlock;
}

std::string GenerateLastPickedDirectoryKey(const std::string& id) {
  return id.empty() ? kDefaultLastPickedDirectoryKey
                    : base::StrCat({kCustomLastPickedDirectoryKey, "-", id});
}

base::StringPiece PathAsPermissionKey(const base::FilePath& path) {
  return base::StringPiece(
      reinterpret_cast<const char*>(path.value().data()),
      path.value().size() * sizeof(base::FilePath::CharType));
}

base::StringPiece GetGrantKeyFromGrantType(GrantType type) {
  return type == GrantType::kWrite ? kPermissionWritableKey
                                   : kPermissionReadableKey;
}

}  // namespace

ChromeFileSystemAccessPermissionContext::Grants::Grants() = default;
ChromeFileSystemAccessPermissionContext::Grants::~Grants() = default;
ChromeFileSystemAccessPermissionContext::Grants::Grants(Grants&&) = default;
ChromeFileSystemAccessPermissionContext::Grants&
ChromeFileSystemAccessPermissionContext::Grants::operator=(Grants&&) = default;

class ChromeFileSystemAccessPermissionContext::PermissionGrantImpl
    : public content::FileSystemAccessPermissionGrant {
 public:
  PermissionGrantImpl(
      base::WeakPtr<ChromeFileSystemAccessPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      GrantType type)
      : context_(std::move(context)),
        origin_(origin),
        path_(path),
        handle_type_(handle_type),
        type_(type) {}

  // FileSystemAccessPermissionGrant:
  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }
  base::FilePath GetPath() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  void RequestPermission(
      content::GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Check if a permission request has already been processed previously. This
    // check is done first because we don't want to reset the status of a
    // permission if it has already been granted.
    if (GetStatus() != PermissionStatus::ASK || !context_) {
      if (GetStatus() == PermissionStatus::GRANTED) {
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
      }
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    if (HasPersistedPermission(MetricsOptions::kRecord)) {
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback),
          PermissionRequestOutcome::kGrantedByPersistentPermission);
      return;
    }

    if (AncestorHasPersistedPermission()) {
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback),
          PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
      return;
    }

    if (type_ == GrantType::kWrite) {
      ContentSetting content_setting =
          context_->GetWriteGuardContentSetting(origin_);

      // Content setting grants write permission without asking.
      if (content_setting == CONTENT_SETTING_ALLOW) {
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kGrantedByContentSetting);
        return;
      }

      // Content setting blocks write permission.
      if (content_setting == CONTENT_SETTING_BLOCK) {
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kBlockedByContentSetting);
        return;
      }
    }

    // Otherwise, perform checks and ask the user for permission.

    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (!rfh) {
      // Requested from a no longer valid render frame host.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    // Don't show request permission UI for an inactive RenderFrameHost as the
    // page might not distinguish properly between user denying the permission
    // and automatic rejection, leading to an inconsistent UX once the page
    // becomes active again.
    // - If this is called when RenderFrameHost is in BackForwardCache, evict
    //   the document from the cache.
    // - If this is called when RenderFrameHost is in prerendering, cancel
    //   prerendering.
    if (rfh->IsInactiveAndDisallowActivation(
            content::DisallowActivationReasonId::
                kFileSystemAccessPermissionRequest)) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }
    // We don't allow file system access from fenced frames.
    if (rfh->IsNestedWithinFencedFrame()) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    if (user_activation_state == UserActivationState::kRequired &&
        !rfh->HasTransientUserActivation()) {
      // No permission prompts without user activation.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kNoUserActivation);
      return;
    }

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents) {
      // Requested from a worker, or a no longer existing tab.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    url::Origin embedding_origin = url::Origin::Create(
        permissions::PermissionUtil::GetLastCommittedOriginAsURL(
            rfh->GetMainFrame()));
    if (embedding_origin != origin_) {
      // Third party iframes are not allowed to request more permissions.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kThirdPartyContext);
      return;
    }

    auto* request_manager =
        FileSystemAccessPermissionRequestManager::FromWebContents(web_contents);
    if (!request_manager) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // Drop fullscreen mode so that the user sees the URL bar.
    base::ScopedClosureRunner fullscreen_block =
        web_contents->ForSecurityDropFullscreen();

    FileSystemAccessPermissionRequestManager::Access access =
        type_ == GrantType::kRead
            ? FileSystemAccessPermissionRequestManager::Access::kRead
            : FileSystemAccessPermissionRequestManager::Access::kWrite;

    // If a website wants both read and write access, code in content will
    // request those as two separate requests. The |request_manager| will then
    // detect this and combine the two requests into one prompt. As such this
    // code does not have to have any way to request Access::kReadWrite.

    request_manager->AddRequest(
        {origin_, path_, handle_type_, access},
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestResult, this,
                       std::move(callback)),
        std::move(fullscreen_block));
  }

  bool HasPersistedPermission(MetricsOptions options) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return context_->HasPersistedPermission(origin_, path_, handle_type_, type_,
                                            options);
  }

  bool AncestorHasPersistedPermission() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (base::FilePath parent = path_.DirName(); parent != parent.DirName();
         parent = parent.DirName()) {
      if (context_->HasPersistedPermission(origin_, parent,
                                           HandleType::kDirectory, type_,
                                           MetricsOptions::kDoNotRecord)) {
        return true;
      }
    }
    return false;
  }

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }

  HandleType handle_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return handle_type_;
  }

  const base::FilePath& path() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  GrantType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void SetStatus(PermissionStatus status,
                 PersistedPermissionOptions persisted_status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool should_notify = status_ != status;
    status_ = status;

    if (context_ &&
        persisted_status ==
            PersistedPermissionOptions::kUpdatePersistedPermission &&
        base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      // PersistedPermissionOptions only applies to this grant's type. If the
      // other type is persisted, only revoke permission to this grant's type.
      base::Value grant = AsValue();
      if (grant.FindBoolKey(kPermissionWritableKey).value_or(false) ||
          grant.FindBoolKey(kPermissionReadableKey).value_or(false)) {
        context_->GrantObjectPermission(origin_, std::move(grant));
      } else {
        context_->RevokeObjectPermission(origin_, GetKey());
      }
    }
    if (should_notify)
      NotifyPermissionStatusChanged();
  }

  static void CollectGrants(
      const std::map<base::FilePath, PermissionGrantImpl*>& grants,
      std::vector<base::FilePath>* directory_grants,
      std::vector<base::FilePath>* file_grants) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (const auto& entry : grants) {
      if (entry.second->GetStatus() != PermissionStatus::GRANTED)
        continue;
      if (entry.second->handle_type() == HandleType::kDirectory) {
        directory_grants->push_back(entry.second->path());
      } else {
        file_grants->push_back(entry.second->path());
      }
    }
  }

 protected:
  ~PermissionGrantImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (context_)
      context_->PermissionGrantDestroyed(this);
  }

 private:
  void OnPermissionRequestResult(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionAction result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (result) {
      case PermissionAction::GRANTED:
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserGranted);
        if (context_)
          context_->ScheduleUsageIconUpdate();
        break;
      case PermissionAction::DENIED:
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDenied);
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::GRANTED_ONCE:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }
  }

  void RunCallbackAndRecordPermissionRequestOutcome(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionRequestOutcome outcome) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (type_ == GrantType::kWrite) {
      base::UmaHistogramEnumeration(
          "Storage.FileSystemAccess.WritePermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.WritePermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.WritePermissionRequestOutcome.File",
            outcome);
      }
    } else {
      base::UmaHistogramEnumeration(
          "Storage.FileSystemAccess.ReadPermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.ReadPermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.ReadPermissionRequestOutcome.File",
            outcome);
      }
    }

    std::move(callback).Run(outcome);
  }

  base::StringPiece GetKey() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return PathAsPermissionKey(path_);
  }
  base::Value AsValue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetKey(kPermissionPathKey, base::FilePathToValue(path_));
    value.SetBoolKey(kPermissionIsDirectoryKey,
                     handle_type_ == HandleType::kDirectory);
    value.SetBoolKey(GetGrantKeyFromGrantType(type_),
                     status_ == PermissionStatus::GRANTED);
    // Persisted permissions include both read and write information in one
    // object. Figure out if the other grant type is already persisted.
    auto opposite_type =
        type_ == GrantType::kRead ? GrantType::kWrite : GrantType::kRead;
    if (context_->HasPersistedPermission(origin_, path_, handle_type_,
                                         opposite_type,
                                         MetricsOptions::kDoNotRecord)) {
      value.SetBoolKey(GetGrantKeyFromGrantType(opposite_type), true);
    }
    value.SetKey(kPermissionLastUsedTimeKey,
                 base::TimeToValue(context_->clock_->Now()));
    return value;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> const context_;
  const url::Origin origin_;
  const base::FilePath path_;
  const HandleType handle_type_;
  const GrantType type_;
  base::Time last_used_time_;

  // This member should only be updated via SetStatus(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;
};

struct ChromeFileSystemAccessPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  std::map<base::FilePath, PermissionGrantImpl*> read_grants;
  std::map<base::FilePath, PermissionGrantImpl*> write_grants;

  // Timer that is triggered whenever the user navigates away from this origin.
  // This is used to give a website a little bit of time for background work
  // before revoking all permissions for the origin.
  std::unique_ptr<base::RetainingOneShotTimer> cleanup_timer;
};

constexpr base::TimeDelta ChromeFileSystemAccessPermissionContext::
    kPersistentPermissionExpirationTimeoutDefault;
constexpr base::TimeDelta ChromeFileSystemAccessPermissionContext::
    kPersistentPermissionExpirationTimeoutExtended;
constexpr base::TimeDelta
    ChromeFileSystemAccessPermissionContext::kPersistentPermissionGracePeriod;

ChromeFileSystemAccessPermissionContext::
    ChromeFileSystemAccessPermissionContext(content::BrowserContext* context,
                                            const base::Clock* clock)
    : ObjectPermissionContextBase(
          ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
          ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
          HostContentSettingsMapFactory::GetForProfile(context)),
      profile_(context),
      clock_(clock) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  content_settings_ = base::WrapRefCounted(
      HostContentSettingsMapFactory::GetForProfile(profile_));

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Revoke expired persisted permissions.
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ChromeFileSystemAccessPermissionContext::
                                      UpdatePersistedPermissions,
                                  weak_factory_.GetWeakPtr()));
    // Periodically sweep persisted permissions to revoke expired
    // permissions and renew those with corresponding active grants.
    periodic_sweep_persisted_permissions_timer_.SetTaskRunner(
        content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT}));
    periodic_sweep_persisted_permissions_timer_.Start(
        FROM_HERE, kPersistentPermissionSweepInterval, this,
        &ChromeFileSystemAccessPermissionContext::UpdatePersistedPermissions);
  }
}

ChromeFileSystemAccessPermissionContext::
    ~ChromeFileSystemAccessPermissionContext() = default;

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  auto*& existing_grant = origin_state.read_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type,
        GrantType::kRead);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetReadGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      existing_grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already readable this new grant should also be
      // readable.
      if (new_grant &&
          AncestorHasActivePermission(origin, path, GrantType::kRead)) {
        existing_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kUpdatePersistedPermission);
        break;
      }
      switch (user_action) {
        case UserAction::kOpen:
        case UserAction::kSave:
          // Open and Save dialog only grant read access for individual files.
          if (handle_type == HandleType::kDirectory)
            break;
          [[fallthrough]];
        case UserAction::kDragAndDrop:
          // Drag&drop grants read access for all handles.
          existing_grant->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
          break;
        case UserAction::kLoadFromStorage:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      // Don't bother revoking persisted permissions. If the permissions have
      // not yet expired when the ContentSettingValue is changed, they will
      // effectively be reinstated.
      if (new_grant) {
        existing_grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  if (existing_grant->GetStatus() == PermissionStatus::GRANTED)
    ScheduleUsageIconUpdate();

  return existing_grant;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  auto*& existing_grant = origin_state.write_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type,
        GrantType::kWrite);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetWriteGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      existing_grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already writable this new grant should also be
      // writable.
      if (new_grant &&
          AncestorHasActivePermission(origin, path, GrantType::kWrite)) {
        existing_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kUpdatePersistedPermission);
        break;
      }
      switch (user_action) {
        case UserAction::kSave:
          // Only automatically grant write access for save dialogs.
          existing_grant->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
          break;
        case UserAction::kOpen:
        case UserAction::kDragAndDrop:
        case UserAction::kLoadFromStorage:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      // Don't bother revoking persisted permissions. If the permissions have
      // not yet expired when the ContentSettingValue is changed, they will
      // effectively be reinstated.
      if (new_grant) {
        existing_grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  if (existing_grant->GetStatus() == PermissionStatus::GRANTED)
    ScheduleUsageIconUpdate();

  return existing_grant;
}

// Returns non-expired persisted permissions. Active grants are ignored here,
// since persisted permissions are almost always a superset of active grants.
// The exception is when CONTENT_SETTING_ALLOW for an origin, which is only
// possible for System Web Apps.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetGrantedObjects(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetGrantedObjects(origin);

  bool has_extended_permissions = OriginHasExtendedPermissions(origin);
  // Filter out expired permissions.
  objects.erase(
      base::ranges::remove_if(
          objects,
          [this,
           &has_extended_permissions](const std::unique_ptr<Object>& object) {
            auto last_activity_time =
                base::ValueToTime(
                    object->value.FindKey(kPermissionLastUsedTimeKey))
                    .value_or(base::Time::Min());
            return this->PersistentPermissionIsExpired(
                last_activity_time, has_extended_permissions);
          }),
      objects.end());
  return objects;
}

// Returns non-expired persisted permissions. Active grants are ignored here,
// since persisted permissions are almost always a superset of active grants.
// The exception is when CONTENT_SETTING_ALLOW for an origin, which is only
// possible for System Web Apps.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetAllGrantedObjects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<Object>> objects =
      GetAllGrantedOrExpiredObjects();

  url::Origin origin;
  GURL origin_as_url;
  bool has_extended_permissions = false;
  // Filter out expired permissions.
  // Checking whether an origin has an installed PWA may be expensive.
  // GetAllGrantedObjects() returns objects grouped by origin, so this should
  // only check once per origin.
  objects.erase(base::ranges::remove_if(
                    objects,
                    [this, &has_extended_permissions, &origin,
                     &origin_as_url](const std::unique_ptr<Object>& object) {
                      if (object->origin != origin_as_url) {
                        origin_as_url = object->origin;
                        origin = url::Origin::Create(object->origin);
                        has_extended_permissions =
                            OriginHasExtendedPermissions(origin);
                      }
                      auto last_activity_time =
                          base::ValueToTime(
                              object->value.FindKey(kPermissionLastUsedTimeKey))
                              .value_or(base::Time::Min());
                      return this->PersistentPermissionIsExpired(
                          last_activity_time, has_extended_permissions);
                    }),
                objects.end());

  return objects;
}

std::string ChromeFileSystemAccessPermissionContext::GetKeyForObject(
    const base::Value& object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto optional_path =
      base::ValueToFilePath(object.FindKey(kPermissionPathKey));
  DCHECK(optional_path);
  return std::string(PathAsPermissionKey(optional_path.value()));
}

bool ChromeFileSystemAccessPermissionContext::IsValidObject(
    const base::Value& object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // At least one of the readable/writable keys needs to be set.
  if (!object.is_dict() || (object.DictSize() != 4 && object.DictSize() != 5) ||
      !object.FindKey(kPermissionPathKey) ||
      !object.FindBoolKey(kPermissionIsDirectoryKey) ||
      (!object.FindBoolKey(kPermissionWritableKey) &&
       !object.FindBoolKey(kPermissionReadableKey)) ||
      !object.FindKey(kPermissionLastUsedTimeKey)) {
    return false;
  }
  return true;
}

std::u16string ChromeFileSystemAccessPermissionContext::GetObjectDisplayName(
    const base::Value& object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto optional_path =
      base::ValueToFilePath(object.FindKey(kPermissionPathKey));
  DCHECK(optional_path);
  return optional_path->LossyDisplayName();
}

ContentSetting
ChromeFileSystemAccessPermissionContext::GetWriteGuardContentSetting(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
}

ContentSetting
ChromeFileSystemAccessPermissionContext::GetReadGuardContentSetting(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
}

bool ChromeFileSystemAccessPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetReadGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetReadGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

bool ChromeFileSystemAccessPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

void ChromeFileSystemAccessPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1009970): Figure out what external paths should be
  // blocked. We could resolve the external path to a local path, and check for
  // blocked directories based on that, but that doesn't work well. Instead we
  // should have a separate Chrome OS only code path to block for example the
  // root of certain external file systems.
  if (path_type == PathType::kExternal) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, path, handle_type),
      base::BindOnce(&ChromeFileSystemAccessPermissionContext::
                         DidConfirmSensitiveDirectoryAccess,
                     GetWeakPtr(), origin, path, handle_type, frame_id,
                     std::move(callback)));
}

void ChromeFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DoSafeBrowsingCheckOnUIThread, frame_id, std::move(item),
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> task_runner,
                 base::OnceCallback<void(AfterWriteCheckResult result)>
                     callback,
                 safe_browsing::DownloadCheckResult result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback),
                                   InterpretSafeBrowsingResult(result)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ChromeFileSystemAccessPermissionContext::
    DidConfirmSensitiveDirectoryAccess(
        const url::Origin& origin,
        const base::FilePath& path,
        HandleType handle_type,
        content::GlobalRenderFrameHostId frame_id,
        base::OnceCallback<void(SensitiveDirectoryResult)> callback,
        bool should_block) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!should_block) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }

  auto result_callback =
      BindResultCallbackToCurrentSequence(std::move(callback));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ShowFileSystemAccessRestrictedDirectoryDialogOnUIThread,
                     frame_id, origin, path, handle_type,
                     std::move(result_callback)));
}

void ChromeFileSystemAccessPermissionContext::MaybeEvictEntries(
    base::Value& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!value.is_dict()) {
    value = base::Value(base::Value::Type::DICTIONARY);
    return;
  }

  std::vector<std::pair<base::Time, std::string>> entries;
  entries.reserve(value.DictSize());
  for (auto entry : value.DictItems()) {
    // Don't evict the default ID.
    if (entry.first == kDefaultLastPickedDirectoryKey)
      continue;
    // If the data is corrupted and `entry.second` is for some reason not a
    // dict, it should be first in line for eviction.
    auto timestamp = base::Time::Min();
    if (entry.second.is_dict()) {
      timestamp = base::ValueToTime(entry.second.FindKey(kTimestampKey))
                      .value_or(base::Time::Min());
    }
    entries.emplace_back(timestamp, entry.first);
  }

  if (entries.size() <= max_ids_per_origin_)
    return;

  base::ranges::sort(entries);
  size_t entries_to_remove = entries.size() - max_ids_per_origin_;
  for (size_t i = 0; i < entries_to_remove; ++i) {
    bool did_remove_entry = value.RemoveKey(entries[i].second);
    DCHECK(did_remove_entry);
  }
}

void ChromeFileSystemAccessPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id,
    const base::FilePath& path,
    const PathType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      /*info=*/nullptr);
  if (!value.is_dict())
    value = base::Value(base::Value::Type::DICTIONARY);

  // Create an entry into the nested dictionary.
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetKey(kPathKey, base::FilePathToValue(path));
  entry.SetIntKey(kPathTypeKey, static_cast<int>(type));
  entry.SetKey(kTimestampKey, base::TimeToValue(clock_->Now()));

  value.SetKey(GenerateLastPickedDirectoryKey(id), std::move(entry));

  MaybeEvictEntries(value);

  content_settings_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, std::move(value));
}

ChromeFileSystemAccessPermissionContext::PathInfo
ChromeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      /*info=*/nullptr);

  PathInfo path_info;
  if (!value.is_dict())
    return path_info;

  auto* entry = value.FindDictKey(GenerateLastPickedDirectoryKey(id));
  if (!entry)
    return path_info;

  auto type_int = entry->FindIntKey(kPathTypeKey)
                      .value_or(static_cast<int>(PathType::kLocal));
  path_info.type = type_int == static_cast<int>(PathType::kExternal)
                       ? PathType::kExternal
                       : PathType::kLocal;
  path_info.path = base::ValueToFilePath(entry->FindKey(kPathKey))
                       .value_or(base::FilePath());
  return path_info;
}

base::FilePath
ChromeFileSystemAccessPermissionContext::GetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int key = base::PATH_START;
  switch (directory) {
    case blink::mojom::WellKnownDirectory::kDefault:
      key = chrome::DIR_USER_DOCUMENTS;
      break;
    case blink::mojom::WellKnownDirectory::kDirDesktop:
      key = base::DIR_USER_DESKTOP;
      break;
    case blink::mojom::WellKnownDirectory::kDirDocuments:
      key = chrome::DIR_USER_DOCUMENTS;
      break;
    case blink::mojom::WellKnownDirectory::kDirDownloads:
      key = chrome::DIR_DEFAULT_DOWNLOADS;
      break;
    case blink::mojom::WellKnownDirectory::kDirMusic:
      key = chrome::DIR_USER_MUSIC;
      break;
    case blink::mojom::WellKnownDirectory::kDirPictures:
      key = chrome::DIR_USER_PICTURES;
      break;
    case blink::mojom::WellKnownDirectory::kDirVideos:
      key = chrome::DIR_USER_VIDEOS;
      break;
  }
  base::FilePath directory_path;
  base::PathService::Get(key, &directory_path);
  return directory_path;
}

std::u16string ChromeFileSystemAccessPermissionContext::GetPickerTitle(
    const blink::mojom::FilePickerOptionsPtr& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(asully): Consider adding custom strings for invocations of the file
  // picker, as well. Returning the empty string will fall back to the platform
  // default for the given picker type.
  std::u16string title;
  if (options->is_directory_picker_options()) {
    title = l10n_util::GetStringUTF16(
        options->get_directory_picker_options()->request_writable
            ? IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_WRITABLE_DIRECTORY_TITLE
            : IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_READABLE_DIRECTORY_TITLE);
  }
  return title;
}

ChromeFileSystemAccessPermissionContext::Grants
ChromeFileSystemAccessPermissionContext::GetPermissionGrants(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return {};

  Grants grants;
  PermissionGrantImpl::CollectGrants(it->second.read_grants,
                                     &grants.directory_read_grants,
                                     &grants.file_read_grants);
  PermissionGrantImpl::CollectGrants(it->second.write_grants,
                                     &grants.directory_write_grants,
                                     &grants.file_write_grants);
  return grants;
}

void ChromeFileSystemAccessPermissionContext::RevokeGrants(
    const url::Origin& origin,
    PersistedPermissionOptions persisted_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto origin_it = origins_.find(origin);
  if (origin_it == origins_.end())
    return;

  OriginState& origin_state = origin_it->second;
  for (auto& grant : origin_state.read_grants)
    grant.second->SetStatus(PermissionStatus::ASK, persisted_status);
  for (auto& grant : origin_state.write_grants)
    grant.second->SetStatus(PermissionStatus::ASK, persisted_status);
  ScheduleUsageIconUpdate();
}

bool ChromeFileSystemAccessPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.read_grants.empty())
    return false;
  for (const auto& grant : it->second.read_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

bool ChromeFileSystemAccessPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.write_grants.empty())
    return false;
  for (const auto& grant : it->second.write_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

void ChromeFileSystemAccessPermissionContext::NavigatedAwayFromOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == origins_.end())
    return;

  // Start a timer to possibly clean up permissions for this origin.
  if (!it->second.cleanup_timer) {
    it->second.cleanup_timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE, kPermissionRevocationTimeout,
        base::BindRepeating(&ChromeFileSystemAccessPermissionContext::
                                MaybeCleanupActivePermissions,
                            base::Unretained(this), origin));
  }
  it->second.cleanup_timer->Reset();
}

void ChromeFileSystemAccessPermissionContext::TriggerTimersForTesting() {
  for (const auto& it : origins_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (it.second.cleanup_timer) {
      auto task = it.second.cleanup_timer->user_task();
      it.second.cleanup_timer->Stop();
      task.Run();
    }
  }
}

void ChromeFileSystemAccessPermissionContext::MaybeCleanupActivePermissions(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == origins_.end())
    return;

#if !BUILDFLAG(IS_ANDROID)
  // Iterate over all top-level frames by iterating over all browsers, and all
  // tabs within those browsers. This also counts PWAs in windows without
  // tab strips, as those are still implemented as a Browser with a single tab.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile())
      continue;
    TabStripModel* tabs = browser->tab_strip_model();
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* web_contents = tabs->GetWebContentsAt(i);
      url::Origin tab_origin = url::Origin::Create(
          permissions::PermissionUtil::GetLastCommittedOriginAsURL(
              web_contents->GetPrimaryMainFrame()));
      // Found a tab for this origin, so early exit and don't revoke grants.
      if (tab_origin == origin)
        return;
    }
  }

  // No tabs found with the same origin, so renew persisted permissions before
  // revoking all active permissions for the origin.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    UpdatePersistedPermissionsForOrigin(origin);
  }
  RevokeGrants(origin,
               PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
#endif
}

bool ChromeFileSystemAccessPermissionContext::AncestorHasActivePermission(
    const url::Origin& origin,
    const base::FilePath& path,
    GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  const auto& relevant_grants = grant_type == GrantType::kWrite
                                    ? it->second.write_grants
                                    : it->second.read_grants;
  if (relevant_grants.empty())
    return false;

  // Permissions are inherited from the closest ancestor.
  for (base::FilePath parent = path.DirName(); parent != parent.DirName();
       parent = parent.DirName()) {
    auto i = relevant_grants.find(parent);
    if (i != relevant_grants.end() && i->second &&
        i->second->GetStatus() == PermissionStatus::GRANTED) {
      return true;
    }
  }
  return false;
}

// Origins with actively installed PWAs will have permissions persisted for
// longer than sites without an installed PWA or with a passively installed
// PWA.
bool ChromeFileSystemAccessPermissionContext::OriginHasExtendedPermissions(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_ANDROID)
  // The File System Access API is not supported on Android (see
  // crbug.com/1011535). If this ever changes, we'll need to revist this.
  return false;
#else
  DCHECK(profile());
  auto* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(static_cast<Profile*>(profile()));
  if (!web_app_provider)
    return false;

  auto app_id =
      web_app_provider->registrar().FindAppWithUrlInScope(origin.GetURL());
  return app_id.has_value() &&
         web_app_provider->registrar().IsActivelyInstalled(app_id.value());
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeFileSystemAccessPermissionContext::
    UpdatePersistedPermissionsForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdatePersistedPermissions();
}

void ChromeFileSystemAccessPermissionContext::UpdatePersistedPermissions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Storage.FileSystemAccess.PersistedPermissions.SweepTime.All");
  url::Origin origin;
  GURL origin_as_url;
  bool has_extended_permissions = false;
  auto objects = GetAllGrantedOrExpiredObjects();
  for (const auto& object : objects) {
    // Checking whether an origin has an installed PWA may be expensive.
    // GetAllGrantedObjects() returns objects grouped by origin, so this should
    // only check once per origin.
    if (object->origin != origin_as_url) {
      origin_as_url = object->origin;
      origin = url::Origin::Create(object->origin);
      has_extended_permissions = OriginHasExtendedPermissions(origin);
    }
    MaybeRenewOrRevokePersistedPermission(origin, std::move(object->value),
                                          has_extended_permissions);
  }
  base::UmaHistogramCounts1000(
      "Storage.FileSystemAccess.PersistedPermissions.Count", objects.size());
}

void ChromeFileSystemAccessPermissionContext::
    UpdatePersistedPermissionsForOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Storage.FileSystemAccess.PersistedPermissions.SweepTime.Origin");
  bool has_extended_permissions = OriginHasExtendedPermissions(origin);
  // Call the base class's version of this method, since this class overrides
  // this method to filter out expired grants.
  for (const auto& object :
       ObjectPermissionContextBase::GetGrantedObjects(origin)) {
    MaybeRenewOrRevokePersistedPermission(origin, std::move(object->value),
                                          has_extended_permissions);
  }
}

void ChromeFileSystemAccessPermissionContext::
    MaybeRenewOrRevokePersistedPermission(const url::Origin& origin,
                                          base::Value value,
                                          bool has_extended_permissions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  // Look for active read or write grants.
  bool found = false;
  if (it != origins_.end()) {
    base::FilePath path =
        base::ValueToFilePath(value.FindKey(kPermissionPathKey)).value();
    HandleType handle_type =
        value.FindBoolKey(kPermissionIsDirectoryKey).value()
            ? HandleType::kDirectory
            : HandleType::kFile;

    const OriginState& origin_state = it->second;
    if (value.FindBoolKey(kPermissionReadableKey).value_or(false)) {
      found = base::ranges::any_of(
          origin_state.read_grants, [&path, &handle_type](auto& grant) {
            return grant.first == path &&
                   grant.second->handle_type() == handle_type &&
                   grant.second->GetStatus() == PermissionStatus::GRANTED;
          });
    }
    if (!found && value.FindBoolKey(kPermissionWritableKey).value_or(false)) {
      found = base::ranges::any_of(
          origin_state.write_grants, [&path, &handle_type](auto& grant) {
            return grant.first == path &&
                   grant.second->handle_type() == handle_type &&
                   grant.second->GetStatus() == PermissionStatus::GRANTED;
          });
    }
  }
  if (found) {
    value.SetKey(kPermissionLastUsedTimeKey, base::TimeToValue(clock_->Now()));
    GrantObjectPermission(origin, std::move(value));
  } else {
    auto last_activity_time =
        base::ValueToTime(value.FindKey(kPermissionLastUsedTimeKey))
            .value_or(base::Time::Min());
    // Allow a grace period before revoking permissions to allow for better
    // metrics regarding permission timeouts.
    if (PersistentPermissionIsExpired(
            last_activity_time + kPersistentPermissionGracePeriod,
            has_extended_permissions)) {
      RevokeObjectPermission(origin, GetKeyForObject(value));
    }
  }
}

absl::optional<base::Value>
ChromeFileSystemAccessPermissionContext::GetPersistedPermission(
    const url::Origin& origin,
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return absl::nullopt;
  }

  // Don't persist permissions when the origin is allowlisted or blocked.
  auto content_setting = GetWriteGuardContentSetting(origin);
  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    return absl::nullopt;
  }

  // TODO(https://crbug.com/984772): If a parent directory has a persisted
  // permission, we should return true here.

  const std::unique_ptr<Object> object =
      GetGrantedObject(origin, PathAsPermissionKey(path));
  if (!object)
    return absl::nullopt;

  return std::move(object->value);
}

std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetAllGrantedOrExpiredObjects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ObjectPermissionContextBase::GetAllGrantedObjects();
}

bool ChromeFileSystemAccessPermissionContext::HasPersistedPermissionForTesting(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasPersistedPermission(origin, path, handle_type, grant_type,
                                MetricsOptions::kDoNotRecord);
}

bool ChromeFileSystemAccessPermissionContext::HasPersistedPermission(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    GrantType grant_type,
    MetricsOptions options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& grant = GetPersistedPermission(origin, path);

  if (!grant.has_value())
    return false;

  if (grant->FindBoolKey(kPermissionIsDirectoryKey).value() !=
      (handle_type == HandleType::kDirectory)) {
    return false;
  }

  if (!grant->FindBoolKey(GetGrantKeyFromGrantType(grant_type))
           .value_or(false)) {
    return false;
  }

  auto has_extended_permissions = OriginHasExtendedPermissions(origin);
  auto last_activity_time =
      base::ValueToTime(grant->FindKey(kPermissionLastUsedTimeKey)).value();

  if (options == MetricsOptions::kRecord) {
    base::UmaHistogramCustomTimes(
        base::StrCat({"Storage.FileSystemAccess.PersistedPermissions.Age.",
                      has_extended_permissions ? "PWA" : "NonPWA"}),
        clock_->Now() - last_activity_time, base::Seconds(1), base::Days(24),
        60);
  }

  return !PersistentPermissionIsExpired(last_activity_time,
                                        has_extended_permissions);
}

bool ChromeFileSystemAccessPermissionContext::PersistentPermissionIsExpired(
    const base::Time& last_used,
    bool has_extended_permissions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta duration =
      has_extended_permissions ? kPersistentPermissionExpirationTimeoutExtended
                               : kPersistentPermissionExpirationTimeoutDefault;

  return (last_used + duration) < clock_->Now();
}

void ChromeFileSystemAccessPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  if (it == origins_.end())
    return;

  auto& grants = grant->type() == GrantType::kRead ? it->second.read_grants
                                                   : it->second.write_grants;
  auto grant_it = grants.find(grant->path());
  // Any non-denied permission grants should have still been in our grants list.
  // If this invariant is voilated we would have permissions that might be
  // granted but won't be visible in any UI because the permission context isn't
  // tracking them anymore.
  if (grant_it == grants.end()) {
    DCHECK_EQ(PermissionStatus::DENIED, grant->GetStatus());
    return;
  }

  // The grant in |grants| for this path might have been replaced with a
  // different grant. Only erase if it actually matches the grant that was
  // destroyed.
  if (grant_it->second == grant)
    grants.erase(grant_it);

  ScheduleUsageIconUpdate();
}

void ChromeFileSystemAccessPermissionContext::ScheduleUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (usage_icon_update_scheduled_)
    return;
  usage_icon_update_scheduled_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeFileSystemAccessPermissionContext::DoUsageIconUpdate,
          weak_factory_.GetWeakPtr()));
}

void ChromeFileSystemAccessPermissionContext::DoUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_icon_update_scheduled_ = false;
#if !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile())
      continue;
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kFileSystemAccess);
  }
#endif
}

base::WeakPtr<ChromeFileSystemAccessPermissionContext>
ChromeFileSystemAccessPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
