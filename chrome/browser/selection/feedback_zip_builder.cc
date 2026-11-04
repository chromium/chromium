// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/feedback_zip_builder.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
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
  if (target_zip_path.empty() ||
      !base::CreateDirectory(target_zip_path.DirName())) {
    return std::nullopt;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return std::nullopt;
  }

  const base::FilePath dir = temp_dir.GetPath();
  auto get_image_path = [&](std::string_view prefix,
                            const EncodedImage& image) {
    CHECK(image.mime_type == "image/jpeg" || image.mime_type == "image/png");
    std::string_view ext = image.mime_type == "image/jpeg" ? "jpg" : "png";
    return dir.AppendASCII(base::StrCat({prefix, ".", ext}));
  };
  const std::optional<EncodedImage> full_screenshot =
      EncodedImage::Create(state.screenshot);
  if (!base::WriteFile(dir.AppendASCII("metadata.json"), metadata_json) ||
      (full_screenshot.has_value() &&
       !base::WriteFile(get_image_path("screenshot", *full_screenshot),
                        full_screenshot->bytes)) ||
      (state.mes_screenshot.has_value() &&
       !base::WriteFile(get_image_path("mes_screenshot", *state.mes_screenshot),
                        state.mes_screenshot->bytes)) ||
      (state.apc.has_value() &&
       !base::WriteFile(dir.AppendASCII("apc.pb"),
                        state.apc->SerializeAsString())) ||
      (state.selected_text.has_value() &&
       !base::WriteFile(dir.AppendASCII("selection.txt"),
                        base::UTF16ToUTF8(*state.selected_text))) ||
      (state.request.has_value() &&
       !base::WriteFile(dir.AppendASCII("request.pb"),
                        state.request->SerializeAsString())) ||
      (state.response.has_value() &&
       !base::WriteFile(dir.AppendASCII("response.pb"),
                        state.response->SerializeAsString())) ||
      (state.suggestions_json.has_value() &&
       !base::WriteFile(dir.AppendASCII("suggestions.json"),
                        *state.suggestions_json))) {
    return std::nullopt;
  }

  base::FilePath zip_path = base::GetUniquePath(target_zip_path);
  if (zip_path.empty()) {
    return std::nullopt;
  }
  if (!zip::Zip(temp_dir.GetPath(), zip_path, /*include_hidden_files=*/false)) {
    base::DeleteFile(zip_path);
    return std::nullopt;
  }
  return zip_path;
}

}  // namespace selection
