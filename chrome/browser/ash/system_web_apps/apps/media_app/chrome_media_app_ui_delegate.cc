// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/media_app/chrome_media_app_ui_delegate.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/file_system_access_helpers.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_app_ash.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/channel_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

ChromeMediaAppUIDelegate::ChromeMediaAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeMediaAppUIDelegate::~ChromeMediaAppUIDelegate() {}

std::optional<std::string> ChromeMediaAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kMediaAppFeedbackCategoryTag[] = "FromMediaApp";

  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  chrome::ShowFeedbackPage(GURL(ash::kChromeUIMediaAppURL), profile,
                           feedback::kFeedbackSourceMediaApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kMediaAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);

  // TODO(crbug/1048368): Showing the feedback dialog can fail, communicate this
  // back to the client with an error string. For now assume dialog opened.
  return std::nullopt;
}

void ChromeMediaAppUIDelegate::ToggleBrowserFullscreenMode() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui_->GetWebContents());
  if (browser) {
    chrome::ToggleFullscreenMode(browser);
  }
}

void ChromeMediaAppUIDelegate::MaybeTriggerPdfHats() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  const base::flat_map<std::string, std::string> product_specific_data;

  if (ash::HatsNotificationController::ShouldShowSurveyToProfile(
          profile, ash::kHatsMediaAppPdfSurvey)) {
    hats_notification_controller_ = new ash::HatsNotificationController(
        profile, ash::kHatsMediaAppPdfSurvey, product_specific_data);
  }
}

void ChromeMediaAppUIDelegate::IsFileArcWritable(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    base::OnceCallback<void(bool)> is_file_arc_writable_callback) {
  ash::ResolveTransferToken(
      std::move(token), web_ui_->GetWebContents(),
      base::BindOnce(&ChromeMediaAppUIDelegate::IsFileArcWritableImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(is_file_arc_writable_callback)));
}

void ChromeMediaAppUIDelegate::EditInPhotos(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    const std::string& mime_type,
    base::OnceCallback<void()> edit_in_photos_callback) {
  ash::ResolveTransferToken(
      std::move(token), web_ui_->GetWebContents(),
      base::BindOnce(&ChromeMediaAppUIDelegate::EditInPhotosImpl,
                     weak_ptr_factory_.GetWeakPtr(), mime_type,
                     std::move(edit_in_photos_callback)));
}

void ChromeMediaAppUIDelegate::IsFileArcWritableImpl(
    base::OnceCallback<void(bool)> is_file_arc_writable_callback,
    std::optional<storage::FileSystemURL> url) {
  if (!url.has_value()) {
    std::move(is_file_arc_writable_callback).Run(false);
    return;
  }

  using file_manager::Volume;
  using file_manager::VolumeManager;
  using file_manager::VolumeType;
  VolumeManager* const volume_manager =
      VolumeManager::Get(web_ui_->GetWebContents()->GetBrowserContext());

  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeFromPath(url->path());

  if (!volume) {
    std::move(is_file_arc_writable_callback).Run(false);
    return;
  }

  switch (volume->type()) {
    case VolumeType::VOLUME_TYPE_DOWNLOADS_DIRECTORY:
    case VolumeType::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
    case VolumeType::VOLUME_TYPE_ANDROID_FILES:
      std::move(is_file_arc_writable_callback).Run(true);
      return;
    case VolumeType::VOLUME_TYPE_TESTING:
    case VolumeType::VOLUME_TYPE_GOOGLE_DRIVE:
    case VolumeType::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
    case VolumeType::VOLUME_TYPE_PROVIDED:
    case VolumeType::VOLUME_TYPE_MTP:
    case VolumeType::VOLUME_TYPE_MEDIA_VIEW:
    case VolumeType::VOLUME_TYPE_CROSTINI:
    case VolumeType::VOLUME_TYPE_DOCUMENTS_PROVIDER:
    case VolumeType::VOLUME_TYPE_SMB:
    case VolumeType::VOLUME_TYPE_SYSTEM_INTERNAL:
    case VolumeType::VOLUME_TYPE_GUEST_OS:
      std::move(is_file_arc_writable_callback).Run(false);
      return;
    case VolumeType::NUM_VOLUME_TYPE:
      NOTREACHED_IN_MIGRATION();
  }
}

void ChromeMediaAppUIDelegate::EditInPhotosImpl(
    const std::string& mime_type,
    base::OnceCallback<void()> edit_in_photos_callback,
    std::optional<storage::FileSystemURL> url) {
  constexpr char kPhotosKeepOpenExtraName[] =
      "com.google.android.apps.photos.editor.contract.keep_photos_open";
  constexpr char kPhotosKeepOpenExtraValue[] = "true";

  if (!url.has_value()) {
    std::move(edit_in_photos_callback).Run();
    return;
  }

  auto* web_contents = web_ui_->GetWebContents();
  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(Profile::FromWebUI(web_ui_));

  GURL filesystem_url;
  file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      proxy->profile(), url->path(), GURL(ash::kChromeUIMediaAppURL),
      &filesystem_url);

  auto intent = apps_util::MakeEditIntent(filesystem_url, mime_type);
  intent->extras = {
      std::make_pair(kPhotosKeepOpenExtraName, kPhotosKeepOpenExtraValue)};

  proxy->LaunchAppWithIntent(
      arc::kGooglePhotosAppId, ui::EF_NONE, std::move(intent),
      apps::LaunchSource::kFromOtherApp, nullptr,
      base::BindOnce(
          [](base::OnceCallback<void()> callback,
             base::WeakPtr<content::WebContents> web_contents,
             apps::LaunchResult&& result) {
            if (result.state == apps::State::kSuccess && web_contents) {
              web_contents->Close();
            }
            std::move(callback).Run();
          },
          std::move(edit_in_photos_callback), web_contents->GetWeakPtr()));
}

void ChromeMediaAppUIDelegate::SubmitForm(const GURL& url,
                                          const std::vector<int8_t>& payload,
                                          const std::string& header) {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    crosapi::CrosapiManager::Get()->crosapi_ash()->media_app_ash()->SubmitForm(
        url, payload, header, base::DoNothing());
    return;
  }
  // Keep this impl in sync with chrome/browser/lacros/media_app_lacros.cc
  Profile* profile = Profile::FromWebUI(web_ui_);
  NavigateParams navigate_params(
      profile, url,
      // The page transition is chosen to satisfy one of the conditions in
      // lacros_url_handling::IsNavigationInterceptable.
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  navigate_params.post_data = network::ResourceRequestBody::CreateFromBytes(
      reinterpret_cast<const char*>(payload.data()), payload.size());
  navigate_params.extra_headers = header;

  navigate_params.browser = chrome::FindTabbedBrowser(profile, false);
  if (!navigate_params.browser &&
      Browser::GetCreationStatusForProfile(profile) ==
          Browser::CreationStatus::kOk) {
    Browser::CreateParams create_params(profile, navigate_params.user_gesture);
    create_params.should_trigger_session_restore = false;
    navigate_params.browser = Browser::Create(create_params);
  }

  Navigate(&navigate_params);
}
