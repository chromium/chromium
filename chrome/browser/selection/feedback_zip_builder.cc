// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/feedback_zip_builder.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/net/variations_command_line.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

namespace selection {

namespace {

struct TabMetadata {
  std::string page_url;
  signin_util::SignedInState signed_in_state =
      signin_util::SignedInState::kSignedOut;
  bool is_signed_in = false;
  bool is_google_internal_account = false;
  std::optional<bool> server_suggestions_enabled;
};

std::string BuildMetadataJson(const TabMetadata& tab_metadata,
                              std::string_view fallback_url) {
  auto get_page_url = [&]() {
    return !tab_metadata.page_url.empty() ? tab_metadata.page_url
                                          : std::string(fallback_url);
  };

  auto get_os_dict = []() {
    return base::DictValue()
        .Set("type", version_info::GetOSType())
        .Set("version", base::SysInfo::OperatingSystemVersion())
        .Set("architecture", base::SysInfo::OperatingSystemArchitecture());
  };

  auto get_chrome_dict = []() {
    return base::DictValue()
        .Set("version", version_info::GetVersionNumber())
        .Set("channel",
             chrome::GetChannelName(chrome::WithExtendedStable(true)))
        .Set("revision", version_info::GetLastChange())
        .Set("is_official_build", version_info::IsOfficialBuild())
        .Set("locale", g_browser_process
                           ? g_browser_process->GetApplicationLocale()
                           : "");
  };

  auto get_account_dict = [&]() {
    return base::DictValue()
        .Set("state",
             signin_util::SignedInStateToString(tab_metadata.signed_in_state))
        .Set("is_signed_in", tab_metadata.is_signed_in)
        .Set("is_google_internal_account",
             tab_metadata.is_google_internal_account);
  };

  auto get_variations_dict = []() {
    base::FieldTrial::ActiveGroups active_groups;
    base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);

    std::vector<std::string> hashes;
    variations::GetFieldTrialActiveGroupIdsAsStrings(std::string_view(),
                                                     active_groups, &hashes);
    base::ListValue active_hashes;
    for (std::string& hash : hashes) {
      active_hashes.Append(std::move(hash));
    }

    base::ListValue active_names;
    for (const base::FieldTrial::ActiveGroup& group : active_groups) {
      active_names.Append(
          base::StrCat({group.trial_name, ":", group.group_name}));
    }

    return base::DictValue()
        .Set("active_hashes", std::move(active_hashes))
        .Set("active_names", std::move(active_names))
        .Set("command_line",
             variations::VariationsCommandLine::GetForCurrentProcess()
                 .ToString());
  };

  auto get_feature_params_dict = [&]() {
    base::DictValue dict;
    if (tab_metadata.server_suggestions_enabled) {
      dict.Set("enabled", *tab_metadata.server_suggestions_enabled);
    }
    return std::move(dict)
        .Set("timeout_ms",
             static_cast<int>(
                 kSmartSelectionServerTimeout.Get().InMilliseconds()))
        .Set("crop_padding", kSmartSelectionServerCropPadding.Get())
        .Set("max_screenshot_dimension",
             kSmartSelectionServerMaxScreenshotDimension.Get())
        .Set("jpeg_quality", kSmartSelectionServerJpegQuality.Get())
        .Set("append_apc", kSmartSelectionServerAppendApc.Get());
  };

  base::DictValue dict =
      base::DictValue()
          .Set("page_url", get_page_url())
          .Set("os", get_os_dict())
          .Set("chrome", get_chrome_dict())
          .Set("account", get_account_dict())
          .Set("variations", get_variations_dict())
          .Set("smart_selection_feature_params", get_feature_params_dict());

  return base::WriteJsonWithOptions(dict,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or("{}");
}

// Collects files to be passed to zip::Zip().
// Makes sure that the ZIP file only contains those contained files. In
// particular, no symlinks that existed before or are created during the
// lifetime of the `StagedFileAccessor` are added to the ZIP file.
class StagedFileAccessor : public zip::FileAccessor {
 public:
  explicit StagedFileAccessor(base::FilePath dir)
      : dir_(std::move(dir)), created_time_(base::Time::Now()) {}
  StagedFileAccessor(const StagedFileAccessor&) = delete;
  StagedFileAccessor& operator=(const StagedFileAccessor&) = delete;
  ~StagedFileAccessor() override = default;

  // Creates a new file and writes `data` to it.
  // Fails if the path, even as a symlink, already exists.
  // Keeps the file open for the lifetime of `this` so that zip::Zip() reads
  // that file's content, even if the file is replaced with another file in
  // the meantime.
  bool WriteNewFile(std::string_view filename, base::span<const uint8_t> data) {
    base::FilePath rel_path = base::FilePath::FromASCII(filename);
    base::File file(dir_.Append(rel_path), base::File::FLAG_CREATE |
                                               base::File::FLAG_WRITE |
                                               base::File::FLAG_READ);
    if (!file.IsValid() || !file.WriteAtCurrentPosAndCheck(data) ||
        file.Seek(base::File::FROM_BEGIN, 0) != 0) {
      return false;
    }
    files_.emplace_back(std::move(rel_path), std::move(file));
    return true;
  }

  bool WriteNewFile(std::string_view filename, std::string_view data) {
    return WriteNewFile(filename, base::as_byte_span(data));
  }

  // zip::FileAccessor:

  // Populates `files` with the files in any of the given `paths` that were
  // created with `WriteNewFile()`.
  bool Open(zip::Paths paths, std::vector<base::File>* files) override {
    files->reserve(files->size() + paths.size());
    for (const base::FilePath& path : paths) {
      auto it = std::ranges::find(files_, path, &Entry::first);
      if (it == files_.end() || !it->second.IsValid()) {
        return false;
      }
      files->push_back(std::move(it->second));
    }
    return true;
  }

  // Only lists the files that were created with `WriteNewFile()`.
  // Does not add other files or subdirectories to avoid adding unwanted data to
  // the ZIP file.
  bool List(const base::FilePath& path,
            std::vector<base::FilePath>* files,
            std::vector<base::FilePath>* subdirs) override {
    if (!path.empty()) {
      return false;
    }
    for (const auto& [rel_path, file] : files_) {
      files->push_back(rel_path);
    }
    return true;
  }

  bool GetInfo(const base::FilePath& path, Info* info) override {
    if (!std::ranges::contains(files_, path, &Entry::first)) {
      return false;
    }
    info->is_directory = false;
    info->last_modified = created_time_;
    return true;
  }

 private:
  using Entry = std::pair<base::FilePath, base::File>;
  const base::FilePath dir_;
  const base::Time created_time_;
  std::vector<Entry> files_;
};

}  // namespace

struct FeedbackZipBuilder::State {
  TabMetadata tab_metadata;
  SkBitmap screenshot;
  std::optional<EncodedImage> mes_screenshot;
  std::optional<optimization_guide::proto::AnnotatedPageContent> apc;
  std::optional<std::u16string> selected_text;
  std::optional<optimization_guide::proto::SmartSelectionSuggestionsRequest>
      request;
  std::optional<optimization_guide::proto::SmartSelectionSuggestionsResponse>
      response;
  std::optional<std::string> suggestions_json;
};

FeedbackZipBuilder::FeedbackZipBuilder() : state_(std::make_unique<State>()) {}
FeedbackZipBuilder::FeedbackZipBuilder(FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder& FeedbackZipBuilder::operator=(
    FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder::~FeedbackZipBuilder() = default;

void FeedbackZipBuilder::SetTab(tabs::TabInterface& tab,
                                bool server_suggestions_enabled) {
  CHECK(state_);
  content::WebContents* web_contents = tab.GetContents();
  Profile* profile = tab.GetProfile();
  if (!profile && web_contents) {
    profile = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  }
  signin::IdentityManager* identity_manager =
      profile ? IdentityManagerFactory::GetForProfile(profile) : nullptr;
  const signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);
  const bool is_signed_in =
      identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  const bool is_google_internal_account =
      is_signed_in &&
      gaia::IsGoogleInternalAccountEmail(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email);

  state_->tab_metadata = TabMetadata{
      .page_url = web_contents ? web_contents->GetLastCommittedURL().spec()
                               : std::string(),
      .signed_in_state = signed_in_state,
      .is_signed_in = is_signed_in,
      .is_google_internal_account = is_google_internal_account,
      .server_suggestions_enabled = server_suggestions_enabled,
  };
}

void FeedbackZipBuilder::SetScreenshot(SkBitmap screenshot) {
  CHECK(state_);
  state_->screenshot = std::move(screenshot);
}

void FeedbackZipBuilder::SetMesScreenshot(EncodedImage mes_screenshot) {
  CHECK(state_);
  state_->mes_screenshot = std::move(mes_screenshot);
}

void FeedbackZipBuilder::SetApc(
    optimization_guide::proto::AnnotatedPageContent apc) {
  CHECK(state_);
  state_->apc = std::move(apc);
}

void FeedbackZipBuilder::SetSelectedText(std::u16string selected_text) {
  CHECK(state_);
  state_->selected_text = std::move(selected_text);
}

void FeedbackZipBuilder::SetRequest(
    optimization_guide::proto::SmartSelectionSuggestionsRequest request) {
  CHECK(state_);
  state_->request = std::move(request);
}

void FeedbackZipBuilder::SetResponse(
    optimization_guide::proto::SmartSelectionSuggestionsResponse response,
    base::span<const std::unique_ptr<Suggestion>> suggestions) {
  CHECK(state_);
  state_->response = std::move(response);
  base::ListValue list;
  for (const std::unique_ptr<Suggestion>& suggestion : suggestions) {
    if (!suggestion) {
      continue;
    }
    list.Append(
        base::DictValue()
            .Set("label", base::UTF16ToUTF8(suggestion->GetLabel()))
            .Set("tool", optimization_guide::proto::SmartSelectionToolId_Name(
                             suggestion->GetToolId())));
  }
  state_->suggestions_json =
      base::WriteJsonWithOptions(list, base::JSONWriter::OPTIONS_PRETTY_PRINT)
          .value_or("[]");
}

void FeedbackZipBuilder::CreateZip(
    base::FilePath target_zip_path,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback) const {
  CHECK(state_);
  const std::string_view fallback_url =
      state_->apc.has_value()
          ? std::string_view(state_->apc->main_frame_data().url())
          : std::string_view();
  std::string metadata_json =
      BuildMetadataJson(state_->tab_metadata, fallback_url);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&FeedbackZipBuilder::CreateZipOnThreadPool,
                     std::move(target_zip_path), std::move(metadata_json),
                     *state_),
      std::move(callback));
}

// static
std::optional<base::FilePath> FeedbackZipBuilder::CreateZipOnThreadPool(
    base::FilePath target_zip_path,
    std::string metadata_json,
    State state) {
  if (target_zip_path.empty()) {
    return std::nullopt;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(target_zip_path.DirName())) {
    return std::nullopt;
  }

  StagedFileAccessor file_accessor(temp_dir.GetPath());
  auto get_image_name = [](std::string_view prefix, const EncodedImage& image) {
    CHECK(image.mime_type == "image/jpeg" || image.mime_type == "image/png");
    std::string_view ext = image.mime_type == "image/jpeg" ? "jpg" : "png";
    return base::StrCat({prefix, ".", ext});
  };
  const std::optional<EncodedImage> full_screenshot =
      EncodedImage::Create(state.screenshot);
  if (!file_accessor.WriteNewFile("metadata.json", metadata_json) ||
      (full_screenshot.has_value() &&
       !file_accessor.WriteNewFile(
           get_image_name("screenshot", *full_screenshot),
           full_screenshot->bytes)) ||
      (state.mes_screenshot.has_value() &&
       !file_accessor.WriteNewFile(
           get_image_name("mes_screenshot", *state.mes_screenshot),
           state.mes_screenshot->bytes)) ||
      (state.apc.has_value() &&
       !file_accessor.WriteNewFile("apc.pb", state.apc->SerializeAsString())) ||
      (state.selected_text.has_value() &&
       !file_accessor.WriteNewFile("selection.txt",
                                   base::UTF16ToUTF8(*state.selected_text))) ||
      (state.request.has_value() &&
       !file_accessor.WriteNewFile("request.pb",
                                   state.request->SerializeAsString())) ||
      (state.response.has_value() &&
       !file_accessor.WriteNewFile("response.pb",
                                   state.response->SerializeAsString())) ||
      (state.suggestions_json.has_value() &&
       !file_accessor.WriteNewFile("suggestions.json",
                                   *state.suggestions_json))) {
    return std::nullopt;
  }

  base::FilePath zip_path = base::GetUniquePath(target_zip_path);
  if (zip_path.empty()) {
    return std::nullopt;
  }
  if (!zip::Zip({.file_accessor = &file_accessor, .dest_file = zip_path})) {
    base::DeleteFile(zip_path);
    return std::nullopt;
  }
  return zip_path;
}

}  // namespace selection
