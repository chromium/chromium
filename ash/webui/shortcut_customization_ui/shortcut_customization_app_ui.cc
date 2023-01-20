// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"

#include <memory>
#include <utility>

#include "ash/webui/grit/ash_shortcut_customization_app_resources.h"
#include "ash/webui/grit/ash_shortcut_customization_app_resources_map.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_SHORTCUT_CUSTOMIZATION_APP_TITLE},
      {"keyboardSettings", IDS_SHORTCUT_CUSTOMIZATION_KEYBOARD_SETTINGS},
      {"addShortcut", IDS_SHORTCUT_CUSTOMIZATION_ADD_SHORTCUT},
      {"restoreDefaults", IDS_SHORTCUT_CUSTOMIZATION_RESTORE_DEFAULTS},
      {"editDialogDone", IDS_SHORTCUT_CUSTOMIZATION_EDIT_DIALOG_DONE},
      {"cancel", IDS_SHORTCUT_CUSTOMIZATION_CANCEL},
      {"editViewStatusMessage",
       IDS_SHORTCUT_CUSTOMIZATION_EDIT_VIEW_STATUS_MESSAGE},
      {"resetAllShortcuts", IDS_SHORTCUT_CUSTOMIZATION_RESET_ALL_SHORTCUTS},
      {"confirmResetAllShortcutsTitle",
       IDS_SHORTCUT_CUSTOMIZATION_CONFIRM_RESET_ALL_SHORTCUTS_TITLE},
      {"confirmResetAllShortcutsButton",
       IDS_SHORTCUT_CUSTOMIZATION_CONFIRM_RESET_SHORTCUTS_BUTTON},
      {"categoryTabsAndWindows",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_TABS_AND_WINDOWS},
      {"categoryPageAndWebBrowser",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_PAGE_AND_WEB_BROWSER},
      {"categorySystemAndDisplaySettings",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_SYSTEM_AND_DISPLAY_SETTINGS},
      {"categoryTextEditing", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_TEXT_EDITING},
      {"categoryAccessibility",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_ACCESSIBILITY},
      {"categoryDebug", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_DEBUG},
      {"categoryDeveloper", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_DEVELOPER},
      {"categoryEventRewriter",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_EVENT_REWRITER},
      {"shortcutWithConflictStatusMessage",
       IDS_SHORTCUT_CUSTOMIZATION_SHORTCUT_WITH_CONFILICT_STATUS_MESSAGE},
      {"lockedShortcutStatusMessage",
       IDS_SHORTCUT_CUSTOMIZATION_LOCKED_SHORTCUT_STATUS_MESSAGE},
      {"subcategoryGeneral", IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_GENERAL},
      {"subcategorySystemApps",
       IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_SYSTEM_APPS},
      {"subcategorySystemControls",
       IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_SYSTEM_CONTROLS},
      {"subcategorySixPackKeys",
       IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_SIX_PACK},
      {"iconLabelArrowDown", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_DOWN},
      {"iconLabelArrowLeft", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_LEFT},
      {"iconLabelArrowRight",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_RIGHT},
      {"iconLabelArrowUp", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ARROW_UP},
      {"iconLabelAudioVolumeDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_DOWN},
      {"iconLabelAudioVolumeMute",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_MUTE},
      {"iconLabelAudioVolumeUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_AUDIO_VOLUME_UP},
      {"iconLabelBrightnessDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BRIGHTNESS_DOWN},
      {"iconLabelBrightnessUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BRIGHTNESS_UP},
      {"iconLabelBrowserBack",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_BACK},
      {"iconLabelBrowserForward",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_FORWARD},
      {"iconLabelBrowserRefresh",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_BROWSER_REFRESH},
      {"iconLabelKeyboardBacklightToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BACKLIGHT_TOGGLE},
      {"iconLabelKeyboardBrightnessUp",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_UP},
      {"iconLabelKeyboardBrightnessDown",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_KEYBOARD_BRIGHTNESS_DOWN},
      {"iconLabelLaunchApplication1",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_APPLICATION1},
      {"iconLabelLaunchAssistant",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_LAUNCH_ASSISTANT},
      {"iconLabelMediaPlayPause",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_PLAY_PAUSE},
      {"iconLabelMediaTrackNext",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_NEXT},
      {"iconLabelMediaTrackPrevious",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MEDIA_TRACK_PREVIOUS},
      {"iconLabelMicrophoneMuteToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_MICROPHONE_MUTE_TOGGLE},
      {"iconLabelOpenLauncher",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_OPEN_LAUNCHER},
      {"iconLabelPower", IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_POWER},
      {"iconLabelPrintScreen",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRINT_SCREEN},
      {"iconLabelPrivacyScreenToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_PRIVACY_SCREEN_TOGGLE},
      {"iconLabelZoomToggle",
       IDS_SHORTCUT_CUSTOMIZATION_ICON_LABEL_ZOOM_TOGGLE},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

void AddFeatureFlags(content::WebUIDataSource* html_source) {
  html_source->AddBoolean("isCustomizationEnabled",
                          features::IsShortcutCustomizationEnabled());
}

}  // namespace

ShortcutCustomizationAppUI::ShortcutCustomizationAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIShortcutCustomizationAppHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test chrome://webui-test "
      "'self';");

  source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshShortcutCustomizationAppResources,
                      kAshShortcutCustomizationAppResourcesSize);
  SetUpWebUIDataSource(source, resources,
                       IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_INDEX_HTML);
  AddLocalizedStrings(source);

  AddFeatureFlags(source);

  provider_ = std::make_unique<shortcut_ui::AcceleratorConfigurationProvider>();
}

ShortcutCustomizationAppUI::~ShortcutCustomizationAppUI() = default;

void ShortcutCustomizationAppUI::BindInterface(
    mojo::PendingReceiver<
        shortcut_customization::mojom::AcceleratorConfigurationProvider>
        receiver) {
  provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShortcutCustomizationAppUI)
}  // namespace ash
