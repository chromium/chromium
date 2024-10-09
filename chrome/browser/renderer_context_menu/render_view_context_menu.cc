// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_menu_observer.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/link_to_text_menu_observer.h"
#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/lens/lens_constants.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/lens_metrics.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/pdf/common/pdf_util.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sharing_message/features.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_util.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_notes/user_notes_features.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/origin.h"

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#include "components/compose/core/browser/compose_manager.h"
#include "components/compose/core/browser/compose_metrics.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/pdf/common/constants.h"
#include "pdf/pdf_features.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "ui/base/menu_source_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/arc/open_with_menu.h"
#include "chrome/browser/chromeos/arc/start_smart_selection_action_menu.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/renderer_context_menu/read_write_card_observer.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/clipboard_history/clipboard_history_submenu_model.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/intent_helper/arc_intent_helper_mojo_ash.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/arc/arc_intent_helper_mojo_lacros.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/aura/window.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#endif

using base::UserMetricsAction;
using blink::ContextMenuData;
using blink::ContextMenuDataEditFlags;
using blink::mojom::ContextMenuDataMediaType;
using content::BrowserContext;
using content::ChildProcessSecurityPolicy;
using content::DownloadManager;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;
using download::DownloadUrlParameters;
using extensions::ContextMenuMatcher;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

constexpr char kOpenLinkAsProfileHistogram[] =
    "RenderViewContextMenu.OpenLinkAsProfile";

base::OnceCallback<void(RenderViewContextMenu*)>* GetMenuShownCallback() {
  static base::NoDestructor<base::OnceCallback<void(RenderViewContextMenu*)>>
      callback;
  return callback.get();
}

enum class UmaEnumIdLookupType {
  GeneralEnumId,
  ContextSpecificEnumId,
};

// Count when Open Link as Profile or Incognito Window menu item is displayed or
// clicked. Metric: "RenderViewContextMenu.OpenLinkAsProfile".
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OpenLinkAs {
  kOpenLinkAsProfileDisplayed = 0,
  kOpenLinkAsProfileClicked = 1,
  kOpenLinkAsIncognitoDisplayed = 2,
  kOpenLinkAsIncognitoClicked = 3,
  kMaxValue = kOpenLinkAsIncognitoClicked,
};

const std::map<int, int>& GetIdcToUmaMap(UmaEnumIdLookupType type) {
  // These maps are from IDC_* -> UMA value. Never alter UMA ids. You may remove
  // items, but add a line to keep the old value from being reused.

  // These UMA values are for the RenderViewContextMenuItem enum, used for
  // the RenderViewContextMenu.Shown and RenderViewContextMenu.Used histograms.
  static const base::NoDestructor<std::map<int, int>> kGeneralMap(
      {// NB: UMA values for 0 and 1 are detected using
       // RenderViewContextMenu::IsContentCustomCommandId() and
       // ContextMenuMatcher::IsExtensionsCustomCommandId()
       {IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST, 2},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 3},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 4},
       {IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 5},
       {IDC_CONTENT_CONTEXT_SAVELINKAS, 6},
       {IDC_CONTENT_CONTEXT_SAVEAVAS, 7},
       {IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 8},
       {IDC_CONTENT_CONTEXT_COPYLINKLOCATION, 9},
       {IDC_CONTENT_CONTEXT_COPYIMAGELOCATION, 10},
       {IDC_CONTENT_CONTEXT_COPYAVLOCATION, 11},
       {IDC_CONTENT_CONTEXT_COPYIMAGE, 12},
       {IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB, 13},
       {IDC_CONTENT_CONTEXT_OPENAVNEWTAB, 14},
       // Removed: {IDC_CONTENT_CONTEXT_PLAYPAUSE, 15},
       // Removed: {IDC_CONTENT_CONTEXT_MUTE, 16},
       {IDC_CONTENT_CONTEXT_LOOP, 17},
       {IDC_CONTENT_CONTEXT_CONTROLS, 18},
       {IDC_CONTENT_CONTEXT_ROTATECW, 19},
       {IDC_CONTENT_CONTEXT_ROTATECCW, 20},
       {IDC_BACK, 21},
       {IDC_FORWARD, 22},
       {IDC_SAVE_PAGE, 23},
       {IDC_RELOAD, 24},
       {IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP, 25},
       {IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP, 26},
       {IDC_PRINT, 27},
       {IDC_VIEW_SOURCE, 28},
       {IDC_CONTENT_CONTEXT_INSPECTELEMENT, 29},
       {IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE, 30},
       {IDC_CONTENT_CONTEXT_VIEWPAGEINFO, 31},
       {IDC_CONTENT_CONTEXT_TRANSLATE, 32},
       {IDC_CONTENT_CONTEXT_RELOADFRAME, 33},
       {IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE, 34},
       {IDC_CONTENT_CONTEXT_VIEWFRAMEINFO, 35},
       {IDC_CONTENT_CONTEXT_UNDO, 36},
       {IDC_CONTENT_CONTEXT_REDO, 37},
       {IDC_CONTENT_CONTEXT_CUT, 38},
       {IDC_CONTENT_CONTEXT_COPY, 39},
       {IDC_CONTENT_CONTEXT_PASTE, 40},
       {IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE, 41},
       {IDC_CONTENT_CONTEXT_DELETE, 42},
       {IDC_CONTENT_CONTEXT_SELECTALL, 43},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFOR, 44},
       {IDC_CONTENT_CONTEXT_GOTOURL, 45},
       {IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS, 46},
       {IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS, 47},
       {IDC_CONTENT_CONTEXT_OPENLINKWITH, 52},
       {IDC_CHECK_SPELLING_WHILE_TYPING, 53},
       {IDC_SPELLCHECK_MENU, 54},
       {IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, 55},
       {IDC_SPELLCHECK_LANGUAGES_FIRST, 56},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE, 57},
       {IDC_SPELLCHECK_SUGGESTION_0, 58},
       {IDC_SPELLCHECK_ADD_TO_DICTIONARY, 59},
       {IDC_SPELLPANEL_TOGGLE, 60},
       {IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB, 61},
       {IDC_WRITING_DIRECTION_MENU, 62},
       {IDC_WRITING_DIRECTION_DEFAULT, 63},
       {IDC_WRITING_DIRECTION_LTR, 64},
       {IDC_WRITING_DIRECTION_RTL, 65},
       {IDC_CONTENT_CONTEXT_LOAD_IMAGE, 66},
       {IDC_ROUTE_MEDIA, 68},
       {IDC_CONTENT_CONTEXT_COPYLINKTEXT, 69},
       {IDC_CONTENT_CONTEXT_OPENLINKINPROFILE, 70},
       {IDC_OPEN_LINK_IN_PROFILE_FIRST, 71},
       {IDC_CONTENT_CONTEXT_GENERATEPASSWORD, 72},
       {IDC_SPELLCHECK_MULTI_LINGUAL, 73},
       {IDC_CONTENT_CONTEXT_OPEN_WITH1, 74},
       {IDC_CONTENT_CONTEXT_OPEN_WITH2, 75},
       {IDC_CONTENT_CONTEXT_OPEN_WITH3, 76},
       {IDC_CONTENT_CONTEXT_OPEN_WITH4, 77},
       {IDC_CONTENT_CONTEXT_OPEN_WITH5, 78},
       {IDC_CONTENT_CONTEXT_OPEN_WITH6, 79},
       {IDC_CONTENT_CONTEXT_OPEN_WITH7, 80},
       {IDC_CONTENT_CONTEXT_OPEN_WITH8, 81},
       {IDC_CONTENT_CONTEXT_OPEN_WITH9, 82},
       {IDC_CONTENT_CONTEXT_OPEN_WITH10, 83},
       {IDC_CONTENT_CONTEXT_OPEN_WITH11, 84},
       {IDC_CONTENT_CONTEXT_OPEN_WITH12, 85},
       {IDC_CONTENT_CONTEXT_OPEN_WITH13, 86},
       {IDC_CONTENT_CONTEXT_OPEN_WITH14, 87},
       {IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN, 88},
       {IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP, 89},
       {IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS, 90},
       {IDC_CONTENT_CONTEXT_PICTUREINPICTURE, 91},
       {IDC_CONTENT_CONTEXT_EMOJI, 92},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1, 93},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2, 94},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3, 95},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4, 96},
       {IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5, 97},
       {IDC_CONTENT_CONTEXT_LOOK_UP, 98},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE, 99},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE, 100},
       {IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS, 101},
       {IDC_SEND_TAB_TO_SELF, 102},
       {IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE, 106},
       {IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES, 107},
       {IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE, 108},
       {IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES, 109},
       {IDC_CONTENT_CONTEXT_GENERATE_QR_CODE, 110},
       {IDC_CONTENT_CLIPBOARD_HISTORY_MENU, 111},
       {IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 112},
       {IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE, 113},
       {IDC_CONTENT_CONTEXT_REMOVELINKTOTEXT, 114},
       {IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, 115},
       {IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH, 116},
       {IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT, 117},
       {IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE, 118},
       // Removed: {IDC_FOLLOW, 119},
       // Removed: {IDC_UNFOLLOW, 120},
       // Removed: {IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST, 121},
       {IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE, 123},
       // Removed: {IDC_CONTENT_CONTEXT_ADD_A_NOTE, 124},
       {IDC_LIVE_CAPTION, 125},
       // Removed: {IDC_CONTENT_CONTEXT_PDF_OCR, 126},
       // Removed: {IDC_CONTENT_CONTEXT_PDF_OCR_ALWAYS, 127},
       // Removed: {IDC_CONTENT_CONTEXT_PDF_OCR_ONCE, 128},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK, 129},
       {IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB, 130},
       {IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS, 131},
       {IDC_CONTENT_CONTEXT_COPYVIDEOFRAME, 132},
       {IDC_CONTENT_CONTEXT_SAVEPLUGINAS, 133},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_ADDRESS, 134},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB, 135},
       // Removed: {IDC_CONTENT_CONTEXT_ORCA, 136},
       // Removed: {IDC_CONTENT_CONTEXT_RUN_LAYOUT_EXTRACTION, 137},
       {IDC_CONTENT_PASTE_FROM_CLIPBOARD, 138},
       {IDC_CONTEXT_COMPOSE, 139},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PAYMENTS, 140},
       {IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS, 141},
       {IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME, 142},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORVIDEOFRAME, 143},
       {IDC_CONTENT_CONTEXT_OPENLINKPREVIEW, 144},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS, 145},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS, 146},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD, 147},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS, 148},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD, 149},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_NO_SAVED_PASSWORDS,
        150},
       {IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS, 151},
       {IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
        152},
       // To add new items:
       //   - Add one more line above this comment block, using the UMA value
       //     from the line below this comment block.
       //   - Increment the UMA value in that latter line.
       //   - Add the new item to the RenderViewContextMenuItem enum in
       //     tools/metrics/histograms/enums.xml.
       {0, 153}});

  // These UMA values are for the ContextMenuOptionDesktop enum, used for
  // the ContextMenu.SelectedOptionDesktop histograms.
  static const base::NoDestructor<std::map<int, int>> kSpecificMap(
      {{IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0},
       {IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 1},
       {IDC_CONTENT_CONTEXT_COPYLINKLOCATION, 2},
       {IDC_CONTENT_CONTEXT_COPY, 3},
       {IDC_CONTENT_CONTEXT_SAVELINKAS, 4},
       {IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 5},
       {IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB, 6},
       {IDC_CONTENT_CONTEXT_COPYIMAGE, 7},
       {IDC_CONTENT_CONTEXT_COPYIMAGELOCATION, 8},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE, 9},
       {IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 10},
       {IDC_PRINT, 11},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFOR, 12},
       {IDC_CONTENT_CONTEXT_SAVEAVAS, 13},
       {IDC_SPELLCHECK_SUGGESTION_0, 14},
       {IDC_SPELLCHECK_ADD_TO_DICTIONARY, 15},
       {IDC_CONTENT_CONTEXT_SPELLING_TOGGLE, 16},
       {IDC_CONTENT_CONTEXT_CUT, 17},
       {IDC_CONTENT_CONTEXT_PASTE, 18},
       {IDC_CONTENT_CONTEXT_GOTOURL, 19},
       {IDC_CONTENT_CONTEXT_COPYLINKTOTEXT, 20},
       {IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE, 21},
       {IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, 22},
       {IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH, 23},
       {IDC_CONTENT_CONTEXT_RESHARELINKTOTEXT, 24},
       {IDC_OPEN_LINK_IN_PROFILE_FIRST, 25},
       // Removed: {IDC_CONTENT_CONTEXT_ADD_A_NOTE, 26},
       {IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB, 27},
       {IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS, 28},
       {IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB, 29},
       {IDC_CONTENT_CONTEXT_OPENLINKPREVIEW, 30},
       // To add new items:
       //   - Add one more line above this comment block, using the UMA value
       //     from the line below this comment block.
       //   - Increment the UMA value in that latter line.
       //   - Add the new item to the ContextMenuOptionDesktop enum in
       //     tools/metrics/histograms/enums.xml.
       {0, 31}});

  return *(type == UmaEnumIdLookupType::GeneralEnumId ? kGeneralMap
                                                      : kSpecificMap);
}

int GetUmaValueMax(UmaEnumIdLookupType type) {
  // The IDC_ "value" of 0 is really a sentinel for the UMA max value.
  return GetIdcToUmaMap(type).find(0)->second;
}

// Collapses large ranges of ids before looking for UMA enum.
int CollapseCommandsForUMA(int id) {
  DCHECK(!RenderViewContextMenu::IsContentCustomCommandId(id));
  DCHECK(!ContextMenuMatcher::IsExtensionsCustomCommandId(id));

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  }

  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id <= IDC_SPELLCHECK_LANGUAGES_LAST) {
    return IDC_SPELLCHECK_LANGUAGES_FIRST;
  }

  if (id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    return IDC_SPELLCHECK_SUGGESTION_0;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return IDC_OPEN_LINK_IN_PROFILE_FIRST;
  }

  return id;
}

// Returns UMA enum value for command specified by |id| or -1 if not found.
int FindUMAEnumValueForCommand(int id, UmaEnumIdLookupType type) {
  if (RenderViewContextMenu::IsContentCustomCommandId(id)) {
    return 0;
  }

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    return 1;
  }

  id = CollapseCommandsForUMA(id);
  const auto& map = GetIdcToUmaMap(type);
  auto it = map.find(id);
  if (it == map.end()) {
    return -1;
  }

  return it->second;
}

// Returns true if the command id is for opening a link.
bool IsCommandForOpenLink(int id) {
  return id == IDC_CONTENT_CONTEXT_OPENLINKNEWTAB ||
         id == IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW ||
         id == IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD ||
         (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
          id <= IDC_OPEN_LINK_IN_PROFILE_LAST);
}

// Returns the preference of the profile represented by the |context|.
PrefService* GetPrefs(content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context);
}

bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                           const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty()) {
    return true;
  }
  return patterns.MatchesURL(url);
}

content::Referrer CreateReferrer(const GURL& url,
                                 const content::ContextMenuParams& params) {
  const GURL& referring_url = params.frame_url;
  return content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
}

content::WebContents* GetWebContentsToUse(
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If we're viewing in a MimeHandlerViewGuest, use its embedder WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host);
  if (guest_view) {
    return guest_view->embedder_web_contents();
  }
#endif
  return content::WebContents::FromRenderFrameHost(render_frame_host);
}

bool g_custom_id_ranges_initialized = false;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void AddAvatarToLastMenuItem(const gfx::Image& icon,
                             ui::SimpleMenuModel* menu) {
  // Don't try to scale too small icons.
  if (icon.Width() < 16 || icon.Height() < 16) {
    return;
  }
  int target_dip_width = icon.Width();
  int target_dip_height = icon.Height();
  gfx::CalculateFaviconTargetSize(&target_dip_width, &target_dip_height);
  gfx::Image sized_icon = profiles::GetSizedAvatarIcon(
      icon, target_dip_width, target_dip_height, profiles::SHAPE_CIRCLE);
  menu->SetIcon(menu->GetItemCount() - 1,
                ui::ImageModel::FromImage(sized_icon));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void OnBrowserCreated(const GURL& link_url,
                      url::Origin initiator_origin,
                      Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }

  NavigateParams nav_params(
      browser, link_url,
      /* |ui::PAGE_TRANSITION_TYPED| is used rather than
         |ui::PAGE_TRANSITION_LINK| since this ultimately opens the link in
         another browser. This parameter is used within the tab strip model of
         the browser it opens in implying a link from the active tab in the
         destination browser which is not correct. */
      ui::PAGE_TRANSITION_TYPED);
  nav_params.initiator_origin = initiator_origin;
  nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  // We are opening the link across profiles, so sending the referer
  // header is a privacy risk.
  nav_params.referrer = content::Referrer();
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&nav_params);
}

bool DoesFormControlTypeSupportEmoji(
    blink::mojom::FormControlType form_control_type) {
  switch (form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    default:
      return false;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// If the link points to a system web app (in |profile|), return its type.
// Otherwise nullopt.
std::optional<ash::SystemWebAppType> GetLinkSystemAppType(Profile* profile,
                                                          const GURL& url) {
  std::optional<webapps::AppId> link_app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, url);

  if (!link_app_id) {
    return std::nullopt;
  }

  return ash::GetSystemWebAppTypeForAppId(profile, *link_app_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)

// Returns the string ID of the clipboard history menu option.
int GetClipboardHistoryStringId() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled()
             ? IDS_CONTEXT_MENU_PASTE_FROM_CLIPBOARD
             : IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU;
}

// Returns the command ID of the clipboard history menu option.
int GetClipboardHistoryCommandId() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled()
             ? IDC_CONTENT_PASTE_FROM_CLIPBOARD
             : IDC_CONTENT_CLIPBOARD_HISTORY_MENU;
}

bool IsCaptivePortalProfile(Profile* profile) {
  return chromeos::features::IsCaptivePortalPopupWindowEnabled() &&
         profile->IsOffTheRecord() &&
         profile->GetOTRProfileID().IsCaptivePortal();
}

#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsFrameInPdfViewer(content::RenderFrameHost* rfh) {
  if (!rfh) {
    return false;
  }

  if (IsPdfExtensionOrigin(rfh->GetLastCommittedOrigin())) {
    return true;
  }

  content::RenderFrameHost* parent_rfh = rfh->GetParent();
  return parent_rfh &&
         IsPdfExtensionOrigin(parent_rfh->GetLastCommittedOrigin());
}

Browser* FindNormalBrowser(const Profile* profile) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (auto it = browser_list->begin_browsers_ordered_by_activation();
       it != browser_list->end_browsers_ordered_by_activation(); ++it) {
    Browser* browser = *it;
    if (browser->is_type_normal() && browser->profile() == profile) {
      return browser;
    }
  }
  return nullptr;
}

#if BUILDFLAG(ENABLE_PDF)
// Returns true if the PDF viewer is handling the save, false otherwise.
bool MaybePdfViewerHandlesSave(RenderFrameHost* frame_host) {
  if (!chrome_pdf::features::IsOopifPdfEnabled() ||
      !IsFrameInPdfViewer(frame_host)) {
    return false;
  }

  // Get the PDF embedder host, either from the PDF extension host or from the
  // PDF content host.
  // If `frame_host` is the PDF extension host, then the parent host is the
  // embedder host. Otherwise, `frame_host` is the PDF content host.
  RenderFrameHost* embedder_host =
      IsPdfExtensionOrigin(frame_host->GetLastCommittedOrigin())
          ? frame_host->GetParent()
          : pdf_frame_util::GetEmbedderHost(frame_host);
  CHECK(embedder_host);

  return pdf_extension_util::MaybeDispatchSaveEvent(embedder_host);
}
#endif  // BUILDFLAG(ENABLE_PDF)

bool IsLensOptionEnteredThroughKeyboard(int event_flags) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This check must be done inside the BUILDFLAG block because
  // GetMenuSourceType is only available in this case.
  return ui::GetMenuSourceType(event_flags) == ui::MENU_SOURCE_KEYBOARD;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

ToastController* GetToastController(Browser* browser) {
  // Browser can sometimes be undefined in tests or when trying to open the
  // context menu on a non-tab WebContents.
  return browser ? browser->GetFeatures().toast_controller() : nullptr;
}
}  // namespace

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

// static
void RenderViewContextMenu::AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                                     bool is_checked) {
  if (is_checked) {
    menu->AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                                   IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  } else {
    menu->AddItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                              IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RenderViewContextMenu,
                                      kExitFullscreenMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RenderViewContextMenu, kComposeMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RenderViewContextMenu, kRegionSearchItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RenderViewContextMenu,
                                      kSearchForImageItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(RenderViewContextMenu,
                                      kSearchForVideoFrameItem);

RenderViewContextMenu::RenderViewContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuBase(render_frame_host, params),
      extension_items_(browser_context_,
                       this,
                       &menu_model_,
                       base::BindRepeating(MenuItemMatchesParams, params_)),
      current_url_(render_frame_host.GetLastCommittedURL()),
      main_frame_url_(render_frame_host.GetMainFrame()->GetLastCommittedURL()),
      profile_link_submenu_model_(this),
      multiple_profiles_open_(false),
      protocol_handler_submenu_model_(this),
      protocol_handler_registry_(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile())),
      accessibility_labels_submenu_model_(this),
      embedder_web_contents_(GetWebContentsToUse(&render_frame_host)),
      autofill_context_menu_manager_(
          autofill::PersonalDataManagerFactory::GetForBrowserContext(
              GetProfile()),
          this,
          &menu_model_) {
  if (!g_custom_id_ranges_initialized) {
    g_custom_id_ranges_initialized = true;
    SetContentCustomCommandIdRange(IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
                                   IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  }
  set_content_type(
      ContextMenuContentTypeFactory::Create(&render_frame_host, params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  system_app_ = GetBrowser() && GetBrowser()->app_controller()
                    ? GetBrowser()->app_controller()->system_app()
                    : nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  observers_.AddObserver(&autofill_context_menu_manager_);
}

RenderViewContextMenu::~RenderViewContextMenu() = default;

// Menu construction functions -------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool RenderViewContextMenu::ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    const MenuItem::ContextList& contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_subframe = params.is_subframe;

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_subframe && contexts.Contains(MenuItem::FRAME))) {
    return true;
  }

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url)) {
    return true;
  }

  switch (params.media_type) {
    case ContextMenuDataMediaType::kImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    case ContextMenuDataMediaType::kVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    case ContextMenuDataMediaType::kAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == ContextMenuDataMediaType::kNone &&
      contexts.Contains(MenuItem::PAGE)) {
    return true;
  }

  return false;
}

// static
bool RenderViewContextMenu::MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match) {
    return false;
  }

  return ExtensionPatternMatch(item->document_url_patterns(), params.frame_url);
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_items_.Clear();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context_);

  MenuManager* menu_manager = MenuManager::Get(browser_context_);
  if (!menu_manager) {
    return;
  }

  std::u16string printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::vector<std::u16string> sorted_menu_titles;
  std::map<std::u16string, std::vector<const Extension*>>
      title_to_extensions_map;
  for (const auto& id : menu_manager->ExtensionIds()) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(id.extension_id);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      std::u16string menu_title = extension_items_.GetTopLevelContextMenuTitle(
          id, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty()) {
    return;
  }

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  for (const auto& title : sorted_menu_titles) {
    const std::vector<const Extension*>& extensions =
        title_to_extensions_map[title];
    for (const Extension* extension : extensions) {
      MenuItem::ExtensionKey extension_key(extension->id());
      extension_items_.AppendExtensionItems(extension_key,
                                            printable_selection_text, &index,
                                            /*is_action_menu=*/false);
    }
  }
}

void RenderViewContextMenu::AppendCurrentExtensionItems() {
  // Avoid appending extension related items when |extension| is null.
  // For Panel, this happens when the panel is navigated to a url outside of the
  // extension's package.
  const Extension* extension = GetExtension();
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id_,
                                           render_frame_id_));
  MenuItem::ExtensionKey key;
  std::u16string title;
  if (web_view_guest) {
    const std::string& extension_id =
        extension ? extension->id() : std::string();
    title = extension ? base::UTF8ToUTF16(extension->name())
                      : web_view_guest->owner_web_contents()->GetTitle();
    key = MenuItem::ExtensionKey(
        extension_id, web_view_guest->owner_rfh()->GetProcess()->GetID(),
        web_view_guest->owner_rfh()->GetRoutingID(),
        web_view_guest->view_instance_id());
  } else {
    key = MenuItem::ExtensionKey(extension->id());
  }

  // Only add extension items from this extension.
  int index = 0;
  extension_items_.AppendExtensionItems(key, PrintableSelectionText(), &index,
                                        /*is_action_menu=*/false, title);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::u16string RenderViewContextMenu::FormatURLForClipboard(const GURL& url) {
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());

  GURL url_to_format = url;
  url_formatter::FormatUrlTypes format_types;
  base::UnescapeRule::Type unescape_rules;
  if (url.SchemeIs(url::kMailToScheme)) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    url_to_format = url.ReplaceComponents(replacements);
    format_types = url_formatter::kFormatUrlOmitMailToScheme;
    unescape_rules =
        base::UnescapeRule::PATH_SEPARATORS |
        base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS;
  } else {
    format_types = url_formatter::kFormatUrlOmitNothing;
    unescape_rules = base::UnescapeRule::NONE;
  }

  return url_formatter::FormatUrl(url_to_format, format_types, unescape_rules,
                                  nullptr, nullptr, nullptr);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  if (url.is_empty() || !url.is_valid()) {
    return;
  }

  ui::ScopedClipboardWriter scw(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/true));
  scw.SetDataSourceURL(main_frame_url_, current_url_);
  scw.WriteText(FormatURLForClipboard(url));
}

void RenderViewContextMenu::IssuePreconnectionToUrl(
    const std::string& anonymization_key_url,
    const std::string& preconnect_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (prefetch::IsSomePreloadingEnabled(*profile->GetPrefs()) !=
      content::PreloadingEligibility::kEligible) {
    return;
  }

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (!loading_predictor) {
    return;
  }

  GURL anonymization_key_gurl(anonymization_key_url);
  net::SchemefulSite anonymization_key_schemeful_site(anonymization_key_gurl);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateCrossSite(
          anonymization_key_schemeful_site);
  loading_predictor->PreconnectURLIfAllowed(GURL(preconnect_url),
                                            /*allow_credentials=*/true,
                                            network_anonymization_key);
}

bool RenderViewContextMenu::IsInProgressiveWebApp() const {
  const Browser* browser = GetBrowser();
  return browser && (browser->is_type_app() || browser->is_type_app_popup());
}

void RenderViewContextMenu::InitMenu() {
  RenderViewContextMenuBase::InitMenu();

  AppendPasswordItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE)) {
    AppendPageItems();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendLinkItems();
    if (params_.media_type != ContextMenuDataMediaType::kNone) {
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }

  bool media_image = content_type_->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE);
  if (media_image) {
    AppendImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCHWEBFORIMAGE)) {
    AppendSearchWebForImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    AppendVideoItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO)) {
    AppendAudioItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_CANVAS)) {
    AppendCanvasItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendPluginItems();
  }

  // ITEM_GROUP_MEDIA_FILE has no specific items.

  bool editable =
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_EDITABLE);

  if (editable) {
    AppendSpellingAndSearchSuggestionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_AUTOFILL)) {
    autofill_context_menu_manager_.AppendItems();
  }

  if (editable) {
    AppendOtherEditableItems();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY)) {
    DCHECK(!editable);
    AppendCopyItem();
    AppendLinkToTextItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_EXISTING_LINK_TO_TEXT)) {
    AppendLinkToTextItems();
  }

  if (!content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendSharingItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER) &&
      params_.misspelled_word.empty() &&
      (params_.page_url != GetGooglePasswordManagerSubPageURLStr() &&
       params_.page_url != chrome::kChromeUIPasswordManagerCheckupURL &&
       params_.page_url != chrome::kChromeUIPasswordManagerSettingsURL)) {
    AppendSearchProvider();
  }

  if (!media_image &&
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)) {
    AppendPrintItem();
  }

  // ITEM_GROUP_SMART_SELECTION is for selected text that is not a link.
  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SMART_SELECTION)) {
    AppendReadingModeItem();
  }

  // Partial Translate is not supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PARTIAL_TRANSLATE) &&
      search::DefaultSearchProviderIsGoogle(GetProfile()) &&
      CanTranslate(/*menu_logging=*/false)) {
    // If the target language isn't supported in partial translation, fall
    // back to showing the full page translate menu item. Partial translate
    // uses a different backend that supports a subset of translation
    // languages, so the current full page target language may not be
    // supported.
    if (CanPartiallyTranslateTargetLanguage()) {
      AppendPartialTranslateItem();
    } else {
      AppendTranslateItem();
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Spell check and writing direction options are not currently supported by
  // pepper plugins.
  if (editable && params_.misspelled_word.empty() &&
      !content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    AppendLanguageSettings();
    AppendPlatformEditableItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendRotationItems();
  }

  bool supports_smart_text_selection = false;
#if BUILDFLAG(IS_CHROMEOS)
  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SMART_SELECTION)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    supports_smart_text_selection =
        arc::IsArcPlayStoreEnabledForProfile(GetProfile());
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* service = chromeos::LacrosService::Get();
    // AppService and ARC are not supporting non-primary profile.
    // Also check if Lacros supports ARC.
    supports_smart_text_selection = GetProfile()->IsMainProfile() && service &&
                                    service->IsAvailable<crosapi::mojom::Arc>();
#endif
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (supports_smart_text_selection) {
    AppendSmartSelectionActionItems();
  }

  extension_items_.set_smart_text_selection_enabled(
      supports_smart_text_selection);

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
        ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
    AppendAllExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
        ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    AppendCurrentExtensionItems();
  }

  // Accessibility label items are appended to all menus (with the exception of
  // within the dev tools and PDF Viewer) when a screen reader is enabled. It
  // can be difficult to open a specific context menu with a screen reader, so
  // this is a UX approved solution.
  bool added_accessibility_labels_items = false;
  if (!IsDevToolsURL(params_.page_url) &&
      !IsFrameInPdfViewer(GetRenderFrameHost())) {
    added_accessibility_labels_items = AppendAccessibilityLabelsItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVELOPER)) {
    AppendDeveloperItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVTOOLS_UNPACKED_EXT)) {
    AppendDevtoolsForUnpackedExtensions();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PRINT_PREVIEW)) {
    AppendPrintPreviewItems();
  }

  // Remove any redundant trailing separator.
  size_t count = menu_model_.GetItemCount();
  if (count > 0 &&
      menu_model_.GetTypeAt(count - 1) == ui::MenuModel::TYPE_SEPARATOR) {
    menu_model_.RemoveItemAt(count - 1);
  }

  // If there is only one item and it is the Accessibility labels item, remove
  // it. We only show this item when it is not the only item.
  // Note that the separator added in AppendAccessibilityLabelsItems will not
  // actually be added if this is the first item in the list, so we don't need
  // to check for or remove the initial separator.
  if (added_accessibility_labels_items && menu_model_.GetItemCount() == 1) {
    menu_model_.RemoveItemAt(0);
  }

  // Always add read write cards UI last, as it is rendered next to the context
  // menu, meaning that each menu item added/removed in this function will cause
  // it to visibly jump on the screen (see b/173569669).
  AppendReadWriteCardItems();

  // If the autofill popup is shown the context menu could
  // overlap with the autofill popup, therefore we hide the autofill popup.
  content::WebContents* web_contents = GetWebContents();
  autofill::AutofillClient* autofill_client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);
  if (autofill_client) {
    autofill_client->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kContextMenuOpened);
  }
}

Profile* RenderViewContextMenu::GetProfile() const {
  return Profile::FromBrowserContext(browser_context_);
}

int RenderViewContextMenu::GetSearchForImageIdc() const {
  if (base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
      search::DefaultSearchProviderIsGoogle(GetProfile())) {
    return IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE;
  }
  return IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE;
}

int RenderViewContextMenu::GetTranslateImageIdc() const {
  if (base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
      search::DefaultSearchProviderIsGoogle(GetProfile())) {
    return IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS;
  }
  return IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB;
}

int RenderViewContextMenu::GetRegionSearchIdc() const {
  return search::DefaultSearchProviderIsGoogle(GetProfile())
             ? IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH
             : IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH;
}

int RenderViewContextMenu::GetSearchForVideoFrameIdc() const {
  return search::DefaultSearchProviderIsGoogle(GetProfile())
             ? IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME
             : IDC_CONTENT_CONTEXT_SEARCHWEBFORVIDEOFRAME;
}

const TemplateURL* RenderViewContextMenu::GetImageSearchProvider() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!crosapi::browser_util::IsAshWebBrowserEnabled()) {
    // If Lacros is the only browser, disable region search in Ash because we
    // have decided not to support this feature in the system UI so as not to
    // confuse users by opening an Ash browser window.
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!GetBrowser()) {
    return nullptr;
  }

  // TODO(b/266624865): Image Search items do not function correctly when
  // |GetBrowser| returns nullptr, as is the case for a context menu in the
  // side panel, so for now we do not append image items in that case.
  // TODO(nguyenbryan): Refactor to use lens_region_search_helper.cc after PDF
  // support is cleaned up.
  auto* service = TemplateURLServiceFactory::GetForProfile(GetProfile());
  if (!service) {
    return nullptr;
  }

  const TemplateURL* provider = service->GetDefaultSearchProvider();
  if (!provider) {
    return nullptr;
  }

  if (provider->image_url().empty() ||
      !provider->image_url_ref().IsValid(service->search_terms_data())) {
    return nullptr;
  }

  return provider;
}

std::u16string RenderViewContextMenu::GetImageSearchProviderName(
    const TemplateURL* provider) const {
  if (search::DefaultSearchProviderIsGoogle(GetProfile())) {
    // The image search branding label should always be 'Google'.
    return provider->short_name();
  }

  // image_search_branding_label() returns the provider short name if no
  // image_search_branding_label is set.
  return provider->image_search_branding_label();
}

void RenderViewContextMenu::RecordUsedItem(int id) {
  // Log general ID.

  int enum_id =
      FindUMAEnumValueForCommand(id, UmaEnumIdLookupType::GeneralEnumId);
  if (enum_id == -1) {
    NOTREACHED_IN_MIGRATION() << "Update GetIdcToUmaMap. Unhandled IDC: " << id;
    return;
  }

  UMA_HISTOGRAM_EXACT_LINEAR(
      "RenderViewContextMenu.Used", enum_id,
      GetUmaValueMax(UmaEnumIdLookupType::GeneralEnumId));

  // Log a user action for the SEARCHWEBFOR case. This value is used as part of
  // a high-level guiding metric, which is being migrated to user actions.
  if (id == IDC_CONTENT_CONTEXT_SEARCHWEBFOR) {
    base::RecordAction(base::UserMetricsAction(
        "RenderViewContextMenu.Used.IDC_CONTENT_CONTEXT_SEARCHWEBFOR"));
  }

  // Log other situations.

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK) &&
      // Ignore link-related commands that don't actually open a link.
      IsCommandForOpenLink(id) &&
      // Ignore using right click + open in new tab for internal links.
      !params_.link_url.SchemeIs(content::kChromeUIScheme)) {
    const GURL doc_url = params_.frame_url;
    const GURL history_url = GURL(chrome::kChromeUIHistoryURL);
    if (doc_url == history_url.Resolve(chrome::kChromeUIHistorySyncedTabs)) {
      UMA_HISTOGRAM_ENUMERATION(
          "HistoryPage.OtherDevicesMenu",
          browser_sync::SyncedTabsHistogram::OPENED_LINK_VIA_CONTEXT_MENU,
          browser_sync::SyncedTabsHistogram::LIMIT);
    } else if (doc_url == GURL(chrome::kChromeUIDownloadsURL)) {
      base::RecordAction(base::UserMetricsAction(
          "Downloads_OpenUrlOfDownloadedItemFromContextMenu"));
    } else if (doc_url.DeprecatedGetOriginAsURL() ==
               chrome::kChromeSearchMostVisitedUrl) {
      base::RecordAction(
          base::UserMetricsAction("MostVisited_ClickedFromContextMenu"));
    } else if (doc_url.DeprecatedGetOriginAsURL() ==
                   GURL(chrome::kChromeUINewTabPageURL) ||
               doc_url.DeprecatedGetOriginAsURL() ==
                   GURL(chrome::kChromeUIUntrustedNewTabPageUrl)) {
      base::RecordAction(base::UserMetricsAction(
          "NewTabPage.LinkOpenedFromContextMenu.WebUI"));
    }
  }

  // Log UKM for Lens context menu items.
  if (id == IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH ||
      id == IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE ||
      id == IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS) {
    // Enum id should correspond to the RenderViewContextMenuItem enum.
    ukm::SourceId source_id =
        source_web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();
    ukm::builders::RenderViewContextMenu_Used(source_id)
        .SetSelectedMenuItem(enum_id)
        .Record(ukm::UkmRecorder::Get());
  }

  // Log for specific contexts. Note that since the menu is displayed for
  // contexts (all of the ContextMenuContentType::SupportsXXX() functions),
  // these UMA metrics are necessarily best-effort in distilling into a context.

  enum_id = FindUMAEnumValueForCommand(
      id, UmaEnumIdLookupType::ContextSpecificEnumId);
  if (enum_id == -1) {
    return;
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Video", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_LINK) &&
             content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.ImageLink", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (content_type_->SupportsGroup(
                 ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Image", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if (!params_.misspelled_word.empty()) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.MisspelledWord", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else if ((!params_.selection_text.empty() ||
              params_.opened_from_highlight) &&
             params_.media_type == ContextMenuDataMediaType::kNone) {
    // Probably just text.
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.SelectedText", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContextMenu.SelectedOptionDesktop.Other", enum_id,
        GetUmaValueMax(UmaEnumIdLookupType::ContextSpecificEnumId));
  }
}

void RenderViewContextMenu::RecordShownItem(int id, bool is_submenu) {
  // The "RenderViewContextMenu.Shown" histogram is not recorded for submenus.
  if (!is_submenu) {
    int enum_id =
        FindUMAEnumValueForCommand(id, UmaEnumIdLookupType::GeneralEnumId);
    if (enum_id != -1) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "RenderViewContextMenu.Shown", enum_id,
          GetUmaValueMax(UmaEnumIdLookupType::GeneralEnumId));
    } else {
      // Just warning here. It's harder to maintain list of all possibly
      // visible items than executable items.
      DLOG(ERROR) << "Update GetIdcToUmaMap. Unhandled IDC: " << id;
    }
  }

  // The "Open Link as Profile" item can either be shown directly in the main
  // menu as an item or as a sub-menu. The metric needs to track the
  // impressions in the main menu, which are
  // IDC_CONTENT_CONTEXT_OPENLINKINPROFILE when there is a sub-menu, and
  // IDC_OPEN_LINK_IN_PROFILE_FIRST when there is not.
  // IDC_OPEN_LINK_IN_PROFILE_FIRST is also emitted when the sub-menu is
  // opened, so it is not taken into account when the sub-menu exists.
  if (id == IDC_CONTENT_CONTEXT_OPENLINKINPROFILE ||
      (id == IDC_OPEN_LINK_IN_PROFILE_FIRST &&
       profile_link_submenu_model_.GetItemCount() == 0)) {
    base::UmaHistogramEnumeration(kOpenLinkAsProfileHistogram,
                                  OpenLinkAs::kOpenLinkAsProfileDisplayed);
  } else if (id == IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD &&
             IsOpenLinkOTREnabled()) {
    base::UmaHistogramEnumeration(kOpenLinkAsProfileHistogram,
                                  OpenLinkAs::kOpenLinkAsIncognitoDisplayed);
  }
}

bool RenderViewContextMenu::IsHTML5Fullscreen() const {
  Browser* browser = chrome::FindBrowserWithTab(embedder_web_contents_);
  if (!browser) {
    return false;
  }

  FullscreenController* controller =
      browser->exclusive_access_manager()->fullscreen_controller();
  return controller->IsTabFullscreen();
}

bool RenderViewContextMenu::IsPressAndHoldEscRequiredToExitFullscreen() const {
  Browser* browser = chrome::FindBrowserWithTab(source_web_contents_);
  if (!browser) {
    return false;
  }

  KeyboardLockController* controller =
      browser->exclusive_access_manager()->keyboard_lock_controller();
  return controller->RequiresPressAndHoldEscToExit();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderViewContextMenu::HandleAuthorizeAllPlugins() {
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      source_web_contents_, false, std::string());
}
#endif

void RenderViewContextMenu::AppendPrintPreviewItems() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_menu_observer_) {
    print_preview_menu_observer_ =
        std::make_unique<PrintPreviewContextMenuObserver>(source_web_contents_);
  }

  observers_.AddObserver(print_preview_menu_observer_.get());
#endif
}

const Extension* RenderViewContextMenu::GetExtension() const {
  return extensions::ProcessManager::Get(browser_context_)
      ->GetExtensionForWebContents(source_web_contents_);
}

std::u16string RenderViewContextMenu::GetTargetLanguageDisplayName(
    bool is_full_page_translation) const {
  std::string source;
  std::string target;

  ChromeTranslateClient::FromWebContents(embedder_web_contents_)
      ->GetTranslateLanguages(embedder_web_contents_, &source, &target,
                              is_full_page_translation);
  return l10n_util::GetDisplayNameForLocale(target, target, true);
}

#if BUILDFLAG(ENABLE_COMPOSE)
ChromeComposeClient* RenderViewContextMenu::GetChromeComposeClient() const {
  return ChromeComposeClient::FromWebContents(source_web_contents_);
}
#endif  // BUILDFLAG(ENABLE_COMPOSE)

void RenderViewContextMenu::AppendDeveloperItems() {
  // Do not Show Inspect Element for DevTools unless DevTools runs with the
  // debugFrontend query param.
  bool hide_developer_items =
      IsDevToolsURL(params_.page_url) &&
      params_.page_url.query().find("debugFrontend=true") == std::string::npos;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  hide_developer_items =
      hide_developer_items || !crosapi::browser_util::IsAshDevToolEnabled();
#endif

  if (hide_developer_items) {
    return;
  }

  // In the DevTools popup menu, "developer items" is normally the only
  // section, so omit the separator there.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE)) {
    menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  }
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_FRAME)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOADFRAME,
                                    IDS_CONTENT_CONTEXT_RELOADFRAME);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                  IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendDevtoolsForUnpackedExtensions() {
  // Add a separator if there are any items already in the menu.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RESTART_APP);
  AppendDeveloperItems();
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                  IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
}

void RenderViewContextMenu::AppendLinkItems() {
  if (!params_.link_url.is_empty()) {
    const bool in_app = IsInProgressiveWebApp();

    bool show_open_in_new_tab = true;
    bool show_open_in_new_window = true;
    bool show_open_link_off_the_record = true;

    if (in_app) {
      show_open_in_new_window = false;
    }

#if BUILDFLAG(IS_CHROMEOS)
    Profile* profile = GetProfile();

    // Disable opening links in a new tab or window for captive portal signin.
    if (IsCaptivePortalProfile(profile)) {
      show_open_in_new_tab = false;
      show_open_in_new_window = false;
      show_open_link_off_the_record = false;
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    const bool in_system_web_dialog =
        ash::SystemWebDialogDelegate::HasInstance(current_url_);

    std::optional<ash::SystemWebAppType> link_system_app_type =
        GetLinkSystemAppType(profile, params_.link_url);

    // true if the link points to a WebUI page, including SWA.
    const bool link_to_webui = content::HasWebUIScheme(params_.link_url);

    // Opening a WebUI page in an incognito window makes little sense, so we
    // don't show the item.
    if (link_to_webui) {
      show_open_link_off_the_record = false;
    }

    // Basically, we don't show "Open link in new tab" and "Open link in new
    // window" items inside SWAs/SystemWebDialogs if that link is to WebUI.
    if ((system_app_ || in_system_web_dialog) && link_to_webui) {
      // We don't show "Open in new tab" if the current app doesn't have a tab
      // strip.
      //
      // Even if the app has a tab strip, we don't show the item for
      // links to a different SWA, because two SWAs can't share the same browser
      // window.
      if (in_system_web_dialog || !system_app_->ShouldHaveTabStrip() ||
          system_app_->GetType() != link_system_app_type) {
        show_open_in_new_tab = false;
      }

      // Don't show "open in new window", this is instead handled below in
      // |AppendOpenInWebAppLinkItems| (which includes app's name and icon).
      show_open_in_new_window = false;
    }

    // If the current browser is a system app or a SystemWebDialog, hide "Open
    // link in ..." items on button-like links i.e. links that have a href='#'.
    // Since most of those links are used to do something in their JavaScript
    // click handlers, opening '#' links in another browser tab/window makes
    // little sense.
    const bool button_like_link =
        current_url_.EqualsIgnoringRef(params_.link_url) &&
        params_.link_url.has_ref() && params_.link_url.ref().empty();
    if ((system_app_ || in_system_web_dialog) && button_like_link) {
      show_open_in_new_tab = false;
      show_open_in_new_window = false;
      show_open_link_off_the_record = false;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS)

    if (show_open_in_new_tab) {
      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
          in_app ? IDS_CONTENT_CONTEXT_OPENLINKNEWTAB_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
    }

    if (show_open_in_new_window) {
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                      IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
    }

    if (params_.link_url.is_valid()) {
      AppendProtocolHandlerSubMenu();
    }

    if (show_open_link_off_the_record) {
      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
          in_app ? IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    }

#if !BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(blink::features::kLinkPreview)) {
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW,
                                      IDS_CONTENT_CONTEXT_OPENLINKPREVIEW);
      // We don't show in-production-help for ChromeOS for now because we should
      // use a different trigger.
      //
      // TODO(b:325390312): Update trigger for ChromeOS and show
      // in-production-help.
#if !BUILDFLAG(IS_CHROMEOS)
      int string_id;
      switch (blink::features::kLinkPreviewTriggerType.Get()) {
        case blink::features::LinkPreviewTriggerType::kAltClick:
          string_id = IDS_CONTENT_CONTEXT_OPENLINKPREVIEW_TRIGGER_ALTCLICK;
          break;
        case blink::features::LinkPreviewTriggerType::kAltHover:
          string_id = IDS_CONTENT_CONTEXT_OPENLINKPREVIEW_TRIGGER_ALTHOVER;
          break;
        case blink::features::LinkPreviewTriggerType::kLongPress:
          string_id = IDS_CONTENT_CONTEXT_OPENLINKPREVIEW_TRIGGER_LONGPRESS;
          break;
      }
      menu_model_.SetMinorText(menu_model_.GetItemCount() - 1,
                               l10n_util::GetStringUTF16(string_id));
#endif  // !BUILDFLAG(IS_CHROMEOS)
    }
#endif  // !BUILDFLAG(IS_ANDROID)

    AppendOpenInWebAppLinkItems();
    AppendOpenWithLinkItems();

    // ChromeOS ASH supports multiple profiles, but only one can be open at a
    // time. With LaCrOS, profile switching is enabled.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // g_browser_process->profile_manager() is null during unit tests.
    if (g_browser_process->profile_manager() &&
        !GetProfile()->IsOffTheRecord()) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      // Find all regular profiles other than the current one which have at
      // least one open window.
      std::vector<ProfileAttributesEntry*> entries =
          profile_manager->GetProfileAttributesStorage()
              .GetAllProfilesAttributesSortedByNameWithCheck();
      std::vector<ProfileAttributesEntry*> target_profiles_entries;
      bool has_active_profiles = false;
      for (ProfileAttributesEntry* entry : entries) {
        base::FilePath profile_path = entry->GetPath();
        Profile* profile_for_path =
            profile_manager->GetProfileByPath(profile_path);
        if (profile_for_path != GetProfile() && !entry->IsOmitted() &&
            !entry->IsSigninRequired()) {
          target_profiles_entries.push_back(entry);
          if (chrome::FindLastActiveWithProfile(profile_for_path)) {
            multiple_profiles_open_ = true;
          }
          if (ProfileMetrics::IsProfileActive(entry)) {
            has_active_profiles = true;
          }
        }
      }

      if (multiple_profiles_open_ || has_active_profiles) {
        DCHECK(!target_profiles_entries.empty());
        profiles::PlaceholderAvatarIconParams icon_params =
            GetPlaceholderAvatarIconParamsVisibleAgainstColor(
                GetWebContents()->GetColorProvider().GetColor(
                    ui::kColorMenuBackground));
        if (target_profiles_entries.size() == 1) {
          int menu_index = static_cast<int>(profile_link_paths_.size());
          ProfileAttributesEntry* entry = target_profiles_entries.front();
          profile_link_paths_.push_back(entry->GetPath());
          menu_model_.AddItem(
              IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index,
              l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_OPENLINKINPROFILE,
                                         entry->GetName()));
          AddAvatarToLastMenuItem(
              entry->GetAvatarIcon(kDefaultSizeForPlaceholderAvatar,
                                   /*use_high_res_file=*/true, icon_params),
              &menu_model_);
        } else {
          for (ProfileAttributesEntry* entry : target_profiles_entries) {
            int menu_index = static_cast<int>(profile_link_paths_.size());
            // In extreme cases, we might have more profiles than available
            // command ids. In that case, just stop creating new entries - the
            // menu is probably useless at this point already.
            if (IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index >
                IDC_OPEN_LINK_IN_PROFILE_LAST) {
              break;
            }
            profile_link_paths_.push_back(entry->GetPath());
            profile_link_submenu_model_.AddItem(
                IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index, entry->GetName());
            AddAvatarToLastMenuItem(
                entry->GetAvatarIcon(kDefaultSizeForPlaceholderAvatar,
                                     /*use_high_res_file=*/true, icon_params),
                &profile_link_submenu_model_);
          }
          menu_model_.AddSubMenuWithStringId(
              IDC_CONTENT_CONTEXT_OPENLINKINPROFILE,
              IDS_CONTENT_CONTEXT_OPENLINKINPROFILES,
              &profile_link_submenu_model_);
        }
      }
    }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

    // Place QR Generator close to send-tab-to-self feature for link images.
    if (params_.has_image_contents) {
      AppendQRCodeGeneratorItem(/*for_image=*/true, /*draw_icon=*/true,
                                /*add_separator*/ false);
    }

    AppendClickToCallItem();

    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
                                  params_.link_url.SchemeIs(url::kMailToScheme)
                                      ? IDS_CONTENT_CONTEXT_COPYEMAILADDRESS
                                      : IDS_CONTENT_CONTEXT_COPYLINKLOCATION);

  if (params_.source_type == ui::MENU_SOURCE_TOUCH &&
      params_.media_type != ContextMenuDataMediaType::kImage &&
      !params_.link_text.empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKTEXT,
                                    IDS_CONTENT_CONTEXT_COPYLINKTEXT);
  }
}

void RenderViewContextMenu::AppendOpenWithLinkItems() {
#if BUILDFLAG(IS_CHROMEOS)
  open_with_menu_observer_ =
      std::make_unique<arc::OpenWithMenu>(browser_context_, this);
  observers_.AddObserver(open_with_menu_observer_.get());
  open_with_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendReadWriteCardItems() {
#if BUILDFLAG(IS_CHROMEOS)
  if (!read_write_card_observer_) {
    read_write_card_observer_ =
        std::make_unique<ReadWriteCardObserver>(this, GetProfile());
  }

  observers_.AddObserver(read_write_card_observer_.get());
  read_write_card_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendSmartSelectionActionItems() {
#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  using ArcIntentHelperMojoDelegate = arc::ArcIntentHelperMojoAsh;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS_
  using ArcIntentHelperMojoDelegate = arc::ArcIntentHelperMojoLacros;
#endif
  start_smart_selection_action_menu_observer_ =
      std::make_unique<arc::StartSmartSelectionActionMenu>(
          browser_context_, this,
          std::make_unique<ArcIntentHelperMojoDelegate>());
  observers_.AddObserver(start_smart_selection_action_menu_observer_.get());

  if (menu_model_.GetItemCount()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
  start_smart_selection_action_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendOpenInWebAppLinkItems() {
  Profile* const profile = Profile::FromBrowserContext(browser_context_);
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }

  auto* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return;
  }

  std::optional<webapps::AppId> link_app_id =
      web_app::FindInstalledAppWithUrlInScope(profile, params_.link_url);
  if (!link_app_id) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't show "Open link in new app window", if the link points to the
  // current app, and the app would reuse an existing window.
  if (system_app_ &&
      system_app_->GetType() ==
          ash::GetSystemWebAppTypeForAppId(profile, *link_app_id) &&
      system_app_->GetWindowForLaunch(profile, params_.link_url) != nullptr) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Only applies to apps that open in an app window.
  if (provider->registrar_unsafe().GetAppUserDisplayMode(*link_app_id) ==
      web_app::mojom::UserDisplayMode::kBrowser) {
    return;
  }

  int open_in_app_string_id;
  const Browser* browser = GetBrowser();
  if (browser && browser->app_name() ==
                     web_app::GenerateApplicationNameFromAppId(*link_app_id)) {
    if (provider->registrar_unsafe().IsTabbedWindowModeEnabled(*link_app_id)) {
      open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKWEBAPP_NEWTAB;
    } else {
      open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP_SAMEAPP;
    }
  } else {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP;
  }

  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
      l10n_util::GetStringFUTF16(
          open_in_app_string_id,
          base::UTF8ToUTF16(
              provider->registrar_unsafe().GetAppShortName(*link_app_id))));

  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      provider->icon_manager().GetFavicon(*link_app_id));
  menu_model_.SetIcon(menu_model_.GetItemCount() - 1,
                      ui::ImageModel::FromImage(icon));
}

void RenderViewContextMenu::AppendImageItems() {
  if (!params_.has_image_contents) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LOAD_IMAGE,
                                    IDS_CONTENT_CONTEXT_LOAD_IMAGE);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                  IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);

  // Don't double-add for linked images, which also add the item.
  if (params_.link_url.is_empty()) {
    AppendQRCodeGeneratorItem(/*for_image=*/true, /*draw_icon=*/false,
                              /*add_separator=*/false);
  }
}

void RenderViewContextMenu::AppendSearchWebForImageItems() {
  if (!params_.has_image_contents) {
    return;
  }

  const auto* provider = GetImageSearchProvider();
  if (!provider) {
    return;
  }

  const int search_for_image_idc = GetSearchForImageIdc();
  if (GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled() &&
      lens::features::UseLensOverlayForImageSearch()) {
    const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleLensMonochromeLogoIcon;
#else
        vector_icons::kSearchChromeRefreshIcon;
#endif
    menu_model_.AddItemWithStringIdAndIcon(
        search_for_image_idc, IDS_CONTENT_CONTEXT_LENS_OVERLAY,
        ui::ImageModel::FromVectorIcon(icon));

    // TODO(b/344600237): Remove when image search using Lens overlay is not new
    // anymore.
    menu_model_.SetIsNewFeatureAt(
        menu_model_.GetItemCount() - 1,
        UserEducationService::MaybeShowNewBadge(GetBrowserContext(),
                                                lens::features::kLensOverlay));
  } else {
    menu_model_.AddItem(
        search_for_image_idc,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHLENSFORIMAGE,
                                   provider->short_name()));
  }
  const int command_index =
      menu_model_.GetIndexOfCommandId(search_for_image_idc).value();
  menu_model_.SetElementIdentifierAt(command_index, kSearchForImageItem);

  MaybePrepareForLensQuery();

  auto* service = TemplateURLServiceFactory::GetForProfile(GetProfile());

  if (base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
      base::FeatureList::IsEnabled(lens::features::kEnableImageTranslate) &&
      provider && !provider->image_translate_url().empty() &&
      provider->image_translate_url_ref().IsValid(
          service->search_terms_data())) {
    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(embedder_web_contents_);
    if (chrome_translate_client &&
        chrome_translate_client->GetLanguageState().IsPageTranslated()) {
      menu_model_.AddItem(
          GetTranslateImageIdc(),
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATEIMAGE,
                                     GetImageSearchProviderName(provider)));
    }
  }
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendCanvasItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
  if (base::FeatureList::IsEnabled(media::kContextMenuSaveVideoFrameAs)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS,
                                    IDS_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  if (base::FeatureList::IsEnabled(media::kContextMenuCopyVideoFrame)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYVIDEOFRAME,
                                    IDS_CONTENT_CONTEXT_COPYVIDEOFRAME);
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_PICTUREINPICTURE,
                                       IDS_CONTENT_CONTEXT_PICTUREINPICTURE);
  AppendMediaRouterItem();

  // Search for video frame menu item.
  if (base::FeatureList::IsEnabled(media::kContextMenuSearchForVideoFrame)) {
    const int search_for_video_frame_idc = GetSearchForVideoFrameIdc();

    if (GetBrowser()
            ->GetFeatures()
            .lens_overlay_entry_point_controller()
            ->IsEnabled() &&
        lens::features::UseLensOverlayForVideoFrameSearch()) {
      const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          vector_icons::kGoogleLensMonochromeLogoIcon;
#else
          vector_icons::kSearchChromeRefreshIcon;
#endif
      menu_model_.AddItemWithStringIdAndIcon(
          search_for_video_frame_idc, IDS_CONTENT_CONTEXT_LENS_OVERLAY,
          ui::ImageModel::FromVectorIcon(icon));

      // TODO(b/344600237): Remove when video frame search using Lens
      // overlay is not new anymore.
      menu_model_.SetIsNewFeatureAt(
          menu_model_.GetItemCount() - 1,
          UserEducationService::MaybeShowNewBadge(
              GetBrowserContext(), lens::features::kLensOverlay));
    } else {
      const auto* provider = GetImageSearchProvider();
      if (!provider) {
        return;
      }

      menu_model_.AddItem(
          search_for_video_frame_idc,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHFORVIDEOFRAME,
                                     GetImageSearchProviderName(provider)));
    }

    // Used for interactive tests. See LensOverlayControllerCUJTest.
    const int command_index =
        menu_model_.GetIndexOfCommandId(search_for_video_frame_idc).value();
    menu_model_.SetElementIdentifierAt(command_index, kSearchForVideoFrameItem);

    MaybePrepareForLensQuery();
  }
}

void RenderViewContextMenu::AppendMediaItems() {
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_LOOP,
                                       IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_CONTROLS,
                                       IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPluginItems() {
  bool is_full_page_oopif_pdf_viewer = false;
#if BUILDFLAG(ENABLE_PDF)
  is_full_page_oopif_pdf_viewer =
      chrome_pdf::features::IsOopifPdfEnabled() && embedder_web_contents_ &&
      embedder_web_contents_->GetContentsMimeType() == pdf::kPDFMimeType;
#endif  // BUILDFLAG(ENABLE_PDF)

  if (params_.page_url == params_.src_url ||
      ((is_full_page_oopif_pdf_viewer ||
        guest_view::GuestViewBase::IsGuest(GetRenderFrameHost())) &&
       (!embedder_web_contents_ || !embedder_web_contents_->IsSavable()))) {
    // Both full page and embedded plugins are hosted as guest now,
    // the difference is a full page plugin is not considered as savable.
    // For full page plugin, we show page menu items so long as focus is not
    // within an editable text area.
    if (params_.link_url.is_empty() && params_.selection_text.empty() &&
        !params_.is_editable) {
      AppendPageItems();
    }
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEPLUGINAS,
                                    IDS_CONTENT_CONTEXT_SAVEPAGEAS);
    // The "Print" menu item should always be included for plugins. If
    // content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)
    // is true the item will be added inside AppendPrintItem(). Otherwise we
    // add "Print" here.
    if (!content_type_->SupportsGroup(
            ContextMenuContentType::ITEM_GROUP_PRINT)) {
      menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
    }
  }
}

void RenderViewContextMenu::AppendPageItems() {
  AppendExitFullscreenItem();

  menu_model_.AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddItemWithStringId(IDC_RELOAD, IDS_CONTENT_CONTEXT_RELOAD);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_SAVE_PAGE,
                                  IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  AppendLiveCaptionItem();
  AppendMediaRouterItem();
  if (IsRegionSearchEnabled()) {
    AppendRegionSearchItem();
  }
  AppendReadingModeItem();

  // Note: `has_sharing_menu_items = true` also implies a separator was added
  // for sharing section.
  bool has_sharing_menu_items = false;
  // Send-Tab-To-Self (user's other devices), page level.
  if (GetBrowser() &&
      send_tab_to_self::ShouldDisplayEntryPoint(embedder_web_contents_)) {
    AppendSendTabToSelfItem(/*add_separator=*/!has_sharing_menu_items);
    has_sharing_menu_items = true;
  }

  // Context menu item for QR Code Generator.
  has_sharing_menu_items |=
      AppendQRCodeGeneratorItem(/*for_image=*/false, /*draw_icon=*/true,
                                /*add_separator=*/!has_sharing_menu_items);

  // Close out sharing section if appropriate.
  if (has_sharing_menu_items) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (CanTranslate(/*menu_logging=*/true)) {
    AppendTranslateItem();
  }
}

void RenderViewContextMenu::AppendExitFullscreenItem() {
  Browser* browser = GetBrowser();
  if (!browser) {
    return;
  }

  // Only show item if in fullscreen mode.
  if (!browser->exclusive_access_manager()
           ->fullscreen_controller()
           ->IsControllerInitiatedFullscreen()) {
    return;
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN,
                                  IDS_CONTENT_CONTEXT_EXIT_FULLSCREEN);
  menu_model_.SetElementIdentifierAt(
      menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN)
          .value(),
      kExitFullscreenMenuItem);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendCopyItem() {
  if (menu_model_.GetItemCount()) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendLinkToTextItems() {
  if (!GetPrefs(browser_context_)
           ->GetBoolean(prefs::kScrollToTextFragmentEnabled)) {
    return;
  }

  // Only show copy link to highlight for publicly accessible web pages.
  if (!params_.page_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (link_to_text_menu_observer_) {
    return;
  }

  link_to_text_menu_observer_ = LinkToTextMenuObserver::Create(
      this,
      content::GlobalRenderFrameHostId(render_process_id_, render_frame_id_),
      GetToastController(GetBrowser()));
  if (link_to_text_menu_observer_) {
    observers_.AddObserver(link_to_text_menu_observer_.get());
    link_to_text_menu_observer_->InitMenu(params_);
  }
}

void RenderViewContextMenu::AppendPrintItem() {
#if BUILDFLAG(ENABLE_PRINTING)
  if (GetPrefs(browser_context_)->GetBoolean(prefs::kPrintingEnabled) &&
      (params_.media_type == ContextMenuDataMediaType::kNone ||
       params_.media_flags & ContextMenuData::kMediaCanPrint) &&
      params_.misspelled_word.empty()) {
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void RenderViewContextMenu::AppendPartialTranslateItem() {
  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE,
      l10n_util::GetStringFUTF16(
          IDS_CONTENT_CONTEXT_PARTIAL_TRANSLATE,
          GetTargetLanguageDisplayName(/*is_full_page_translation=*/false)));
}

void RenderViewContextMenu::AppendTranslateItem() {
  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_TRANSLATE,
      l10n_util::GetStringFUTF16(
          IDS_CONTENT_CONTEXT_TRANSLATE,
          GetTargetLanguageDisplayName(/*is_full_page_translation=*/true)));
}

void RenderViewContextMenu::AppendMediaRouterItem() {
  if (media_router::MediaRouterEnabled(browser_context_)) {
    menu_model_.AddItemWithStringId(IDC_ROUTE_MEDIA,
                                    IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  }
}

void RenderViewContextMenu::AppendReadingModeItem() {
  // Show Read Anything option if it's not already open in the side panel.
  if (GetBrowser() && GetBrowser()->is_type_normal() &&
      !IsReadAnythingEntryShowing(GetBrowser())) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE,
                                    IDS_CONTENT_CONTEXT_READING_MODE);
  }
}

void RenderViewContextMenu::AppendRotationItems() {
  if (params_.media_flags & ContextMenuData::kMediaCanRotate) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECW,
                                    IDS_CONTENT_CONTEXT_ROTATECW);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECCW,
                                    IDS_CONTENT_CONTEXT_ROTATECCW);
  }
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(browser_context_);

  base::TrimWhitespace(params_.selection_text, base::TRIM_ALL,
                       &params_.selection_text);
  if (params_.selection_text.empty()) {
    return;
  }

  base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                     u" ", &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())
      ->Classify(params_.selection_text, false, false,
                 metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid()) {
    return;
  }

  std::u16string printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(GetProfile())
            ->GetDefaultSearchProvider();
    if (!default_provider) {
      return;
    }

    if (!base::Contains(
            params_.properties,
            prefs::kDefaultSearchProviderContextMenuAccessAllowed)) {
      return;
    }

    // If we couldn't obtain a valid helper from current WebContents, this item
    // should not be appended in this context.
    SideSearchTabContentsHelper* helper =
        SideSearchTabContentsHelper::FromWebContents(embedder_web_contents_);

    Browser* browser = chrome::FindBrowserWithTab(embedder_web_contents_);
    if (!side_search::IsSearchWebInSidePanelSupported(browser) ||
        (helper && helper->CanShowSidePanelFromContextMenuSearch())) {
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                     default_provider->short_name(),
                                     printable_selection_text));
      if (companion::IsSearchWebInCompanionSidePanelSupported(GetBrowser())) {
        // Add an "in new tab" item performing the non-side panel behavior.
        if (base::FeatureList::IsEnabled(
                companion::features::
                    kCompanionEnableSearchWebInNewTabContextMenuItem) &&
            selection_navigation_url_ != params_.link_url &&
            ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
                selection_navigation_url_.scheme())) {
          menu_model_.AddItem(
              IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB,
              l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB,
                                         default_provider->short_name(),
                                         printable_selection_text));
        }
      }
    }
  } else {
    if ((selection_navigation_url_ != params_.link_url) &&
        ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
            selection_navigation_url_.scheme())) {
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_GOTOURL,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                     printable_selection_text));
    }
  }
}

void RenderViewContextMenu::AppendSpellingAndSearchSuggestionItems() {
  const bool use_spelling = !IsRunningInForcedAppMode();
  if (use_spelling) {
    AppendSpellingSuggestionItems();
  }

  if (!params_.misspelled_word.empty()) {
    AppendSearchProvider();
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
  bool render_separator = false;
  if (params_.misspelled_word.empty()) {
    if ((!params_.form_control_type ||
         DoesFormControlTypeSupportEmoji(*params_.form_control_type)) &&
        ui::IsEmojiPanelSupported()) {
      render_separator = true;
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EMOJI,
                                      IDS_CONTENT_CONTEXT_EMOJI);
    }
  }
#if BUILDFLAG(ENABLE_COMPOSE)
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (render_frame_host) {
    auto* compose_client = GetChromeComposeClient();
    if (compose_client &&
        compose_client->ShouldTriggerContextMenu(render_frame_host, params_)) {
      compose::LogComposeContextMenuCtr(
          compose::ComposeContextMenuCtrEvent::kMenuItemDisplayed);
      base::RecordAction(
          base::UserMetricsAction("Compose.ContextMenu.ItemSeen"));
      menu_model_.AddItemWithStringId(IDC_CONTEXT_COMPOSE,
                                      IDS_COMPOSE_CONTEXT_MENU_TEXT);
      menu_model_.SetElementIdentifierAt(
          menu_model_.GetIndexOfCommandId(IDC_CONTEXT_COMPOSE).value(),
          kComposeMenuItem);

      // TODO(b/303646344): Remove new feature tag when no longer new.
      menu_model_.SetIsNewFeatureAt(
          menu_model_.GetItemCount() - 1,
          UserEducationService::MaybeShowNewBadge(
              GetBrowserContext(), compose::features::kEnableCompose));
      render_separator = true;
    }
  }
#endif  // BUILDFLAG(ENABLE_COMPOSE)
  if (render_separator) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void RenderViewContextMenu::AppendOtherEditableItems() {
// 'Undo' and 'Redo' for text input with no suggestions and no text selected.
// We make an exception for OS X as context clicking will select the closest
// word. In this case both items are always shown.
#if BUILDFLAG(IS_MAC)
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                  IDS_CONTENT_CONTEXT_UNDO);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                  IDS_CONTENT_CONTEXT_REDO);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
#else
  // Also want to show 'Undo' and 'Redo' if 'Emoji' is the only item in the menu
  // so far.
  if (!IsDevToolsURL(params_.page_url) &&
      !content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT) &&
      (!menu_model_.GetItemCount() ||
       menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_EMOJI)
           .has_value())) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                    IDS_CONTENT_CONTEXT_UNDO);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                    IDS_CONTENT_CONTEXT_REDO);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
#endif

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_CUT,
                                  IDS_CONTENT_CONTEXT_CUT);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE,
                                  IDS_CONTENT_CONTEXT_PASTE);

  const bool has_misspelled_word = !params_.misspelled_word.empty();
  if (!has_misspelled_word) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                                    IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
  }

#if BUILDFLAG(IS_CHROMEOS)
  bool need_clipboard_history_menu = true;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (auto* service = chromeos::LacrosService::Get();
      !service || !service->IsAvailable<crosapi::mojom::ClipboardHistory>()) {
    need_clipboard_history_menu = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (need_clipboard_history_menu) {
    // If the clipboard history refresh feature is enabled, insert a submenu of
    // clipboard history descriptors; otherwise, insert a menu option to trigger
    // the clipboard history menu.
    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      // `submenu_model_` is a class member. Therefore, it is safe to use `this`
      // pointer in the callback.
      submenu_model_ = chromeos::clipboard_history::
          ClipboardHistorySubmenuModel::CreateClipboardHistorySubmenuModel(
              crosapi::mojom::ClipboardHistoryControllerShowSource::
                  kRenderViewContextSubmenu,
              base::BindRepeating(
                  &RenderViewContextMenu::ShowClipboardHistoryMenu,
                  base::Unretained(this)));
      menu_model_.AddSubMenuWithStringId(GetClipboardHistoryCommandId(),
                                         GetClipboardHistoryStringId(),
                                         submenu_model_.get());
    } else {
      menu_model_.AddItemWithStringId(GetClipboardHistoryCommandId(),
                                      GetClipboardHistoryStringId());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (!has_misspelled_word) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SELECTALL,
                                    IDS_CONTENT_CONTEXT_SELECTALL);
  }

  AppendReadingModeItem();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendLanguageSettings() {
  const bool use_spelling = !IsRunningInForcedAppMode();
  if (!use_spelling) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
                                  IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);
#else
  if (!spelling_options_submenu_observer_) {
    const int kLanguageRadioGroup = 1;
    spelling_options_submenu_observer_ =
        std::make_unique<SpellingOptionsSubMenuObserver>(this, this,
                                                         kLanguageRadioGroup);
  }

  spelling_options_submenu_observer_->InitMenu(params_);
  observers_.AddObserver(spelling_options_submenu_observer_.get());
#endif
}

void RenderViewContextMenu::AppendSpellingSuggestionItems() {
  if (!spelling_suggestions_menu_observer_) {
    spelling_suggestions_menu_observer_ =
        std::make_unique<SpellingMenuObserver>(this);
  }
  observers_.AddObserver(spelling_suggestions_menu_observer_.get());
  spelling_suggestions_menu_observer_->InitMenu(params_);
}

bool RenderViewContextMenu::AppendAccessibilityLabelsItems() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (!accessibility_labels_menu_observer_) {
    accessibility_labels_menu_observer_ =
        std::make_unique<AccessibilityLabelsMenuObserver>(this);
  }
  observers_.AddObserver(accessibility_labels_menu_observer_.get());
  accessibility_labels_menu_observer_->InitMenu(params_);
  return accessibility_labels_menu_observer_->ShouldShowLabelsItem();
}

void RenderViewContextMenu::AppendProtocolHandlerSubMenu() {
  const custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty()) {
    return;
  }

  protocol_handler_registry_observation_.Observe(
      protocol_handler_registry_.get());
  is_protocol_submenu_valid_ = true;

  size_t max = IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST -
               IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  for (size_t i = 0; i < handlers.size() && i <= max; i++) {
    protocol_handler_submenu_model_.AddItem(
        IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST + i,
        base::UTF8ToUTF16(handlers[i].url().host()));
  }
  protocol_handler_submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  protocol_handler_submenu_model_.AddItem(
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH_CONFIGURE));

  menu_model_.AddSubMenu(
      IDC_CONTENT_CONTEXT_OPENLINKWITH,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH),
      &protocol_handler_submenu_model_);
}

void RenderViewContextMenu::AppendPasswordItems() {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetRenderFrameHost());

  if (!driver ||
      !driver->IsPasswordFieldForPasswordManager(
          autofill::FieldRendererId(params_.field_renderer_id), params_)) {
    return;
  }

  bool add_separator = false;

  // Don't show the item for guest or incognito profiles and also when the
  // automatic generation feature is disabled.
  if (password_manager_util::ManualPasswordGenerationEnabled(driver)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATEPASSWORD,
                                    IDS_CONTENT_CONTEXT_GENERATEPASSWORD);
    add_separator = true;
  }
  if (password_manager_util::ShowAllSavedPasswordsContextMenuEnabled(driver)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS,
                                    IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK);
    add_separator = true;
  }

  if (add_separator) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void RenderViewContextMenu::AppendSharingItems() {
  size_t items_initial = menu_model_.GetItemCount();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  // Check if the starting separator got added.
  size_t items_before_sharing = menu_model_.GetItemCount();
  bool starting_separator_added = items_before_sharing > items_initial;

  AppendClickToCallItem();

  // Add an ending separator if there are sharing items, otherwise remove the
  // starting separator iff we added one above.
  size_t sharing_items = menu_model_.GetItemCount() - items_before_sharing;
  if (sharing_items > 0) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  } else if (starting_separator_added) {
    menu_model_.RemoveItemAt(items_initial);
  }
}

void RenderViewContextMenu::AppendClickToCallItem() {
  SharingClickToCallEntryPoint entry_point;
  std::optional<std::string> phone_number;
  std::string selection_text;
  if (ShouldOfferClickToCallForURL(browser_context_, params_.link_url)) {
    entry_point = SharingClickToCallEntryPoint::kRightClickLink;
    phone_number = params_.link_url.GetContent();
  } else if (!params_.selection_text.empty()) {
    entry_point = SharingClickToCallEntryPoint::kRightClickSelection;
    selection_text = base::UTF16ToUTF8(params_.selection_text);
    phone_number =
        ExtractPhoneNumberForClickToCall(browser_context_, selection_text);
  }

  if (!phone_number || phone_number->empty()) {
    return;
  }

  if (!click_to_call_context_menu_observer_) {
    click_to_call_context_menu_observer_ =
        std::make_unique<ClickToCallContextMenuObserver>(this);
    observers_.AddObserver(click_to_call_context_menu_observer_.get());
  }

  click_to_call_context_menu_observer_->BuildMenu(*phone_number, selection_text,
                                                  entry_point);
}

void RenderViewContextMenu::AppendRegionSearchItem() {
  if (GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled()) {
    const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleLensMonochromeLogoIcon;
#else
        vector_icons::kSearchChromeRefreshIcon;
#endif
    menu_model_.AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH,
        IDS_CONTENT_CONTEXT_LENS_OVERLAY, ui::ImageModel::FromVectorIcon(icon));
    const int command_index =
        menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH)
            .value();
    menu_model_.SetElementIdentifierAt(command_index, kRegionSearchItem);
    menu_model_.SetIsNewFeatureAt(
        command_index, UserEducationService::MaybeShowNewBadge(
                           GetBrowserContext(), lens::features::kLensOverlay));
    return;
  }

  // GetImageSearchProvider can return null in unit tests or when the default
  // search provider is disabled by policy. In these cases, we align with the
  // search web for image menu item by not adding the region search menu item.
  const TemplateURL* provider = GetImageSearchProvider();
  if (provider) {
    const int region_search_idc = GetRegionSearchIdc();
    int resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH;
    if (lens::features::IsLensFullscreenSearchEnabled()) {
      // Default text for fullscreen search when enabled.
      resource_id = IDS_CONTENT_CONTEXT_LENS_REGION_SEARCH_ALT1;
    }
    menu_model_.AddItem(region_search_idc,
                        l10n_util::GetStringFUTF16(
                            resource_id, GetImageSearchProviderName(provider)));
    menu_model_.SetElementIdentifierAt(
        menu_model_.GetIndexOfCommandId(region_search_idc).value(),
        kRegionSearchItem);

    MaybePrepareForLensQuery();
  }
}

void RenderViewContextMenu::AppendLiveCaptionItem() {
  if (captions::IsLiveCaptionFeatureSupported() &&
      base::FeatureList::IsEnabled(media::kLiveCaptionRightClick)) {
    PrefService* prefs = GetPrefs(browser_context_);
    int string_id = prefs->GetBoolean(prefs::kLiveCaptionEnabled)
                        ? IDS_CONTENT_CONTEXT_LIVE_CAPTION_DISABLE
                        : IDS_CONTENT_CONTEXT_LIVE_CAPTION_ENABLE;
    menu_model_.AddItemWithStringId(IDC_LIVE_CAPTION, string_id);
  }
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
  // Disable context menu in locked fullscreen mode to prevent users from
  // exiting this mode (the menu is not really disabled as the user can still
  // open it, but all the individual context menu entries are disabled / greyed
  // out). We enable page navigation commands as well as extension ones when
  // locked for OnTask (only relevant for non-web browser scenarios).
  //
  // NOTE: If new commands are being added, please disable them by default and
  // notify the ChromeOS team by filing a bug under this component --
  // b/?q=componentid:1389107.
  Browser* const browser = GetBrowser();
  if (browser && platform_util::IsBrowserLockedFullscreen(browser)) {
    bool should_disable_command_for_locked_fullscreen = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (browser->IsLockedForOnTask()) {
      bool is_page_nav_command =
          (id == IDC_BACK) || (id == IDC_FORWARD) || (id == IDC_RELOAD);
      should_disable_command_for_locked_fullscreen =
          !is_page_nav_command &&
          !ContextMenuMatcher::IsExtensionsCustomCommandId(id);
    }
#endif
    if (should_disable_command_for_locked_fullscreen) {
      return false;
    }
  }

  {
    bool enabled = false;
    if (RenderViewContextMenuBase::IsCommandIdKnown(id, &enabled)) {
      return enabled;
    }
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  int content_restrictions = 0;
  if (core_tab_helper) {
    content_restrictions = core_tab_helper->content_restrictions();
  }
  if (id == IDC_PRINT && (content_restrictions & CONTENT_RESTRICTION_PRINT)) {
    return false;
  }

  if (id == IDC_SAVE_PAGE &&
      (content_restrictions & CONTENT_RESTRICTION_SAVE)) {
    return false;
  }

  PrefService* prefs = GetPrefs(browser_context_);

  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
  }

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    return extension_items_.IsCommandIdEnabled(id);
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return true;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return params_.link_url.is_valid();
  }

  // On ChromeOS a dedicated OTR profile is used for captive portal signin to
  // protect user privacy. Since some policies prevent Incognito browsing,
  // disable options that trigger navigation in the context menu.
  bool navigation_allowed = true;
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = GetProfile();
  if (IsCaptivePortalProfile(profile)) {
    navigation_allowed = false;
  }
#endif

  switch (id) {
    case IDC_BACK:
      return embedder_web_contents_->GetController().CanGoBack();

    case IDC_FORWARD:
      return embedder_web_contents_->GetController().CanGoForward();

    case IDC_RELOAD:
      return IsReloadEnabled();

    case IDC_LIVE_CAPTION:
      return true;

    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return IsViewSourceEnabled();

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      return IsDevCommandEnabled(id);

    case IDC_CONTENT_CONTEXT_TRANSLATE:
      return navigation_allowed && IsTranslateEnabled();

    case IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE:
      return navigation_allowed;

    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDC_CONTENT_CONTEXT_OPENLINKINPROFILE:
    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      return navigation_allowed && params_.link_url.is_valid() &&
             IsOpenLinkAllowedByDlp(params_.link_url);
    case IDC_CONTENT_CONTEXT_OPENLINKPREVIEW:
      return navigation_allowed && params_.link_url.is_valid() &&
             IsOpenLinkAllowedByDlp(params_.link_url) &&
             IsAllowedByUntrustedNetworkStatus();

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      return true;

    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      return IsSaveLinkAsEnabled();

    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
      return IsSaveImageAsEnabled();

    // The images shown in the most visited thumbnails can't be opened or
    // searched for conventionally.
    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
    case IDC_CONTENT_CONTEXT_LOAD_IMAGE:
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
    case IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE:
    case IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB:
    case IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS:
      return navigation_allowed && params_.src_url.is_valid() &&
             (params_.src_url.scheme() != content::kChromeUIScheme);

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return params_.has_image_contents;

    // Loop command should be disabled if the player is in an error state.
    case IDC_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags & ContextMenuData::kMediaCanLoop) != 0 &&
             (params_.media_flags & ContextMenuData::kMediaInError) == 0;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags & ContextMenuData::kMediaCanToggleControls) !=
             0;

    case IDC_CONTENT_CONTEXT_ROTATECW:
    case IDC_CONTENT_CONTEXT_ROTATECCW: {
      // Rotate commands should be disabled when in PDF Viewer's Presentation
      // mode.
      bool is_pdf_viewer_fullscreen =
          IsFrameInPdfViewer(GetRenderFrameHost()) && IsHTML5Fullscreen();
      return !is_pdf_viewer_fullscreen &&
             (params_.media_flags & ContextMenuData::kMediaCanRotate) != 0;
    }

    case IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS:
    case IDC_CONTENT_CONTEXT_COPYVIDEOFRAME:
    case IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORVIDEOFRAME:
      return IsVideoFrameItemEnabled(id);

    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
      return params_.src_url.is_valid() && !params_.src_url.SchemeIsBlob();

    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEPLUGINAS:
      return IsSaveAsEnabled();

    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      // Currently, a media element can be opened in a new tab iff it can
      // be saved. So rather than duplicating the MediaCanSave flag, we rely
      // on that here.
      return navigation_allowed &&
             !!(params_.media_flags & ContextMenuData::kMediaCanSave);

    case IDC_SAVE_PAGE:
      return IsSavePageEnabled();

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      return params_.is_subframe &&
             params_.frame_url.DeprecatedGetOriginAsURL() !=
                 chrome::kChromeUIPrintURL;

    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanUndo);

    case IDC_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanRedo);

    case IDC_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanCut);

    case IDC_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanCopy);

    case IDC_CONTENT_CONTEXT_PASTE:
      return IsPasteEnabled();

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      return IsPasteAndMatchStyleEnabled();

    case IDC_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanDelete);

    case IDC_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & ContextMenuDataEditFlags::kCanSelectAll);

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return navigation_allowed && IsOpenLinkOTREnabled();

    case IDC_PRINT:
      return IsPrintPreviewEnabled();

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB:
    case IDC_CONTENT_CONTEXT_GOTOURL:
      return navigation_allowed &&
             IsOpenLinkAllowedByDlp(selection_navigation_url_);

    case IDC_SPELLPANEL_TOGGLE:
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
    case IDC_SEND_TAB_TO_SELF:
      return true;

    case IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH:
    case IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH:
      // These region search items will not be added if there is no default
      // search provider available.
      return navigation_allowed;

    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE:
      return IsQRCodeGeneratorEnabled();

    case IDC_CONTENT_CONTEXT_SHARING_SUBMENU:
      return true;

    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);

#if !BUILDFLAG(IS_MAC) && BUILDFLAG(IS_POSIX)
    // TODO(suzhe): this should not be enabled for password fields.
    case IDC_INPUT_METHODS_MENU:
      return true;
#endif

    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_OPENLINKWITH:
    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
    case IDC_CONTENT_CONTEXT_GENERATEPASSWORD:
    case IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS:
      return true;

    case IDC_ROUTE_MEDIA:
      return IsRouteMediaEnabled();

    case IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE:
      return navigation_allowed;

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN:
      return true;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      return !!(params_.media_flags &
                ContextMenuData::kMediaCanPictureInPicture);

    case IDC_CONTENT_CONTEXT_EMOJI:
      return params_.is_editable;
    case IDC_CONTEXT_COMPOSE:
      return params_.is_editable;

    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5:
      return true;

    case IDC_CONTENT_CLIPBOARD_HISTORY_MENU:
    case IDC_CONTENT_PASTE_FROM_CLIPBOARD:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return ash::ClipboardHistoryController::Get()->HasAvailableHistoryItems();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    {
      // Disable the clipboard history menu option if:
      // 1. The clipboard history service is not available, or
      // 2. The paste menu option is not enabled, or
      // 3. There are no clipboard history item descriptors to populate a
      //    submenu when the clipboard history refresh feature is enabled.
      auto* service = chromeos::LacrosService::Get();
      if (!service ||
          !service->IsAvailable<crosapi::mojom::ClipboardHistory>() ||
          !IsPasteEnabled()) {
        return false;
      }
      return !chromeos::features::IsClipboardHistoryRefreshEnabled() ||
             !chromeos::clipboard_history::QueryItemDescriptors().empty();
    }
#else
      NOTREACHED_IN_MIGRATION() << "Unhandled id: " << id;
      return false;
#endif

    default:
      DUMP_WILL_BE_NOTREACHED() << "Unhandled id: " << id;
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  if (RenderViewContextMenuBase::IsCommandIdChecked(id)) {
    return true;
  }

  // See if the video is set to looping.
  if (id == IDC_CONTENT_CONTEXT_LOOP) {
    return (params_.media_flags & ContextMenuData::kMediaLoop) != 0;
  }

  if (id == IDC_CONTENT_CONTEXT_CONTROLS) {
    return (params_.media_flags & ContextMenuData::kMediaControls) != 0;
  }

  if (id == IDC_CONTENT_CONTEXT_PICTUREINPICTURE) {
    return (params_.media_flags & ContextMenuData::kMediaPictureInPicture) != 0;
  }

  if (id == IDC_CONTENT_CONTEXT_EMOJI) {
    return false;
  }

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    return extension_items_.IsCommandIdChecked(id);
  }

  return false;
}

bool RenderViewContextMenu::IsCommandIdVisible(int id) const {
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    return extension_items_.IsCommandIdVisible(id);
  }
  return RenderViewContextMenuBase::IsCommandIdVisible(id);
}

void RenderViewContextMenu::OpenURLWithExtraHeaders(
    const GURL& url,
    const GURL& referring_url,
    const url::Origin& initiator,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    const std::string& extra_headers,
    bool started_from_context_menu) {
  RenderViewContextMenuBase::OpenURLWithExtraHeaders(
      url, referring_url, initiator, disposition, transition, extra_headers,
      started_from_context_menu);
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  RenderViewContextMenuBase::ExecuteCommand(id, event_flags);
  if (command_executed_) {
    return;
  }
  command_executed_ = true;

  // Process extension menu items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    RenderFrameHost* render_frame_host = GetRenderFrameHost();
    if (render_frame_host) {
      extension_items_.ExecuteCommand(id, source_web_contents_,
                                      render_frame_host, params_);
    }
    return;
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    ExecProtocolHandler(event_flags,
                        id - IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST);
    return;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    ExecOpenLinkInProfile(id - IDC_OPEN_LINK_IN_PROFILE_FIRST);
    base::UmaHistogramEnumeration(kOpenLinkAsProfileHistogram,
                                  OpenLinkAs::kOpenLinkAsProfileClicked);
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
      WindowOpenDisposition new_tab_disposition =
          WindowOpenDisposition::NEW_BACKGROUND_TAB;
      Browser* browser = nullptr;
      if (IsInProgressiveWebApp()) {
        browser = FindNormalBrowser(GetProfile());
        new_tab_disposition = browser
                                  ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::NEW_WINDOW;
      }

      OpenURLParams params = GetOpenURLParamsWithExtraHeaders(
          params_.link_url, params_.frame_url, params_.frame_origin,
          new_tab_disposition, ui::PAGE_TRANSITION_LINK,
          /*extra_headers=*/std::string(), /*started_from_context_menu=*/true);

      if (browser) {
        browser->OpenURL(params, /*navigation_handle_callback=*/{});
      } else {
        source_web_contents_->OpenURL(params,
                                      /*navigation_handle_callback=*/{});
      }
      break;
    }

    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      DCHECK(!IsInProgressiveWebApp());
      OpenURLWithExtraHeaders(
          params_.link_url, params_.frame_url, params_.frame_origin,
          WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_LINK,
          /*extra_headers=*/std::string(),
          /*started_from_context_menu=*/true);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      // Pass along the |referring_url| so we can show it in browser UI. Note
      // that this won't and shouldn't be sent via the referrer header.
      // Note that PWA app windows are never incognito, we always open an
      // incognito browser tab.
      OpenURLWithExtraHeaders(
          params_.link_url, params_.frame_url, params_.frame_origin,
          WindowOpenDisposition::OFF_THE_RECORD, ui::PAGE_TRANSITION_LINK,
          /*extra_headers=*/std::string(),
          /*started_from_context_menu=*/true);
      base::UmaHistogramEnumeration(kOpenLinkAsProfileHistogram,
                                    OpenLinkAs::kOpenLinkAsIncognitoClicked);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      ExecOpenWebApp();
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKPREVIEW:
      ExecOpenLinkPreview();
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      CheckSupervisedUserURLFilterAndSaveLinkAs();
      break;

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEPLUGINAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
      ExecSaveAs();
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
#if !BUILDFLAG(IS_ANDROID)
      if (toast_features::IsEnabled(toast_features::kLinkCopiedToast)) {
        auto* const toast_controller = GetToastController(GetBrowser());
        if (toast_controller) {
          toast_controller->MaybeShowToast(ToastParams(ToastId::kLinkCopied));
        }
      }
#endif
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      ExecCopyLinkText();
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      ExecCopyImageAt();
#if !BUILDFLAG(IS_ANDROID)
      if (toast_features::IsEnabled(toast_features::kImageCopiedToast)) {
        auto* const toast_controller = GetToastController(GetBrowser());
        if (toast_controller) {
          toast_controller->MaybeShowToast(ToastParams(ToastId::kImageCopied));
        }
      }
#endif
      break;

    case IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS:
      ExecSaveVideoFrameAs();
      break;

    case IDC_CONTENT_CONTEXT_COPYVIDEOFRAME:
      ExecCopyVideoFrame();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHLENSFORVIDEOFRAME:
      ExecSearchForVideoFrame(event_flags, /*is_lens_query=*/true);
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORVIDEOFRAME:
      ExecSearchForVideoFrame(event_flags, /*is_lens_query=*/false);
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      ExecSearchWebForImage(/*is_image_translate=*/false);
      break;

    case IDC_CONTENT_CONTEXT_SEARCHLENSFORIMAGE:
      ExecSearchLensForImage(event_flags, /*is_image_translate=*/false);
      break;

    case IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHWEB:
      ExecSearchWebForImage(/*is_image_translate=*/true);
      break;

    case IDC_CONTENT_CONTEXT_TRANSLATEIMAGEWITHLENS:
      ExecSearchLensForImage(event_flags, /*is_image_translate=*/true);
      break;

    case IDC_CONTENT_CONTEXT_OPEN_IN_READING_MODE:
      ExecOpenInReadAnything();
      break;

    case IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH:
      ExecRegionSearch(event_flags, true);
      break;
    case IDC_CONTENT_CONTEXT_WEB_REGION_SEARCH:
      ExecRegionSearch(event_flags, false);
      break;

    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
      OpenURLWithExtraHeaders(params_.src_url, params_.frame_url,
                              params_.frame_origin,
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, std::string(), false);
      break;

    case IDC_CONTENT_CONTEXT_LOAD_IMAGE:
      ExecLoadImage();
      break;

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(params_.src_url, params_.frame_url, params_.frame_origin,
              WindowOpenDisposition::NEW_BACKGROUND_TAB,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_LOOP:
      ExecLoop();
      break;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      ExecControls();
      break;

    case IDC_CONTENT_CONTEXT_ROTATECW:
      ExecRotateCW();
      break;

    case IDC_CONTENT_CONTEXT_ROTATECCW:
      ExecRotateCCW();
      break;

    // When the context menu was initialized, we checked whether there were
    // back/forward entries. Session history may have changed while the context
    // menu was open. So we need to check `CanGoBack`/`CanGoForward` again.
    case IDC_BACK:
      chrome::GoBack(embedder_web_contents_);
      break;

    case IDC_FORWARD:
      chrome::GoForward(embedder_web_contents_);
      break;

    case IDC_SAVE_PAGE:
#if BUILDFLAG(ENABLE_PDF)
      // Give the PDF viewer a chance to handle the save, otherwise have the
      // embedder `WebContents` handle it.
      if (MaybePdfViewerHandlesSave(GetRenderFrameHost())) {
        break;
      }
#endif  // BUILDFLAG(ENABLE_PDF)
      embedder_web_contents_->OnSavePage();
      break;

    case IDC_SEND_TAB_TO_SELF:
      send_tab_to_self::ShowBubble(embedder_web_contents_);
      break;

    case IDC_CONTENT_CONTEXT_GENERATE_QR_CODE: {
      auto* bubble_controller =
          qrcode_generator::QRCodeGeneratorBubbleController::Get(
              embedder_web_contents_);
      if (params_.media_type == ContextMenuDataMediaType::kImage) {
        base::RecordAction(
            UserMetricsAction("SharingQRCode.DialogLaunched.ContextMenuImage"));
        bubble_controller->ShowBubble(params_.src_url);
      } else {
        base::RecordAction(
            UserMetricsAction("SharingQRCode.DialogLaunched.ContextMenuPage"));
        NavigationEntry* entry =
            embedder_web_contents_->GetController().GetLastCommittedEntry();
        bubble_controller->ShowBubble(entry->GetURL());
      }
      break;
    }

    case IDC_RELOAD:
      chrome::Reload(GetBrowser(), WindowOpenDisposition::CURRENT_TAB);
      break;

    case IDC_LIVE_CAPTION:
      ExecLiveCaption();
      break;

    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
      ExecReloadPackagedApp();
      break;

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      ExecRestartPackagedApp();
      break;

    case IDC_PRINT:
      ExecPrint();
      break;

    case IDC_ROUTE_MEDIA:
      ExecRouteMedia();
      break;

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN:
      base::RecordAction(base::UserMetricsAction("ExitFullscreen_ContextMenu"));
      ExecExitFullscreen();
      break;

    case IDC_VIEW_SOURCE:
      embedder_web_contents_->GetPrimaryMainFrame()->ViewSource();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      ExecInspectElement();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
      ExecInspectBackgroundPage();
      break;

    case IDC_CONTENT_CONTEXT_TRANSLATE:
      ExecTranslate();
      break;

    case IDC_CONTENT_CONTEXT_PARTIAL_TRANSLATE:
      ExecPartialTranslate();
      break;

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      source_web_contents_->ReloadFocusedFrame();
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      if (GetRenderFrameHost()) {
        GetRenderFrameHost()->ViewSource();
      }
      break;

    case IDC_CONTENT_CONTEXT_UNDO:
      source_web_contents_->Undo();
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      source_web_contents_->Redo();
      break;

    case IDC_CONTENT_CONTEXT_CUT:
      source_web_contents_->Cut();
      break;

    case IDC_CONTENT_CONTEXT_COPY:
      source_web_contents_->Copy();
      break;

    case IDC_CONTENT_CONTEXT_PASTE:
      source_web_contents_->Paste();
      break;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      source_web_contents_->PasteAndMatchStyle();
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
      source_web_contents_->Delete();
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->SelectAll();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR: {
      RecordAmbientSearchQuery(
          lens::AmbientSearchEntryPoint::CONTEXT_MENU_SEARCH_WEB_FOR);
      if (companion::IsSearchWebInCompanionSidePanelSupported(
              chrome::FindBrowserWithTab(embedder_web_contents_))) {
        ExecSearchWebInCompanionSidePanel(selection_navigation_url_);
        break;
      }
      // Searching in this side panel is dependent on the companion feature
      // being disabled.
      if (side_search::IsSearchWebInSidePanelSupported(
              chrome::FindBrowserWithTab(embedder_web_contents_)) &&
          !companion::IsSearchInCompanionSidePanelSupported(
              chrome::FindBrowserWithTab(embedder_web_contents_))) {
        ExecSearchWebInSidePanel(selection_navigation_url_);
        break;
      }
      ABSL_FALLTHROUGH_INTENDED;
    }
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORNEWTAB:
    case IDC_CONTENT_CONTEXT_GOTOURL: {
      auto disposition = ui::DispositionFromEventFlags(
          event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
      OpenURL(selection_navigation_url_, GURL(), {}, disposition,
              ui::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      ExecLanguageSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
      ExecProtocolHandlerSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_GENERATEPASSWORD:
      password_manager_util::UserTriggeredManualGenerationFromContextMenu(
          ChromePasswordManagerClient::FromWebContents(source_web_contents_),
          autofill::ContentAutofillClient::FromWebContents(
              source_web_contents_));
      break;

    case IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS:
      NavigateToManagePasswordsPage(
          GetBrowser(),
          password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
      break;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      ExecPictureInPicture();
      break;

    case IDC_CONTENT_CONTEXT_EMOJI: {
      // The emoji dialog is UI that can interfere with the fullscreen bubble,
      // so drop fullscreen when it is shown. https://crbug.com/1170584
      // TODO(avi): Do we need to attach the fullscreen block to the emoji
      // panel?
      source_web_contents_
          ->ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId)
          .RunAndReset();

      Browser* browser = GetBrowser();
      if (browser) {
        browser->window()->ShowEmojiPanel();
      } else {
        // TODO(crbug.com/40608277): Ensure this is called in the correct
        // process. This fails in print preview for PWA windows on Mac.
        ui::ShowEmojiPanel();
      }
      break;
    }

#if BUILDFLAG(ENABLE_COMPOSE)
    case IDC_CONTEXT_COMPOSE: {
      ExecOpenCompose();
      break;
    }
#endif  // BUILDFLAG(ENABLE_COMPOSE)

    case IDC_CONTENT_CLIPBOARD_HISTORY_MENU: {
#if BUILDFLAG(IS_CHROMEOS)
      // If the clipboard history refresh feature is enabled, we add a submenu
      // instead of a command item. The following code should not be executed
      // for a submenu.
      CHECK(!chromeos::features::IsClipboardHistoryRefreshEnabled());

      ShowClipboardHistoryMenu(event_flags);
#else
      NOTREACHED_IN_MIGRATION() << "Unhandled id: " << id;
#endif  // BUILDFLAG(IS_CHROMEOS)
      break;
    }

    default:
      DUMP_WILL_BE_NOTREACHED() << "Unhandled id: " << id;
      break;
  }
}

void RenderViewContextMenu::AddSpellCheckServiceItem(bool is_checked) {
  AddSpellCheckServiceItem(&menu_model_, is_checked);
}

void RenderViewContextMenu::AddAccessibilityLabelsServiceItem(bool is_checked) {
  if (is_checked) {
    menu_model_.AddCheckItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_MENU_OPTION);
  } else {
    // Add the submenu if the whole feature is not enabled.
    accessibility_labels_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_SEND);
    accessibility_labels_submenu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE_ONCE,
        IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_SEND_ONCE);
    menu_model_.AddSubMenu(
        IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_MENU_OPTION),
        &accessibility_labels_submenu_model_);
  }
}

// static
void RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
    base::OnceCallback<void(RenderViewContextMenu*)> cb) {
  *GetMenuShownCallback() = std::move(cb);
}

void RenderViewContextMenu::RegisterExecutePluginActionCallbackForTesting(
    ExecutePluginActionCallback cb) {
  execute_plugin_action_callback_ = std::move(cb);
}

custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList
RenderViewContextMenu::GetHandlersForLinkUrl() {
  custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      protocol_handler_registry_->GetHandlersFor(params_.link_url.scheme());
  std::sort(handlers.begin(), handlers.end());
  return handlers;
}

void RenderViewContextMenu::OnProtocolHandlerRegistryChanged() {
  is_protocol_submenu_valid_ = false;
}

void RenderViewContextMenu::NotifyMenuShown() {
  auto* cb = GetMenuShownCallback();
  if (!cb->is_null()) {
    std::move(*cb).Run(this);
  }
}

std::u16string RenderViewContextMenu::PrintableSelectionText() {
  return gfx::TruncateString(params_.selection_text, kMaxSelectionTextLength,
                             gfx::WORD_BREAK);
}

void RenderViewContextMenu::EscapeAmpersands(std::u16string* text) {
  base::ReplaceChars(*text, u"&", u"&&", text);
}

#if BUILDFLAG(IS_CHROMEOS)
const policy::DlpRulesManager* RenderViewContextMenu::GetDlpRulesManager()
    const {
  return policy::DlpRulesManagerFactory::GetForPrimaryProfile();
}
#endif

bool RenderViewContextMenu::IsSaveAsItemAllowedByPolicy(
    const GURL& item_url) const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Check if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs)) {
    return false;
  }

  // Check if all downloads are forbidden by policy.
  if (DownloadPrefs::FromBrowserContext(GetProfile())->download_restriction() ==
      DownloadPrefs::DownloadRestriction::ALL_FILES) {
    return false;
  }

  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context_);
  if (service->GetURLBlocklistState(item_url) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return false;
  }

  return true;
}

bool RenderViewContextMenu::IsAllowedByUntrustedNetworkStatus() const {
  if (!GetRenderFrameHost()) {
    return true;
  }

  // Download requests are not allowed in a frame tree that has untrusted
  // network access disabled.
  return !GetRenderFrameHost()->IsUntrustedNetworkDisabled();
}

// Controller functions --------------------------------------------------------

bool RenderViewContextMenu::IsReloadEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  return core_tab_helper && chrome::CanReload(GetBrowser());
}

bool RenderViewContextMenu::IsViewSourceEnabled() const {
  if (!!extensions::MimeHandlerViewGuest::FromRenderFrameHost(
          GetRenderFrameHost())) {
    return false;
  }
  // Disallow ViewSource if DevTools are disabled.
  if (!IsDevCommandEnabled(IDC_CONTENT_CONTEXT_INSPECTELEMENT)) {
    return false;
  }
  return (params_.media_type != ContextMenuDataMediaType::kPlugin) &&
         embedder_web_contents_->GetController().CanViewSource();
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) {
    PrefService* prefs = GetPrefs(browser_context_);
    if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
      return false;
    }

    // Don't enable the web inspector if the developer tools are disabled via
    // the preference dev-tools-disabled.
    if (!DevToolsWindow::AllowDevToolsFor(GetProfile(), source_web_contents_)) {
      return false;
    }
  }

  return true;
}

bool RenderViewContextMenu::IsTranslateEnabled() const {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  // If no |chrome_translate_client| attached with this WebContents, or
  // the translate manager has been shut down, or we're viewing in a
  // MimeHandlerViewGuest translate will be disabled.
  if (!chrome_translate_client ||
      !chrome_translate_client->GetTranslateManager() ||
      !!extensions::MimeHandlerViewGuest::FromRenderFrameHost(
          GetRenderFrameHost())) {
    return false;
  }
  std::string source_lang =
      chrome_translate_client->GetLanguageState().source_language();
  // Note that we intentionally enable the menu even if the source and
  // target languages are identical.  This is to give a way to user to
  // translate a page that might contains text fragments in a different
  // language.
  return ((params_.edit_flags & ContextMenuDataEditFlags::kCanTranslate) !=
          0) &&
         !source_lang.empty() &&  // Did we receive the page language yet?
         // Disable on the Instant Extended NTP.
         !search::IsInstantNTP(embedder_web_contents_);
}

bool RenderViewContextMenu::IsSaveLinkAsEnabled() const {
  if (!IsSaveAsItemAllowedByPolicy(params_.link_url)) {
    return false;
  }

  if (!IsAllowedByUntrustedNetworkStatus()) {
    return false;
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context_);
  CHECK(profile);
  if (profile->IsChild()) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    // Use the URL filter's synchronous call to check if a site has been
    // manually blocked for the user. This does not filter websites that are
    // blocked by SafeSites API for having mature content. The mature content
    // filter requires an async call. This call is made if the user selects
    // "Save link as" and blocks the download.
    if (url_filter->GetFilteringBehaviorForURL(params_.link_url) !=
        supervised_user::FilteringBehavior::kAllow) {
      return false;
    }
  }

  return params_.link_url.is_valid() &&
         ProfileIOData::IsHandledProtocol(params_.link_url.scheme());
}

bool RenderViewContextMenu::IsSaveImageAsEnabled() const {
  if (!IsSaveAsItemAllowedByPolicy(params_.src_url)) {
    return false;
  }

  if (!IsAllowedByUntrustedNetworkStatus()) {
    return false;
  }

  return params_.has_image_contents;
}

bool RenderViewContextMenu::IsSaveAsEnabled() const {
  const GURL& url = params_.src_url;
  if (!IsSaveAsItemAllowedByPolicy(url)) {
    return false;
  }

  if (!IsAllowedByUntrustedNetworkStatus()) {
    return false;
  }

  bool can_save = (params_.media_flags & ContextMenuData::kMediaCanSave) &&
                  url.is_valid() &&
                  ProfileIOData::IsHandledProtocol(url.scheme());
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Do not save the preview PDF on the print preview page.
  can_save =
      can_save &&
      !printing::PrintPreviewDialogController::IsPrintPreviewContentURL(url);
#endif
  return can_save;
}

bool RenderViewContextMenu::IsSavePageEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  if (!core_tab_helper) {
    return false;
  }

  Browser* browser = GetBrowser();
  if (browser && !browser->CanSaveContents(embedder_web_contents_)) {
    return false;
  }

  // We save the last committed entry (which the user is looking at), as
  // opposed to any pending URL that hasn't committed yet.
  NavigationEntry* entry =
      embedder_web_contents_->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }

  GURL url = entry->GetURL();
  if (!IsSaveAsItemAllowedByPolicy(url)) {
    return false;
  }

  return content::IsSavableURL(url);
}

bool RenderViewContextMenu::IsPasteEnabled() const {
  if (!(params_.edit_flags & ContextMenuDataEditFlags::kCanPaste)) {
    return false;
  }

  std::vector<std::u16string> types;
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/false).get(), &types);
  return !types.empty();
}

bool RenderViewContextMenu::IsOpenLinkAllowedByDlp(const GURL& link_url) const {
#if BUILDFLAG(IS_CHROMEOS)
  const policy::DlpRulesManager* dlp_rules_manager = GetDlpRulesManager();
  if (!dlp_rules_manager) {
    return true;
  }
  policy::DlpRulesManager::Level level =
      dlp_rules_manager->IsRestrictedDestination(
          params_.page_url, link_url,
          policy::DlpRulesManager::Restriction::kClipboard,
          /*out_source_pattern=*/nullptr, /*out_destination_pattern=*/nullptr,
          /*out_rule_metadata=*/nullptr);
  // TODO(crbug.com/1222057): show a warning if the level is kWarn
  return level != policy::DlpRulesManager::Level::kBlock;
#else
  return true;
#endif
}

bool RenderViewContextMenu::IsPasteAndMatchStyleEnabled() const {
  if (!(params_.edit_flags & ContextMenuDataEditFlags::kCanPaste)) {
    return false;
  }

  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::ClipboardFormatType::PlainTextType(), ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/false).get());
}

bool RenderViewContextMenu::IsPrintPreviewEnabled() const {
  if (params_.media_type != ContextMenuDataMediaType::kNone &&
      !(params_.media_flags & ContextMenuData::kMediaCanPrint)) {
    return false;
  }

  Browser* browser = GetBrowser();
  return browser && chrome::CanPrint(browser);
}

bool RenderViewContextMenu::IsQRCodeGeneratorEnabled() const {
  if (!GetBrowser() || !GetProfile()) {
    return false;
  }

  if (sharing_hub::SharingIsDisabledByPolicy(GetProfile())) {
    // If the sharing hub is disabled, clicking the QR code item (which tries to
    // show the sharing hub) won't work.
    return false;
  }

  if (params_.media_type == ContextMenuDataMediaType::kImage) {
    return qrcode_generator::QRCodeGeneratorBubbleController::
        IsGeneratorAvailable(params_.src_url);
  }

  NavigationEntry* entry =
      embedder_web_contents_->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }
  return qrcode_generator::QRCodeGeneratorBubbleController::
      IsGeneratorAvailable(entry->GetURL());
}

bool RenderViewContextMenu::IsRegionSearchEnabled() const {
  if (!GetBrowser()) {
    return false;
  }

  if (GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled()) {
    return true;
  }

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#if BUILDFLAG(IS_MAC)
  // Region selection is broken in PWAs on Mac b/250074889
  if (IsInProgressiveWebApp()) {
    return false;
  }
#endif  // BUILDFLAG(IS_MAC)

  return base::FeatureList::IsEnabled(lens::features::kLensStandalone) &&
         GetImageSearchProvider() &&
         !params_.frame_url.SchemeIs(content::kChromeUIScheme) &&
         GetPrefs(browser_context_)
             ->GetBoolean(prefs::kLensRegionSearchEnabled);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}

bool RenderViewContextMenu::IsVideoFrameItemEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS &&
      !IsAllowedByUntrustedNetworkStatus()) {
    return false;
  }

  return (params_.media_flags & ContextMenuData::kMediaEncrypted) == 0 &&
         (params_.media_flags & ContextMenuData::kMediaHasReadableVideoFrame) !=
             0;
}

// Returns true if the item was appended.
bool RenderViewContextMenu::AppendQRCodeGeneratorItem(bool for_image,
                                                      bool draw_icon,
                                                      bool add_separator) {
  if (!IsQRCodeGeneratorEnabled()) {
    return false;
  }

  if (add_separator) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  auto string_id = for_image ? IDS_CONTEXT_MENU_GENERATE_QR_CODE_IMAGE
                             : IDS_CONTEXT_MENU_GENERATE_QR_CODE_PAGE;
#if BUILDFLAG(IS_MAC)
  draw_icon = false;
#endif
  if (draw_icon) {
    menu_model_.AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_GENERATE_QR_CODE, string_id,
        ui::ImageModel::FromVectorIcon(kQrcodeGeneratorIcon));
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_GENERATE_QR_CODE,
                                    string_id);
  }

  return true;
}

void RenderViewContextMenu::AppendSendTabToSelfItem(bool add_separator) {
  if (add_separator) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
#if BUILDFLAG(IS_MAC)
  menu_model_.AddItem(IDC_SEND_TAB_TO_SELF,
                      l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF));
#else
  menu_model_.AddItemWithIcon(
      IDC_SEND_TAB_TO_SELF,
      l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF),
      ui::ImageModel::FromVectorIcon(kDevicesIcon));
#endif
}

std::unique_ptr<ui::DataTransferEndpoint>
RenderViewContextMenu::CreateDataEndpoint(bool notify_if_restricted) const {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (render_frame_host) {
    return std::make_unique<ui::DataTransferEndpoint>(
        render_frame_host->GetMainFrame()->GetLastCommittedURL(),
        ui::DataTransferEndpointOptions{
            .notify_if_restricted = notify_if_restricted,
            .off_the_record =
                render_frame_host->GetBrowserContext()->IsOffTheRecord()});
  }
  return nullptr;
}

bool RenderViewContextMenu::IsRouteMediaEnabled() const {
  if (!media_router::MediaRouterEnabled(browser_context_)) {
    return false;
  }

  Browser* browser = GetBrowser();
  if (!browser) {
    return false;
  }

  // Disable the command if there is an active modal dialog.
  // We don't use |source_web_contents_| here because it could be the
  // WebContents for something that's not the current tab (e.g., WebUI
  // modal dialog).
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return !manager || !manager->IsDialogActive();
}

bool RenderViewContextMenu::IsOpenLinkOTREnabled() const {
  if (browser_context_->IsOffTheRecord() || !params_.link_url.is_valid()) {
    return false;
  }

  if (!IsURLAllowedInIncognito(params_.link_url, browser_context_)) {
    return false;
  }

  policy::IncognitoModeAvailability incognito_avail =
      IncognitoModePrefs::GetAvailability(GetPrefs(browser_context_));
  return incognito_avail != policy::IncognitoModeAvailability::kDisabled;
}

void RenderViewContextMenu::ExecOpenWebApp() {
  std::optional<webapps::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(
          Profile::FromBrowserContext(browser_context_), params_.link_url);
  // |app_id| could be nullopt if it has been uninstalled since the user
  // opened the context menu.
  if (!app_id) {
    return;
  }

  apps::AppLaunchParams launch_params(
      *app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromMenu);
  launch_params.override_url = params_.link_url;
  apps::AppServiceProxyFactory::GetForProfile(GetProfile())
      ->LaunchAppWithParams(std::move(launch_params));
}

void RenderViewContextMenu::ExecOpenLinkPreview() {
  CHECK(embedder_web_contents_);
  CHECK(embedder_web_contents_->GetDelegate());

  embedder_web_contents_->GetDelegate()->InitiatePreview(
      *embedder_web_contents_, params_.link_url);
}

void RenderViewContextMenu::ExecProtocolHandler(int event_flags,
                                                int handler_index) {
  if (!is_protocol_submenu_valid_) {
    // A protocol was changed since the time that the menu was built, so the
    // index passed in isn't valid. The only thing that can be done now safely
    // is to bail.
    return;
  }

  custom_handlers::ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty()) {
    return;
  }

  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Open"));
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  OpenURL(handlers[handler_index].TranslateUrl(params_.link_url),
          params_.frame_url, params_.frame_origin, disposition,
          ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecOpenLinkInProfile(int profile_index) {
  DCHECK_GE(profile_index, 0);
  DCHECK_LE(profile_index, static_cast<int>(profile_link_paths_.size()));

  base::FilePath profile_path = profile_link_paths_[profile_index];
  profiles::SwitchToProfile(
      profile_path, false,
      base::BindRepeating(OnBrowserCreated, params_.link_url,
                          params_.frame_origin));
}

#if BUILDFLAG(ENABLE_COMPOSE)
void RenderViewContextMenu::ExecOpenCompose() {
  ChromeComposeClient* client = GetChromeComposeClient();
  if (!client) {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kNoChromeComposeClient);
    return;
  }
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kNoRenderFrameHost);
    return;
  }
  if (auto* driver = autofill::ContentAutofillDriver::GetForRenderFrameHost(
          render_frame_host)) {
    autofill::LocalFrameToken frame_token = driver->GetFrameToken();
    client->GetManager().OpenCompose(
        *driver,
        autofill::FormGlobalId(
            frame_token, autofill::FormRendererId(params_.form_renderer_id)),
        autofill::FieldGlobalId(
            frame_token, autofill::FieldRendererId(params_.field_renderer_id)),
        compose::ComposeManagerImpl::UiEntryPoint::kContextMenu);
    GetBrowser()->window()->NotifyNewBadgeFeatureUsed(
        compose::features::kEnableCompose);
  } else {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kNoContentAutofillDriver);
  }
}
#endif

void RenderViewContextMenu::ExecOpenInReadAnything() {
  Browser* browser = GetBrowser();
  if (!browser) {
    return;
  }
  ShowReadAnythingSidePanel(browser,
                            SidePanelOpenTrigger::kReadAnythingContextMenu);
}

void RenderViewContextMenu::ExecInspectElement() {
  base::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }
  DevToolsWindow::InspectElement(render_frame_host, params_.x, params_.y);
}

void RenderViewContextMenu::ExecInspectBackgroundPage() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  extensions::devtools_util::InspectBackgroundPage(
      platform_app, GetProfile(), DevToolsOpenedByAction::kContextMenuInspect);
}

void RenderViewContextMenu::CheckSupervisedUserURLFilterAndSaveLinkAs() {
  Profile* const profile = Profile::FromBrowserContext(browser_context_);
  CHECK(profile);
  if (profile->IsChild()) {
    supervised_user::SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile);
    supervised_user::SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilter();
    url_filter->GetFilteringBehaviorForURLWithAsyncChecks(
        params_.link_url,
        base::BindOnce(&RenderViewContextMenu::OnSupervisedUserURLFilterChecked,
                       weak_pointer_factory_.GetWeakPtr()),
        /* skip_manual_parent_filter= */ false);
    return;
  }
  ExecSaveLinkAs();
}

void RenderViewContextMenu::OnSupervisedUserURLFilterChecked(
    supervised_user::FilteringBehavior filtering_behavior,
    supervised_user::FilteringBehaviorReason reason,
    bool uncertain) {
  if (filtering_behavior == supervised_user::FilteringBehavior::kAllow) {
    ExecSaveLinkAs();
  }
}

void RenderViewContextMenu::ExecSaveLinkAs() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }

  RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);

  const GURL& url = params_.link_url;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("render_view_context_menu", R"(
        semantics {
          sender: "Save Link As"
          description: "Saving url to local file."
          trigger:
            "The user selects the 'Save link as...' command in the context "
            "menu."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings. The request is made "
            "only if user chooses 'Save link as...' in the context menu."
          policy_exception_justification: "Not implemented."
        })");

  auto dl_params = std::make_unique<DownloadUrlParameters>(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
  content::Referrer referrer = CreateReferrer(url, params_);
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  dl_params->set_referrer_encoding(params_.frame_charset);
  // TODO(crbug.com/40066346): use the actual origin here rather than
  // pulling it out of the frame url.
  dl_params->set_initiator(url::Origin::Create(params_.frame_url));
  dl_params->set_suggested_name(params_.suggested_filename);
  dl_params->set_prompt(true);
  dl_params->set_download_source(download::DownloadSource::CONTEXT_MENU);

  // Attach the nonce. This allows URL loader to stop in-progress download
  // request if the nonce is revoked for untruested network access.
  url::Origin origin = url::Origin::Create(url);
  dl_params->set_isolation_info(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, /*top_frame_origin=*/origin,
      /*frame_origin=*/origin, net::SiteForCookies::FromUrl(url),
      /*nonce=*/render_frame_host->GetIsolationInfoForSubresources().nonce()));

  browser_context_->GetDownloadManager()->DownloadUrl(std::move(dl_params));
}

void RenderViewContextMenu::ExecSaveAs() {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  bool is_large_data_url =
      params_.has_image_contents && params_.src_url.is_empty();
  if (params_.media_type == ContextMenuDataMediaType::kCanvas ||
      (params_.media_type == ContextMenuDataMediaType::kImage &&
       is_large_data_url)) {
    if (frame_host) {
      frame_host->SaveImageAt(params_.x, params_.y);
    }
    return;
  }

  RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
  GURL url = params_.src_url;
  const bool is_plugin =
      params_.media_type == ContextMenuDataMediaType::kPlugin;
  RenderFrameHost* target_frame_host = nullptr;

#if BUILDFLAG(ENABLE_PDF)
  if (chrome_pdf::features::IsOopifPdfEnabled() && is_plugin &&
      IsFrameInPdfViewer(frame_host)) {
    // Give the PDF viewer a chance to handle the save.
    if (MaybePdfViewerHandlesSave(frame_host)) {
      return;
    }

    // Handle the save here.
    target_frame_host = pdf_frame_util::GetEmbedderHost(frame_host);
    CHECK(target_frame_host);
    url = target_frame_host->GetLastCommittedURL();
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  if (!target_frame_host) {
    target_frame_host = is_plugin
                            ? source_web_contents_->GetOuterWebContentsFrame()
                            : frame_host;
    if (!target_frame_host) {
      return;
    }
  }

  net::HttpRequestHeaders headers;

  if (params_.media_type == ContextMenuDataMediaType::kImage) {
    headers.SetHeaderIfMissing(net::HttpRequestHeaders::kAccept,
                               blink::network_utils::ImageAcceptHeader());
  }
  content::Referrer referrer = CreateReferrer(url, params_);
  bool is_subresource = params_.media_type != ContextMenuDataMediaType::kNone &&
                        !params_.is_image_media_plugin_document;
  source_web_contents_->SaveFrameWithHeaders(url, referrer, headers.ToString(),
                                             params_.suggested_filename,
                                             target_frame_host, is_subresource);
}

void RenderViewContextMenu::ExecExitFullscreen() {
  Browser* browser = GetBrowser();
  if (!browser) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  browser->exclusive_access_manager()->ExitExclusiveAccess();
}

void RenderViewContextMenu::ExecCopyLinkText() {
  ui::ScopedClipboardWriter scw(
      ui::ClipboardBuffer::kCopyPaste,
      CreateDataEndpoint(/*notify_if_restricted=*/true));
  scw.SetDataSourceURL(main_frame_url_, current_url_);
  scw.WriteText(params_.link_text);
}

void RenderViewContextMenu::ExecCopyImageAt() {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (frame_host) {
    frame_host->CopyImageAt(params_.x, params_.y);
  }
}

void RenderViewContextMenu::ExecSearchLensForImage(int event_flags,
                                                   bool is_image_translate) {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper) {
    return;
  }
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }

  bool entered_through_keyboard =
      IsLensOptionEnteredThroughKeyboard(event_flags);
  bool lens_overlay_for_image_search_enabled =
      GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled() &&
      lens::features::UseLensOverlayForImageSearch();
  if (lens_overlay_for_image_search_enabled && !entered_through_keyboard) {
    lens::RecordAmbientSearchQuery(
        lens::AmbientSearchEntryPoint::
            CONTEXT_MENU_SEARCH_IMAGE_WITH_LENS_OVERLAY);

    auto view_bounds = render_frame_host->GetView()->GetViewBounds();
    auto tab_bounds = source_web_contents_->GetViewBounds();
    float device_scale_factor =
        render_frame_host->GetView()->GetDeviceScaleFactor();
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
        &chrome_render_frame);
    // Bind the InterfacePtr into the callback so that it's kept alive until
    // there's either a connection error or a response.
    auto* frame = chrome_render_frame.get();

    frame->RequestBitmapForContextNodeWithBoundsHint(base::BindOnce(
        &RenderViewContextMenu::OpenLensOverlayWithPreselectedRegion,
        weak_pointer_factory_.GetWeakPtr(), std::move(chrome_render_frame),
        tab_bounds, view_bounds, device_scale_factor));
  } else {
    // When the Lens image search feature is entered via the context menu
    // with a Keyboard action, use the Lens region search flow through
    // core_tab_helper (with results forced into a new tab) instead of the
    // Lens Overlay flow.
    bool force_open_in_new_tab =
        lens_overlay_for_image_search_enabled && entered_through_keyboard;
    lens::RecordAmbientSearchQuery(
        lens_overlay_for_image_search_enabled
            ? lens::AmbientSearchEntryPoint::
                  CONTEXT_MENU_SEARCH_IMAGE_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK
            : lens::AmbientSearchEntryPoint::
                  CONTEXT_MENU_SEARCH_IMAGE_WITH_GOOGLE_LENS);
    core_tab_helper->SearchWithLens(
        render_frame_host, params().src_url,
        is_image_translate
            ? lens::EntryPoint::
                  CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM
            : lens::EntryPoint::
                  CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM,
        is_image_translate, force_open_in_new_tab);
  }
}

void RenderViewContextMenu::OpenLensOverlayWithPreselectedRegion(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    float device_scale_factor,
    const SkBitmap& region_bitmap,
    const gfx::Rect& region_bounds) {
  // Scale the region bounds, which are in physical pixels, to device pixels.
  auto scaled_region_bounds =
      gfx::ScaleToEnclosedRect(region_bounds, 1.f / device_scale_factor);
  LensOverlayController* const controller =
      LensOverlayController::GetController(source_web_contents_);
  CHECK(controller);
  controller->ShowUIWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      tab_bounds, view_bounds, scaled_region_bounds, region_bitmap);
}

void RenderViewContextMenu::ExecRegionSearch(
    int event_flags,
    bool is_google_default_search_provider) {
  Browser* browser = GetBrowser();
  CHECK(browser);

  bool lens_overlay_for_region_search_enabled =
      GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled();
  // If Lens overlay is enabled, but the user triggered the context menu
  // option via keyboard, use the Lens region search flow (with results
  // forced into a new tab) instead of the Lens Overlay flow.
  // TODO(crbug/353984457): Clean this branching when the new server
  // results flow is ready.
  bool entered_through_keyboard =
      IsLensOptionEnteredThroughKeyboard(event_flags);
  if (lens_overlay_for_region_search_enabled) {
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        GetBrowserContext(), lens::features::kLensOverlay);
    if (!entered_through_keyboard) {
      lens::RecordAmbientSearchQuery(
          lens::AmbientSearchEntryPoint::
              CONTEXT_MENU_SEARCH_REGION_WITH_LENS_OVERLAY);
      LensOverlayController* const controller =
          LensOverlayController::GetController(embedder_web_contents_);
      CHECK(controller);
      controller->ShowUI(
          lens::LensOverlayInvocationSource::kContentAreaContextMenuPage);
      return;
    }
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (lens::features::IsLensRegionSearchStaticPageEnabled()) {
    lens::OpenLensStaticPage(browser);
    return;
  }

  // If Lens fullscreen search is enabled, we want to send every region search
  // as a fullscreen capture.
  // TODO(crbug/353984457): Clean this branching when the new server
  // results flow is ready.
  bool use_fullscreen_capture = entered_through_keyboard ||
                                lens::features::IsLensFullscreenSearchEnabled();
  bool force_open_in_new_tab =
      lens_overlay_for_region_search_enabled && entered_through_keyboard;

  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(embedder_web_contents_);
  if (companion_helper &&
      companion::IsSearchImageInCompanionSidePanelSupported(browser)) {
    companion_helper->StartRegionSearch(
        embedder_web_contents_, use_fullscreen_capture, force_open_in_new_tab);
    return;
  }

  if (!lens_region_search_controller_) {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>();
  }
  const lens::AmbientSearchEntryPoint entry_point =
      lens_overlay_for_region_search_enabled
          ? lens::AmbientSearchEntryPoint::
                CONTEXT_MENU_SEARCH_REGION_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK
      : is_google_default_search_provider
          ? lens::AmbientSearchEntryPoint::
                CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS
          : lens::AmbientSearchEntryPoint::CONTEXT_MENU_SEARCH_REGION_WITH_WEB;
  lens_region_search_controller_->Start(
      embedder_web_contents_, use_fullscreen_capture, force_open_in_new_tab,
      is_google_default_search_provider, entry_point);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void RenderViewContextMenu::ExecSearchWebForImage(bool is_image_translate) {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper) {
    return;
  }
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }
  lens::RecordAmbientSearchQuery(
      lens::AmbientSearchEntryPoint::CONTEXT_MENU_SEARCH_IMAGE_WITH_WEB);
  core_tab_helper->SearchByImage(render_frame_host, params().src_url,
                                 is_image_translate);
}

void RenderViewContextMenu::ExecSearchWebInCompanionSidePanel(const GURL& url) {
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(embedder_web_contents_);
  if (!companion_helper) {
    return;
  }
  companion_helper->ShowCompanionSidePanelForSearchURL(url);
}

void RenderViewContextMenu::ExecSearchWebInSidePanel(const GURL& url) {
  SideSearchTabContentsHelper* helper =
      SideSearchTabContentsHelper::FromWebContents(embedder_web_contents_);
  DCHECK(helper);
  helper->OpenSidePanelFromContextMenuSearch(url);
}

void RenderViewContextMenu::ExecLoadImage() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host) {
    return;
  }
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  chrome_render_frame->RequestReloadImageForContextNode();
}

void RenderViewContextMenu::ExecLoop() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Loop"));
  MediaPlayerAction(blink::mojom::MediaPlayerAction(
      blink::mojom::MediaPlayerActionType::kLoop,
      !IsCommandIdChecked(IDC_CONTENT_CONTEXT_LOOP)));
}

void RenderViewContextMenu::ExecControls() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Controls"));
  MediaPlayerAction(blink::mojom::MediaPlayerAction(
      blink::mojom::MediaPlayerActionType::kControls,
      !IsCommandIdChecked(IDC_CONTENT_CONTEXT_CONTROLS)));
}

void RenderViewContextMenu::ExecSaveVideoFrameAs() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_SaveVideoFrameAs"));
  MediaPlayerAction(blink::mojom::MediaPlayerAction(
      blink::mojom::MediaPlayerActionType::kSaveVideoFrameAs,
      /*enable=*/true));
}

void RenderViewContextMenu::ExecCopyVideoFrame() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_CopyVideoFrame"));
  MediaPlayerAction(blink::mojom::MediaPlayerAction(
      blink::mojom::MediaPlayerActionType::kCopyVideoFrame,
      /*enable=*/true));
}

void RenderViewContextMenu::ExecSearchForVideoFrame(int event_flags,
                                                    bool is_lens_query) {
  base::RecordAction(UserMetricsAction("MediaContextMenu_SearchForVideoFrame"));

  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (!frame_host) {
    return;
  }

  frame_host->RequestVideoFrameAtWithBoundsHint(
      gfx::Point(params_.x, params_.y),
      gfx::Size(lens::kMaxPixelsForImageSearch, lens::kMaxPixelsForImageSearch),
      lens::kMaxAreaForImageSearch,
      base::BindOnce(&RenderViewContextMenu::SearchForVideoFrame,
                     weak_pointer_factory_.GetWeakPtr(), event_flags,
                     is_lens_query));
}

void RenderViewContextMenu::ExecLiveCaption() {
  PrefService* prefs = GetPrefs(browser_context_);
  bool is_enabled = !prefs->GetBoolean(prefs::kLiveCaptionEnabled);
  prefs->SetBoolean(prefs::kLiveCaptionEnabled, is_enabled);
  base::UmaHistogramBoolean("Accessibility.LiveCaption.EnableFromContextMenu",
                            is_enabled);
}

void RenderViewContextMenu::ExecRotateCW() {
  base::RecordAction(UserMetricsAction("PluginContextMenu_RotateClockwise"));
  PluginActionAt(gfx::Point(params_.x, params_.y),
                 blink::mojom::PluginActionType::kRotate90Clockwise);
}

void RenderViewContextMenu::ExecRotateCCW() {
  base::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateCounterclockwise"));
  PluginActionAt(gfx::Point(params_.x, params_.y),
                 blink::mojom::PluginActionType::kRotate90Counterclockwise);
}

void RenderViewContextMenu::ExecReloadPackagedApp() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  extensions::ExtensionSystem::Get(browser_context_)
      ->extension_service()
      ->ReloadExtension(platform_app->id());
}

void RenderViewContextMenu::ExecRestartPackagedApp() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  apps::AppLoadService::Get(GetProfile())
      ->RestartApplication(platform_app->id());
}

void RenderViewContextMenu::ExecPrint() {
#if BUILDFLAG(ENABLE_PRINTING)
  const bool print_preview_disabled =
      GetPrefs(browser_context_)->GetBoolean(prefs::kPrintPreviewDisabled);
  if (params_.media_type != ContextMenuDataMediaType::kNone) {
    RenderFrameHost* rfh = GetRenderFrameHost();
    if (rfh) {
      printing::StartPrintNodeUnderContextMenu(rfh, print_preview_disabled);
    }
    return;
  }

  printing::StartPrint(source_web_contents_,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                       mojo::NullAssociatedRemote(),
#endif
                       print_preview_disabled, !params_.selection_text.empty());
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void RenderViewContextMenu::ExecRouteMedia() {
  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          embedder_web_contents_);
  if (!dialog_controller) {
    return;
  }

  dialog_controller->ShowMediaRouterDialog(
      media_router::MediaRouterDialogActivationLocation::CONTEXTUAL_MENU);
}

void RenderViewContextMenu::ExecTranslate() {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  if (!chrome_translate_client) {
    return;
  }

  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  manager->ShowTranslateUI(/*auto_translate=*/true,
                           /*triggered_from_menu=*/true);
}

void RenderViewContextMenu::ExecPartialTranslate() {
  std::string source_language;
  std::string target_language;

  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  if (chrome_translate_client) {
    chrome_translate_client->GetTranslateLanguages(
        embedder_web_contents_, &source_language, &target_language,
        /*for_display=*/false);
    GetBrowser()->window()->StartPartialTranslate(
        source_language, target_language, params_.selection_text);
  }
}

void RenderViewContextMenu::ExecLanguageSettings(int event_flags) {
// Open the browser language settings.
// Exception: On Ash, the browser language settings consists solely of a link to
// the OS language settings, so just open the OS settings directly (this has the
// added benefit of also doing the right thing when Lacros is enabled).
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      GetProfile(),
      ash::features::IsOsSettingsRevampWayfindingEnabled()
          ? chromeos::settings::mojom::kLanguagesSubpagePath
          : chromeos::settings::mojom::kLanguagesAndInputSectionPath);
#else
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
  OpenURL(url, GURL(), {}, disposition, ui::PAGE_TRANSITION_LINK);
#endif
}

void RenderViewContextMenu::ExecProtocolHandlerSettings(int event_flags) {
  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  GURL url = chrome::GetSettingsUrl(chrome::kHandlerSettingsSubPage);
  OpenURL(url, GURL(), {}, disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecPictureInPicture() {
  bool picture_in_picture_active =
      IsCommandIdChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE);

  if (picture_in_picture_active) {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_ExitPictureInPicture"));
  } else {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_EnterPictureInPicture"));
  }

  MediaPlayerAction(blink::mojom::MediaPlayerAction(
      blink::mojom::MediaPlayerActionType::kPictureInPicture,
      !picture_in_picture_active));
}

void RenderViewContextMenu::MediaPlayerAction(
    const blink::mojom::MediaPlayerAction& action) {
  if (auto* frame_host = GetRenderFrameHost(); frame_host) {
    frame_host->ExecuteMediaPlayerActionAtLocation(
        gfx::Point(params_.x, params_.y), action);
  }
}

void RenderViewContextMenu::SearchForVideoFrame(
    int event_flags,
    bool is_lens_query,
    const SkBitmap& bitmap,
    const gfx::Rect& region_bounds) {
  if (bitmap.isNull()) {
    return;
  }

  bool lens_overlay_for_video_search_enabled =
      GetBrowser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled() &&
      lens::features::UseLensOverlayForVideoFrameSearch() && is_lens_query;
  bool entered_through_keyboard =
      IsLensOptionEnteredThroughKeyboard(event_flags);
  bool force_open_in_new_tab = false;
  // TODO(crbug/353984457): Clean this branching when the new server
  // results flow is ready.
  if (lens_overlay_for_video_search_enabled) {
    if (entered_through_keyboard) {
      // Using the keyboard to invoke this entry point should use the
      // Lens region search flow through core_tab_helper (with results forced
      // into a new tab) instead of the Lens Overlay flow.
      force_open_in_new_tab = true;
    } else {
      RenderFrameHost* render_frame_host = GetRenderFrameHost();
      if (!render_frame_host) {
        return;
      }

      auto tab_bounds = source_web_contents_->GetViewBounds();
      auto view_bounds = render_frame_host->GetView()->GetViewBounds();
      float device_scale_factor =
          render_frame_host->GetView()->GetDeviceScaleFactor();

      RecordAmbientSearchQuery(
          lens::AmbientSearchEntryPoint::
              CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_LENS_OVERLAY);

      // OpenLensOverlayWithPreselectedRegion() only takes a `ChromeRenderFrame`
      // to keep it alive while the mojo calls run, which is not needed here.
      OpenLensOverlayWithPreselectedRegion(
          /*chrome_render_frame=*/mojo::AssociatedRemote<
              chrome::mojom::ChromeRenderFrame>(),
          tab_bounds, view_bounds, device_scale_factor, bitmap, region_bounds);
      return;
    }
  }

  // If not using Lens overlay for video frame search, fallback to use
  // CoreTabHelper.
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper) {
    return;
  }

  auto image =
      gfx::Image(gfx::ImageSkia::CreateFromBitmap(bitmap, /*scale=*/1));

  if (is_lens_query) {
    RecordAmbientSearchQuery(
        lens_overlay_for_video_search_enabled
            ? lens::AmbientSearchEntryPoint::
                  CONTEXT_MENU_SEARCH_VIDEO_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK
            : lens::AmbientSearchEntryPoint::
                  CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_GOOGLE_LENS);
    core_tab_helper->SearchWithLens(
        image, lens::EntryPoint::CHROME_VIDEO_FRAME_SEARCH_CONTEXT_MENU_ITEM,
        force_open_in_new_tab);
  } else {
    RecordAmbientSearchQuery(lens::AmbientSearchEntryPoint::
                                 CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_WEB);
    core_tab_helper->SearchByImage(image);
  }
}

void RenderViewContextMenu::PluginActionAt(
    const gfx::Point& location,
    blink::mojom::PluginActionType plugin_action) {
  content::RenderFrameHost* plugin_rfh = nullptr;
#if BUILDFLAG(ENABLE_PDF)
  // A PDF plugin exists in a child frame embedded inside the PDF extension's
  // frame. To trigger any plugin action, detect this child frame and trigger
  // the actions from there.
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (chrome_pdf::features::IsOopifPdfEnabled() && IsFrameInPdfViewer(rfh)) {
    // For OOPIF PDF viewer, the current frame should be the PDF plugin frame.
    // The PDF extension frame shouldn't be performing any plugin actions.
    CHECK(rfh->GetProcess()->IsPdf());
    plugin_rfh = rfh;
  } else {
    // For GuestView PDF viewer, find the plugin frame by using the PDF
    // extension frame.
    plugin_rfh = pdf_frame_util::FindPdfChildFrame(
        source_web_contents_->GetPrimaryMainFrame());
  }
#endif
  if (!plugin_rfh) {
    plugin_rfh = source_web_contents_->GetPrimaryMainFrame();
  }

  plugin_rfh->ExecutePluginActionAtLocalLocation(location, plugin_action);

  if (execute_plugin_action_callback_) {
    std::move(execute_plugin_action_callback_).Run(plugin_rfh, plugin_action);
  }
}

Browser* RenderViewContextMenu::GetBrowser() const {
  return chrome::FindBrowserWithTab(embedder_web_contents_);
}

bool RenderViewContextMenu::CanTranslate(bool menu_logging) {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  return chrome_translate_client &&
         chrome_translate_client->GetTranslateManager()->CanManuallyTranslate(
             menu_logging);
}

bool RenderViewContextMenu::CanPartiallyTranslateTargetLanguage() {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  return chrome_translate_client &&
         chrome_translate_client->GetTranslateManager()
             ->CanPartiallyTranslateTargetLanguage();
}

void RenderViewContextMenu::MaybePrepareForLensQuery() {
  if (!search::DefaultSearchProviderIsGoogle(GetProfile())) {
    return;
  }

  // Chrome Search Companion preparation
  if (companion::IsSearchImageInCompanionSidePanelSupported(GetBrowser())) {
    if (companion::GetShouldIssuePreconnectForCompanion()) {
      IssuePreconnectionToUrl(companion::GetPreconnectKeyForCompanion(),
                              companion::GetImageUploadURLForCompanion());
    }
    if (companion::GetShouldIssueProcessPrewarmingForCompanion() &&
        !base::SysInfo::IsLowEndDevice()) {
      content::SpareRenderProcessHostManager::Get().WarmupSpare(
          browser_context_);
    }
    return;
  }

  // Lens Side Panel preparation
  if (lens::features::IsLensSidePanelEnabled()) {
    if (lens::features::GetShouldIssuePreconnectForLens()) {
      IssuePreconnectionToUrl(lens::features::GetPreconnectKeyForLens(),
                              lens::features::GetHomepageURLForLens());
    }
    if (lens::features::GetShouldIssueProcessPrewarmingForLens() &&
        !base::SysInfo::IsLowEndDevice()) {
      content::SpareRenderProcessHostManager::Get().WarmupSpare(
          browser_context_);
    }
    return;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void RenderViewContextMenu::ShowClipboardHistoryMenu(int event_flags) {
  auto* const host_native_view =
      GetRenderFrameHost() ? GetRenderFrameHost()->GetNativeView() : nullptr;
  if (!host_native_view) {
    return;
  }

  // Calculate the anchor point in screen coordinates.
  gfx::Point anchor_point_in_screen =
      host_native_view->GetBoundsInScreen().origin();
  anchor_point_in_screen.Offset(params_.x, params_.y);

  // Calculate the menu source type from `event_flags`.
  const ui::MenuSourceType source_type = ui::GetMenuSourceType(event_flags);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ClipboardHistoryController::Get()->ShowMenu(
      gfx::Rect(anchor_point_in_screen, gfx::Size()), source_type,
      crosapi::mojom::ClipboardHistoryControllerShowSource::
          kRenderViewContextMenu);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (auto* service = chromeos::LacrosService::Get();
      service && service->IsAvailable<crosapi::mojom::ClipboardHistory>()) {
    service->GetRemote<crosapi::mojom::ClipboardHistory>()->ShowClipboard(
        gfx::Rect(anchor_point_in_screen, gfx::Size()), source_type,
        crosapi::mojom::ClipboardHistoryControllerShowSource::
            kRenderViewContextMenu);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif  // BUILDFLAG(IS_CHROMEOS)
