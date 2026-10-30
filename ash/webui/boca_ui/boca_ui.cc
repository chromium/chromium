// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/chrome_pref_names.h"
#include "ash/webui/boca_ui/boca_app_page_handler.h"
#include "ash/webui/boca_ui/boca_util.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "ash/webui/grit/ash_boca_ui_resources_map.h"
#include "base/check_deref.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/gemini/gemini_status_fetcher.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources.h"
#include "chromeos/grit/chromeos_boca_app_bundle_resources_map.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_allowlist.h"

namespace ash::boca {

namespace {
content::WebUIDataSource* CreateAndAddHostDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeBocaAppUntrustedURL);

  source->SetDefaultResource(IDR_ASH_BOCA_UI_INDEX_HTML);
  source->AddResourcePaths(kAshBocaUiResources);

  // Resources obtained from CIPD.
  source->AddResourcePaths(kChromeosBocaAppBundleResources);
  return source;
}

void PopulateLoadTimeData(
    content::BrowserContext* browser_context,
    const ApplicationLocaleStorage& application_locale_storage,
    const PrefService& pref_service,
    content::WebUIDataSource* source) {
  const user_manager::User* const user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(browser_context);
  const version_info::Channel channel = GetChannel();
  source->AddBoolean("isDevChannel",
                     channel == version_info::Channel::DEV ||
                         channel == version_info::Channel::UNKNOWN);
  source->AddBoolean("isProducer", boca_util::IsProducer(user));
  source->AddBoolean("isConsumer", boca_util::IsConsumer(user));
  // Do not honor sub-feature flag for student as there is no way to config
  // it for consumer.
  source->AddBoolean(
      "spotlightEnabled",
      boca_util::IsConsumer(user) ||
          pref_service.GetBoolean(
              prefs::kClassManagementToolsViewScreenEligibilitySetting));
  source->AddString("appLocale", application_locale_storage.Get());
  source->AddBoolean(
      "classroomEnabled",
      pref_service.GetBoolean(
          prefs::kClassManagementToolsClassroomEligibilitySetting));
  source->AddBoolean(
      "captionEnabled",
      boca_util::IsConsumer(user) ||
          pref_service.GetBoolean(
              prefs::kClassManagementToolsCaptionEligibilitySetting));
  if (features::IsBocaSpotlightEnabled()) {
    source->AddString("spotlightUrlTemplate",
                      features::kBocaSpotlightUrlTemplate.Get());
  }
  source->AddBoolean("sessionControlsUpdate",
                     features::IsBocaLockPauseUpdateEnabled());
  source->AddBoolean("navSettingsDialog",
                     features::IsBocaNavSettingsDialogEnabled());
  source->AddBoolean("captionToggle", features::IsBocaCaptionToggleEnabled());
  source->AddBoolean("materialTypeIndicator",
                     features::IsBocaMaterialTypeUiIndicatorEnabled());

  source->AddBoolean("spotlightNativeClientUpdate",
                     features::IsBocaSpotlightRobotRequesterEnabled());
  source->AddBoolean(
      "userFeedbackAllowed",
      pref_service.GetBoolean(chrome_prefs::kUserFeedbackAllowed));
  if (features::IsBocaConfigureMaxStudentsEnabled()) {
    source->AddInteger("maxNumStudentsAllowed",
                       features::kBocaMaxNumStudentsAllowed.Get());
  }
  source->AddBoolean("screenSharingTeacher",
                     features::IsBocaScreenSharingTeacherEnabled());
  source->AddBoolean("screenSharingStudent",
                     features::IsBocaScreenSharingStudentEnabled());

  source->AddBoolean("geminiIntegration",
                     features::IsBocaGeminiIntegrationEnabled());
  source->AddString("geminiUrl", features::kBocaGeminiUrl.Get());
  source->AddString("geminiGuidedLearningUrl",
                    features::kBocaGeminiGuidedLearningUrl.Get());
}

}  // namespace

BocaUI::BocaUI(
    content::WebUI* web_ui,
    const ApplicationLocaleStorage& application_locale_storage,
    BocaSessionManager* boca_session_manager,
    bool is_producer,
    HostContentSettingsMap* host_content_settings_map,
    OnTaskSystemWebAppManager* on_task_system_web_app_manager,
    std::string gaia_id,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : UntrustedWebUIController(web_ui),
      boca_session_manager_(CHECK_DEREF(boca_session_manager)),
      is_producer_(is_producer),
      host_content_settings_map_(CHECK_DEREF(host_content_settings_map)),
      on_task_system_web_app_manager_(on_task_system_web_app_manager),
      gaia_id_(std::move(gaia_id)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      url_loader_factory_(std::move(url_loader_factory)),
      pref_service_(CHECK_DEREF(pref_service)),
      spotlight_service_(boca_session_manager) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source =
      CreateAndAddHostDataSource(browser_context);

  // Allow styles to include inline styling needed for Polymer elements and
  // the material 3 dynamic palette.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline' chrome-untrusted://theme;");

  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");

  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer_resin lit-html goog#html polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  // For loading favicon and profile photo.
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src 'self' https: data:;");

  // For testing
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources chrome-untrusted://webui-test "
      "'self';");

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* permissions_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeBocaAppUntrustedURL));
  permissions_allowlist->RegisterAutoGrantedPermissions(
      untrusted_origin, {
                            ContentSettingsType::IMAGES,
                            ContentSettingsType::JAVASCRIPT,
                            ContentSettingsType::SOUND,
                        });
  PopulateLoadTimeData(browser_context, application_locale_storage,
                       pref_service_.get(), host_source);
  host_source->UseStringsJs();

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  host_source->SetDefaultResource(IDR_ASH_BOCA_UI_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

BocaUI::~BocaUI() = default;

void BocaUI::BindInterface(
    mojo::PendingReceiver<boca::mojom::BocaPageHandlerFactory> factory) {
  receiver_.reset();
  receiver_.Bind(std::move(factory));
}

void BocaUI::Create(
    mojo::PendingReceiver<boca::mojom::PageHandler> page_handler,
    mojo::PendingRemote<boca::mojom::Page> page) {
  content::BrowserContext* context =
      web_ui()->GetWebContents()->GetBrowserContext();
  CHECK(context);
  auto content_settings_handler = std::make_unique<ContentSettingsHandler>(
      &host_content_settings_map_.get());
  boca_session_manager_->OnAppWindowOpened();
  page_handler_impl_ = std::make_unique<BocaAppHandler>(
      std::move(page_handler), std::move(page), web_ui(),
      &boca_session_manager_.get(),
      std::make_unique<ClassroomPageHandlerImpl>(
          CHECK_DEREF(boca_session_manager_->session_client_impl())),
      std::move(content_settings_handler),
      TabInfoCollector::Create(web_ui(), is_producer_),
      on_task_system_web_app_manager_,
      boca_session_manager_->session_client_impl(),
      std::make_unique<GeminiStatusFetcher>(gaia_id_, &identity_manager_.get(),
                                            url_loader_factory_,
                                            &pref_service_.get()),
      is_producer_);
  page_handler_impl_->SetSpotlightService(&spotlight_service_);
  if (is_producer_) {
    ash::boca::util::EnableOrDisableMarkerMode(/*enable=*/true);
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(BocaUI)

}  // namespace ash::boca
