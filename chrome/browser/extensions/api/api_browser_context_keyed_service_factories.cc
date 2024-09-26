// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_factory_impl.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_event_router_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/settings_overrides/settings_overrides_api.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_api.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/common/buildflags.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_api.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_events.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/api/image_writer_private/image_writer_controller_lacros.h"
#endif

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif

namespace chrome_extensions {

void EnsureApiBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::ActivityLogAPI::GetFactoryInstance();
  extensions::AutofillPrivateEventRouterFactory::GetInstance();
  extensions::BluetoothLowEnergyAPI::GetFactoryInstance();
  extensions::BookmarksAPI::GetFactoryInstance();
  extensions::BookmarkManagerPrivateAPI::GetFactoryInstance();
  extensions::BrailleDisplayPrivateAPI::GetFactoryInstance();
  extensions::CommandService::GetFactoryInstance();
  extensions::CookiesAPI::GetFactoryInstance();
  extensions::DeveloperPrivateAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS)
  extensions::DocumentScanAPIHandler::GetFactoryInstance();
#endif
  extensions::ExtensionActionAPI::GetFactoryInstance();
  extensions::FontSettingsAPI::GetFactoryInstance();
  extensions::HistoryAPI::GetFactoryInstance();
  extensions::IdentityAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  extensions::image_writer::ImageWriterControllerLacros::GetFactoryInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::InputImeAPI::GetFactoryInstance();
#endif
  extensions::LanguageSettingsPrivateDelegateFactory::GetInstance();
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  extensions::MDnsAPI::GetFactoryInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto networking_private_ui_delegate_factory =
      std::make_unique<extensions::NetworkingPrivateUIDelegateFactoryImpl>();
  extensions::NetworkingPrivateDelegateFactory::GetInstance()
      ->SetUIDelegateFactory(std::move(networking_private_ui_delegate_factory));
#endif
  extensions::OmniboxAPI::GetFactoryInstance();
  extensions::PasswordsPrivateDelegateFactory::GetInstance();
  extensions::PasswordsPrivateEventRouterFactory::GetInstance();
  extensions::PreferenceAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  extensions::PrintingAPIHandler::GetFactoryInstance();
#endif
  extensions::ProcessesAPI::GetFactoryInstance();
  extensions::ReadingListEventRouterFactory::GetInstance();
  extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance();
  extensions::SessionsAPI::GetFactoryInstance();
  extensions::SettingsPrivateEventRouterFactory::GetInstance();
  extensions::SettingsOverridesAPI::GetFactoryInstance();
  extensions::SidePanelService::GetFactoryInstance();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  extensions::SystemIndicatorManagerFactory::GetInstance();
#endif
  extensions::TabGroupsEventRouterFactory::GetInstance();
  extensions::TabCaptureRegistry::GetFactoryInstance();
  extensions::TabsWindowsAPI::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::TerminalPrivateAPI::GetFactoryInstance();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  extensions::VerifyTrustAPI::GetFactoryInstance();
#endif
  extensions::WebAuthenticationProxyAPI::GetFactoryInstance();
  extensions::WebAuthenticationProxyRegistrarFactory::GetInstance();
  extensions::WebAuthenticationProxyServiceFactory::GetInstance();
  extensions::WebNavigationAPI::GetFactoryInstance();
  extensions::WebrtcAudioPrivateEventService::GetFactoryInstance();
#if BUILDFLAG(IS_CHROMEOS)
  extensions::WMDesksPrivateEventsAPI::GetFactoryInstance();
#endif
}

}  // namespace chrome_extensions
