// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_features.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/google/core/common/google_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "net/base/escape.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_media_player_action.h"
#include "third_party/blink/public/web/web_plugin_action.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "chrome/browser/renderer_context_menu/spelling_options_submenu_observer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#include "components/printing/common/print_messages.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_pin_type.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/intent_helper/open_with_menu.h"
#include "chrome/browser/chromeos/arc/intent_helper/start_smart_selection_action_menu.h"
#endif

using base::UserMetricsAction;
using blink::WebContextMenuData;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;
using blink::WebString;
using blink::WebURL;
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

base::OnceCallback<void(RenderViewContextMenu*)>* GetMenuShownCallback() {
  static base::NoDestructor<base::OnceCallback<void(RenderViewContextMenu*)>>
      callback;
  return callback.get();
}

// State of the profile that is activated via "Open Link as User".
enum UmaEnumOpenLinkAsUser {
  OPEN_LINK_AS_USER_ACTIVE_PROFILE_ENUM_ID,
  OPEN_LINK_AS_USER_INACTIVE_PROFILE_MULTI_PROFILE_SESSION_ENUM_ID,
  OPEN_LINK_AS_USER_INACTIVE_PROFILE_SINGLE_PROFILE_SESSION_ENUM_ID,
  OPEN_LINK_AS_USER_LAST_ENUM_ID,
};

// State of other profiles when the "Open Link as User" context menu is shown.
enum UmaEnumOpenLinkAsUserProfilesState {
  OPEN_LINK_AS_USER_PROFILES_STATE_NO_OTHER_ACTIVE_PROFILES_ENUM_ID,
  OPEN_LINK_AS_USER_PROFILES_STATE_OTHER_ACTIVE_PROFILES_ENUM_ID,
  OPEN_LINK_AS_USER_PROFILES_STATE_LAST_ENUM_ID,
};

#if !defined(OS_CHROMEOS)
// We report the number of "Open Link as User" entries shown in the context
// menu via UMA, differentiating between at most that many profiles.
const int kOpenLinkAsUserMaxProfilesReported = 10;
#endif  // !defined(OS_CHROMEOS)

// Whether to return the general enum_id or context_specific_enum_id
// in the FindUMAEnumValueForCommand lookup function.
enum UmaEnumIdLookupType {
  GENERAL_ENUM_ID,
  CONTEXT_SPECIFIC_ENUM_ID,
};

// Maps UMA enumeration to IDC. IDC could be changed so we can't use
// just them and |UMA_HISTOGRAM_CUSTOM_ENUMERATION|.
// Never change mapping or reuse |enum_id|. Always push back new items.
// Items that is not used any more by |RenderViewContextMenu.ExecuteCommand|
// could be deleted, but don't change the rest of |kUmaEnumToControlId|.
//
// |context_specific_enum_id| matches the ContextMenuOption histogram enum.
// Used to track command usage under specific contexts, specifically Menu
// items under 'link + image' and 'selected text'. Should be set to -1 if
// command is not context specific tracked.
const struct UmaEnumCommandIdPair {
  int enum_id;
  int context_specific_enum_id;
  int control_id;
} kUmaEnumToControlId[] = {
    /*
      enum id for 0, 1 are detected using
      RenderViewContextMenu::IsContentCustomCommandId and
      ContextMenuMatcher::IsExtensionsCustomCommandId
    */
    {2, -1, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST},
    {3, 0, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB},
    {4, 15, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW},
    {5, 1, IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD},
    {6, 5, IDC_CONTENT_CONTEXT_SAVELINKAS},
    {7, 18, IDC_CONTENT_CONTEXT_SAVEAVAS},
    {8, 6, IDC_CONTENT_CONTEXT_SAVEIMAGEAS},
    {9, 2, IDC_CONTENT_CONTEXT_COPYLINKLOCATION},
    {10, 10, IDC_CONTENT_CONTEXT_COPYIMAGELOCATION},
    {11, -1, IDC_CONTENT_CONTEXT_COPYAVLOCATION},
    {12, 9, IDC_CONTENT_CONTEXT_COPYIMAGE},
    {13, 8, IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB},
    {14, -1, IDC_CONTENT_CONTEXT_OPENAVNEWTAB},
    {15, -1, IDC_CONTENT_CONTEXT_PLAYPAUSE},
    {16, -1, IDC_CONTENT_CONTEXT_MUTE},
    {17, -1, IDC_CONTENT_CONTEXT_LOOP},
    {18, -1, IDC_CONTENT_CONTEXT_CONTROLS},
    {19, -1, IDC_CONTENT_CONTEXT_ROTATECW},
    {20, -1, IDC_CONTENT_CONTEXT_ROTATECCW},
    {21, -1, IDC_BACK},
    {22, -1, IDC_FORWARD},
    {23, -1, IDC_SAVE_PAGE},
    {24, -1, IDC_RELOAD},
    {25, -1, IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP},
    {26, -1, IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP},
    {27, 16, IDC_PRINT},
    {28, -1, IDC_VIEW_SOURCE},
    {29, -1, IDC_CONTENT_CONTEXT_INSPECTELEMENT},
    {30, -1, IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE},
    {31, -1, IDC_CONTENT_CONTEXT_VIEWPAGEINFO},
    {32, -1, IDC_CONTENT_CONTEXT_TRANSLATE},
    {33, -1, IDC_CONTENT_CONTEXT_RELOADFRAME},
    {34, -1, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE},
    {35, -1, IDC_CONTENT_CONTEXT_VIEWFRAMEINFO},
    {36, -1, IDC_CONTENT_CONTEXT_UNDO},
    {37, -1, IDC_CONTENT_CONTEXT_REDO},
    {38, 28, IDC_CONTENT_CONTEXT_CUT},
    {39, 4, IDC_CONTENT_CONTEXT_COPY},
    {40, 29, IDC_CONTENT_CONTEXT_PASTE},
    {41, -1, IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE},
    {42, -1, IDC_CONTENT_CONTEXT_DELETE},
    {43, -1, IDC_CONTENT_CONTEXT_SELECTALL},
    {44, 17, IDC_CONTENT_CONTEXT_SEARCHWEBFOR},
    {45, -1, IDC_CONTENT_CONTEXT_GOTOURL},
    {46, -1, IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS},
    {47, -1, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS},
    {52, -1, IDC_CONTENT_CONTEXT_OPENLINKWITH},
    {53, -1, IDC_CHECK_SPELLING_WHILE_TYPING},
    {54, -1, IDC_SPELLCHECK_MENU},
    {55, 27, IDC_CONTENT_CONTEXT_SPELLING_TOGGLE},
    {56, -1, IDC_SPELLCHECK_LANGUAGES_FIRST},
    {57, 11, IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE},
    {58, 25, IDC_SPELLCHECK_SUGGESTION_0},
    {59, 26, IDC_SPELLCHECK_ADD_TO_DICTIONARY},
    {60, -1, IDC_SPELLPANEL_TOGGLE},
    {61, -1, IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB},
    {62, -1, IDC_WRITING_DIRECTION_MENU},
    {63, -1, IDC_WRITING_DIRECTION_DEFAULT},
    {64, -1, IDC_WRITING_DIRECTION_LTR},
    {65, -1, IDC_WRITING_DIRECTION_RTL},
    {66, -1, IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE},
    {68, -1, IDC_ROUTE_MEDIA},
    {69, -1, IDC_CONTENT_CONTEXT_COPYLINKTEXT},
    {70, -1, IDC_CONTENT_CONTEXT_OPENLINKINPROFILE},
    {71, -1, IDC_OPEN_LINK_IN_PROFILE_FIRST},
    {72, -1, IDC_CONTENT_CONTEXT_GENERATEPASSWORD},
    {73, -1, IDC_SPELLCHECK_MULTI_LINGUAL},
    {74, -1, IDC_CONTENT_CONTEXT_OPEN_WITH1},
    {75, -1, IDC_CONTENT_CONTEXT_OPEN_WITH2},
    {76, -1, IDC_CONTENT_CONTEXT_OPEN_WITH3},
    {77, -1, IDC_CONTENT_CONTEXT_OPEN_WITH4},
    {78, -1, IDC_CONTENT_CONTEXT_OPEN_WITH5},
    {79, -1, IDC_CONTENT_CONTEXT_OPEN_WITH6},
    {80, -1, IDC_CONTENT_CONTEXT_OPEN_WITH7},
    {81, -1, IDC_CONTENT_CONTEXT_OPEN_WITH8},
    {82, -1, IDC_CONTENT_CONTEXT_OPEN_WITH9},
    {83, -1, IDC_CONTENT_CONTEXT_OPEN_WITH10},
    {84, -1, IDC_CONTENT_CONTEXT_OPEN_WITH11},
    {85, -1, IDC_CONTENT_CONTEXT_OPEN_WITH12},
    {86, -1, IDC_CONTENT_CONTEXT_OPEN_WITH13},
    {87, -1, IDC_CONTENT_CONTEXT_OPEN_WITH14},
    {88, -1, IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN},
    {89, -1, IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP},
    {90, -1, IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS},
    {91, -1, IDC_CONTENT_CONTEXT_PICTUREINPICTURE},
    {92, -1, IDC_CONTENT_CONTEXT_EMOJI},
    {93, -1, IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1},
    {94, -1, IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2},
    {95, -1, IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3},
    {96, -1, IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4},
    {97, -1, IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5},
    {98, -1, IDC_CONTENT_CONTEXT_LOOK_UP},
    // Add new items here and use |enum_id| from the next line.
    // Also, add new items to RenderViewContextMenuItem enum in
    // tools/metrics/histograms/enums.xml.
    {99, -1, 0},  // Must be the last. Increment |enum_id| when new IDC
                  // was added.
};

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
// |use_specific_context_enum| set to true returns the context_specific_enum_id.
int FindUMAEnumValueForCommand(int id, UmaEnumIdLookupType enum_lookup_type) {
  if (RenderViewContextMenu::IsContentCustomCommandId(id))
    return 0;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return 1;

  id = CollapseCommandsForUMA(id);
  const size_t kMappingSize = arraysize(kUmaEnumToControlId);
  for (size_t i = 0; i < kMappingSize; ++i) {
    if (kUmaEnumToControlId[i].control_id == id) {
      if (enum_lookup_type == GENERAL_ENUM_ID)
        return kUmaEnumToControlId[i].enum_id;
      if (enum_lookup_type == CONTEXT_SPECIFIC_ENUM_ID &&
          kUmaEnumToControlId[i].context_specific_enum_id > -1) {
        return kUmaEnumToControlId[i].context_specific_enum_id;
      }
    }
  }

  return -1;
}

// Usually a new tab is expected where this function is used,
// however users should be able to open a tab in background
// or in a new window.
WindowOpenDisposition ForceNewTabDispositionFromEventFlags(
    int event_flags) {
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  return disposition == WindowOpenDisposition::CURRENT_TAB
             ? WindowOpenDisposition::NEW_FOREGROUND_TAB
             : disposition;
}

// Returns the preference of the profile represented by the |context|.
PrefService* GetPrefs(content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context);
}

bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                           const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty())
    return true;
  return patterns.MatchesURL(url);
}

const GURL& GetDocumentURL(const content::ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

content::Referrer CreateReferrer(const GURL& url,
                                 const content::ContextMenuParams& params) {
  const GURL& referring_url = GetDocumentURL(params);
  return content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
}

content::WebContents* GetWebContentsToUse(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If we're viewing in a MimeHandlerViewGuest, use its embedder WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (guest_view)
    return guest_view->embedder_web_contents();
#endif
  return web_contents;
}

void WriteURLToClipboard(const GURL& url) {
  if (url.is_empty() || !url.is_valid())
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  base::string16 text =
      url.SchemeIs(url::kMailToScheme)
          ? base::ASCIIToUTF16(url.path())
          : url_formatter::FormatUrl(
                url, url_formatter::kFormatUrlOmitNothing,
                net::UnescapeRule::NONE, nullptr, nullptr, nullptr);

  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  scw.WriteText(text);
}

bool g_custom_id_ranges_initialized = false;

#if !defined(OS_CHROMEOS)
void AddAvatarToLastMenuItem(const gfx::Image& icon,
                             ui::SimpleMenuModel* menu) {
  // Don't try to scale too small icons.
  if (icon.Width() < 16 || icon.Height() < 16)
    return;
  int target_dip_width = icon.Width();
  int target_dip_height = icon.Height();
  gfx::CalculateFaviconTargetSize(&target_dip_width, &target_dip_height);
  gfx::Image sized_icon = profiles::GetSizedAvatarIcon(
      icon, true /* is_rectangle */, target_dip_width, target_dip_height,
      profiles::SHAPE_CIRCLE);
  menu->SetIcon(menu->GetItemCount() - 1, sized_icon);
}
#endif  // !defined(OS_CHROMEOS)

// Adds Google icon to the last added menu item. Used for Google-powered
// services like translate and search.
void AddGoogleIconToLastMenuItem(ui::SimpleMenuModel* menu) {
#if defined(GOOGLE_CHROME_BUILD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGoogleBrandedContextMenu) ||
      base::FieldTrialList::FindFullName("GoogleBrandedContextMenu") ==
          "branded") {
    menu->SetIcon(
        menu->GetItemCount() - 1,
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_GOOGLE_ICON));
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void OnProfileCreated(const GURL& link_url,
                      const content::Referrer& referrer,
                      Profile* profile,
                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile);
    NavigateParams nav_params(browser, link_url, ui::PAGE_TRANSITION_LINK);
    nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    nav_params.referrer = referrer;
    nav_params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&nav_params);
  }
}

bool DoesInputFieldTypeSupportEmoji(
    blink::WebContextMenuData::InputFieldType text_input_type) {
  // Disable emoji for input types that definitely do not support emoji.
  switch (text_input_type) {
    case blink::WebContextMenuData::kInputFieldTypeNumber:
    case blink::WebContextMenuData::kInputFieldTypeTelephone:
    case blink::WebContextMenuData::kInputFieldTypeOther:
      return false;
    default:
      return true;
  }
}

}  // namespace

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

// static
bool RenderViewContextMenu::IsInternalResourcesURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUISyncResourcesHost;
}

// static
void RenderViewContextMenu::AddSpellCheckServiceItem(ui::SimpleMenuModel* menu,
                                                     bool is_checked) {
  // When the Google branding experiment is enabled, we want to show an icon
  // next to this item, but we can't show icons on check items.  So when the
  // item is enabled show it as checked, and otherwise add it as a normal item
  // (instead of a check item) so that, if necessary, we can add the Google
  // icon.  (If the experiment is off, there's no harm in adding this as a
  // normal item, as it will look identical to an unchecked check item.)
  if (is_checked) {
    menu->AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                                   IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
  } else {
    menu->AddItemWithStringId(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
                              IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
    AddGoogleIconToLastMenuItem(menu);
  }
}

RenderViewContextMenu::RenderViewContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuBase(render_frame_host, params),
      extension_items_(browser_context_,
                       this,
                       &menu_model_,
                       base::Bind(MenuItemMatchesParams, params_)),
      profile_link_submenu_model_(this),
      multiple_profiles_open_(false),
      protocol_handler_submenu_model_(this),
      protocol_handler_registry_(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile())),
      embedder_web_contents_(GetWebContentsToUse(source_web_contents_)) {
  if (!g_custom_id_ranges_initialized) {
    g_custom_id_ranges_initialized = true;
    SetContentCustomCommandIdRange(IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
                                   IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  }
  set_content_type(ContextMenuContentTypeFactory::Create(
      source_web_contents_, params));
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool RenderViewContextMenu::ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    const MenuItem::ContextList& contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_frame = !params.frame_url.is_empty();

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_frame && contexts.Contains(MenuItem::FRAME)))
    return true;

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url))
    return true;

  switch (params.media_type) {
    case WebContextMenuData::kMediaTypeImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::kMediaTypeVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::kMediaTypeAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == WebContextMenuData::kMediaTypeNone &&
      contexts.Contains(MenuItem::PAGE))
    return true;

  return false;
}

// static
bool RenderViewContextMenu::MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match)
    return false;

  const GURL& document_url = GetDocumentURL(params);
  return ExtensionPatternMatch(item->document_url_patterns(), document_url);
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_items_.Clear();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  if (!service)
    return;  // In unit-tests, we may not have an ExtensionService.

  MenuManager* menu_manager = MenuManager::Get(browser_context_);
  if (!menu_manager)
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::set<MenuItem::ExtensionKey> ids = menu_manager->ExtensionIds();
  std::vector<base::string16> sorted_menu_titles;
  std::map<base::string16, std::vector<const Extension*>>
      title_to_extensions_map;
  for (auto iter = ids.begin(); iter != ids.end(); ++iter) {
    const Extension* extension =
        service->GetExtensionById(iter->extension_id, false);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      base::string16 menu_title = extension_items_.GetTopLevelContextMenuTitle(
          *iter, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty())
    return;

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
  if (!extension)
    return;

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(source_web_contents_);
  MenuItem::ExtensionKey key;
  if (web_view_guest) {
    key = MenuItem::ExtensionKey(extension->id(),
                                 web_view_guest->owner_web_contents()
                                     ->GetMainFrame()
                                     ->GetProcess()
                                     ->GetID(),
                                 web_view_guest->view_instance_id());
  } else {
    key = MenuItem::ExtensionKey(extension->id());
  }

  // Only add extension items from this extension.
  int index = 0;
  extension_items_.AppendExtensionItems(key, PrintableSelectionText(), &index,
                                        /*is_action_menu=*/false);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void RenderViewContextMenu::InitMenu() {
  RenderViewContextMenuBase::InitMenu();

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PASSWORD)) {
    AppendPasswordItems();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    AppendPageItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::kMediaTypeNone)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  bool media_image = content_type_->SupportsGroup(
      ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE);
  if (media_image)
    AppendImageItems();

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
  if (editable)
    AppendEditableItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY)) {
    DCHECK(!editable);
    AppendCopyItem();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER) &&
      params_.misspelled_word.empty()) {
    AppendSearchProvider();
  }

  if (!media_image &&
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)) {
    AppendPrintItem();
  }

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
#if defined(OS_CHROMEOS)
  supports_smart_text_selection =
      base::FeatureList::IsEnabled(arc::kSmartTextSelectionFeature) &&
      content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SMART_SELECTION) &&
      arc::IsArcPlayStoreEnabledForProfile(GetProfile());
#endif  // defined(OS_CHROMEOS)
  if (supports_smart_text_selection)
    AppendSmartSelectionActionItems();

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
  int index = menu_model_.GetItemCount() - 1;
  if (index >= 0 &&
      menu_model_.GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
    menu_model_.RemoveItemAt(index);
  }
}

Profile* RenderViewContextMenu::GetProfile() const {
  return Profile::FromBrowserContext(browser_context_);
}

void RenderViewContextMenu::RecordUsedItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id, GENERAL_ENUM_ID);
  if (enum_id == -1) {
    NOTREACHED() << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
    return;
  }

  const size_t kMappingSize = arraysize(kUmaEnumToControlId);
  UMA_HISTOGRAM_EXACT_LINEAR("RenderViewContextMenu.Used", enum_id,
                             kUmaEnumToControlId[kMappingSize - 1].enum_id);
  // Record to additional context specific histograms.
  enum_id = FindUMAEnumValueForCommand(id, CONTEXT_SPECIFIC_ENUM_ID);

  // Linked image context.
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK) &&
      content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    UMA_HISTOGRAM_EXACT_LINEAR("ContextMenu.SelectedOption.ImageLink", enum_id,
                               kUmaEnumToControlId[kMappingSize - 1].enum_id);
  }
  // Selected text context.
  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER) &&
      content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)) {
    UMA_HISTOGRAM_EXACT_LINEAR("ContextMenu.SelectedOption.SelectedText",
                               enum_id,
                               kUmaEnumToControlId[kMappingSize - 1].enum_id);
  }
  // Misspelled word context.
  if (!params_.misspelled_word.empty()) {
    UMA_HISTOGRAM_EXACT_LINEAR("ContextMenu.SelectedOption.MisspelledWord",
                               enum_id,
                               kUmaEnumToControlId[kMappingSize - 1].enum_id);
  }
}

void RenderViewContextMenu::RecordShownItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id, GENERAL_ENUM_ID);
  if (enum_id != -1) {
    const size_t kMappingSize = arraysize(kUmaEnumToControlId);
    UMA_HISTOGRAM_EXACT_LINEAR("RenderViewContextMenu.Shown", enum_id,
                               kUmaEnumToControlId[kMappingSize - 1].enum_id);
  } else {
    // Just warning here. It's harder to maintain list of all possibly
    // visible items than executable items.
    DLOG(ERROR) << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

bool RenderViewContextMenu::IsHTML5Fullscreen() const {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents_);
  if (!browser)
    return false;

  FullscreenController* controller =
      browser->exclusive_access_manager()->fullscreen_controller();
  return controller->IsTabFullscreen();
}

bool RenderViewContextMenu::IsPressAndHoldEscRequiredToExitFullscreen() const {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents_);
  if (!browser)
    return false;

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

std::string RenderViewContextMenu::GetTargetLanguage() const {
  std::unique_ptr<translate::TranslatePrefs> prefs(
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefs(browser_context_)));
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(browser_context_)
          ->GetPrimaryModel();
  return translate::TranslateManager::GetTargetLanguage(prefs.get(),
                                                        language_model);
}

void RenderViewContextMenu::AppendDeveloperItems() {
  // Show Inspect Element in DevTools itself only in case of the debug
  // devtools build.
  bool show_developer_items = !IsDevToolsURL(params_.page_url);

#if BUILDFLAG(DEBUG_DEVTOOLS)
  show_developer_items = true;
#endif

  if (!show_developer_items)
    return;

  // In the DevTools popup menu, "developer items" is normally the only
  // section, so omit the separator there.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
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
    if (base::FeatureList::IsEnabled(features::kDesktopPWAWindowing)) {
      const Browser* browser = GetBrowser();
      const bool is_app = browser && browser->is_app();

      AppendOpenInBookmarkAppLinkItems();

      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
          is_app ? IDS_CONTENT_CONTEXT_OPENLINKNEWTAB_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
      if (!is_app) {
        menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                        IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
      }

      if (params_.link_url.is_valid()) {
        AppendProtocolHandlerSubMenu();
      }

      menu_model_.AddItemWithStringId(
          IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
          is_app ? IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD_INAPP
                 : IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);

    } else {
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                      IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                      IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
      if (params_.link_url.is_valid()) {
        AppendProtocolHandlerSubMenu();
      }

      menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                                      IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
    }

    AppendOpenWithLinkItems();

    // While ChromeOS supports multiple profiles, only one can be open at a
    // time.
    // TODO(jochen): Consider adding support for ChromeOS with similar
    // semantics as the profile switcher in the system tray.
#if !defined(OS_CHROMEOS)
    // g_browser_process->profile_manager() is null during unit tests.
    if (g_browser_process->profile_manager() &&
        GetProfile()->GetProfileType() == Profile::REGULAR_PROFILE) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      // Find all regular profiles other than the current one which have at
      // least one open window.
      std::vector<ProfileAttributesEntry*> entries =
          profile_manager->GetProfileAttributesStorage().
              GetAllProfilesAttributesSortedByName();
      std::vector<ProfileAttributesEntry*> target_profiles_entries;
      for (ProfileAttributesEntry* entry : entries) {
        base::FilePath profile_path = entry->GetPath();
        Profile* profile = profile_manager->GetProfileByPath(profile_path);
        if (profile != GetProfile() &&
            !entry->IsOmitted() && !entry->IsSigninRequired()) {
          target_profiles_entries.push_back(entry);
          if (chrome::FindLastActiveWithProfile(profile))
            multiple_profiles_open_ = true;
        }
      }

      if (!target_profiles_entries.empty()) {
        UmaEnumOpenLinkAsUserProfilesState profiles_state;
        if (multiple_profiles_open_) {
          profiles_state =
              OPEN_LINK_AS_USER_PROFILES_STATE_OTHER_ACTIVE_PROFILES_ENUM_ID;
        } else {
          profiles_state =
              OPEN_LINK_AS_USER_PROFILES_STATE_NO_OTHER_ACTIVE_PROFILES_ENUM_ID;
        }
        UMA_HISTOGRAM_ENUMERATION(
            "RenderViewContextMenu.OpenLinkAsUserProfilesState", profiles_state,
            OPEN_LINK_AS_USER_PROFILES_STATE_LAST_ENUM_ID);

        UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.OpenLinkAsUserShown",
                                  target_profiles_entries.size(),
                                  kOpenLinkAsUserMaxProfilesReported);
      }

      if (multiple_profiles_open_ && !target_profiles_entries.empty()) {
        if (target_profiles_entries.size() == 1) {
          int menu_index = static_cast<int>(profile_link_paths_.size());
          ProfileAttributesEntry* entry = target_profiles_entries.front();
          profile_link_paths_.push_back(entry->GetPath());
          menu_model_.AddItem(
              IDC_OPEN_LINK_IN_PROFILE_FIRST + menu_index,
              l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_OPENLINKINPROFILE,
                                         entry->GetName()));
          AddAvatarToLastMenuItem(entry->GetAvatarIcon(), &menu_model_);
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
            AddAvatarToLastMenuItem(entry->GetAvatarIcon(),
                                    &profile_link_submenu_model_);
          }
          menu_model_.AddSubMenuWithStringId(
              IDC_CONTENT_CONTEXT_OPENLINKINPROFILE,
              IDS_CONTENT_CONTEXT_OPENLINKINPROFILES,
              &profile_link_submenu_model_);
        }
      }
    }
#endif  // !defined(OS_CHROMEOS)
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
      params_.link_url.SchemeIs(url::kMailToScheme) ?
          IDS_CONTENT_CONTEXT_COPYEMAILADDRESS :
          IDS_CONTENT_CONTEXT_COPYLINKLOCATION);

  if (params_.source_type == ui::MENU_SOURCE_TOUCH &&
      params_.media_type != WebContextMenuData::kMediaTypeImage &&
      !params_.link_text.empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKTEXT,
                                    IDS_CONTENT_CONTEXT_COPYLINKTEXT);
  }
}

void RenderViewContextMenu::AppendOpenWithLinkItems() {
#if defined(OS_CHROMEOS)
  open_with_menu_observer_ =
      std::make_unique<arc::OpenWithMenu>(browser_context_, this);
  observers_.AddObserver(open_with_menu_observer_.get());
  open_with_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendSmartSelectionActionItems() {
#if defined(OS_CHROMEOS)
  start_smart_selection_action_menu_observer_ =
      std::make_unique<arc::StartSmartSelectionActionMenu>(this);
  observers_.AddObserver(start_smart_selection_action_menu_observer_.get());

  if (menu_model_.GetItemCount())
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  start_smart_selection_action_menu_observer_->InitMenu(params_);
#endif
}

void RenderViewContextMenu::AppendOpenInBookmarkAppLinkItems() {
  const Extension* pwa = extensions::util::GetInstalledPwaForUrl(
      browser_context_, params_.link_url);
  if (!pwa)
    return;

  int open_in_app_string_id;
  const Browser* browser = GetBrowser();
  if (browser && browser->app_name() ==
                     web_app::GenerateApplicationNameFromAppId(pwa->id())) {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP_SAMEAPP;
  } else {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP;
  }

  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
      l10n_util::GetStringFUTF16(open_in_app_string_id,
                                 base::UTF8ToUTF16(pwa->short_name())));

  MenuManager* menu_manager = MenuManager::Get(browser_context_);
  gfx::Image icon = menu_manager->GetIconForExtension(pwa->id());
  menu_model_.SetIcon(menu_model_.GetItemCount() - 1, icon);
}

void RenderViewContextMenu::AppendImageItems() {
  std::map<std::string, std::string>::const_iterator it =
      params_.properties.find(
          data_reduction_proxy::chrome_proxy_content_transform_header());
  if (it != params_.properties.end() && it->second ==
      data_reduction_proxy::empty_image_directive()) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE,
        IDS_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE);
  }
  DataReductionProxyChromeSettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context_);
  if (settings && settings->CanUseDataReductionProxy(params_.src_url)) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB,
        IDS_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB);
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                    IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                  IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
}

void RenderViewContextMenu::AppendSearchWebForImageItems() {
  if (!params_.has_image_contents)
    return;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* const provider = service->GetDefaultSearchProvider();
  if (!provider || provider->image_url().empty() ||
      !provider->image_url_ref().IsValid(service->search_terms_data())) {
    return;
  }

  menu_model_.AddItem(
      IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
      l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
                                 provider->short_name()));
  if (provider->image_url_ref().HasGoogleBaseURLs(service->search_terms_data()))
    AddGoogleIconToLastMenuItem(&menu_model_);
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
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendPictureInPictureItem();
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendMediaItems() {
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_LOOP,
                                       IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_CONTROLS,
                                       IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPluginItems() {
  if (params_.page_url == params_.src_url ||
      (guest_view::GuestViewBase::IsGuest(source_web_contents_) &&
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
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                    IDS_CONTENT_CONTEXT_SAVEPAGEAS);
    // The "Print" menu item should always be included for plugins. If
    // content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)
    // is true the item will be added inside AppendPrintItem(). Otherwise we
    // add "Print" here.
    if (!content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT))
      menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
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
  AppendMediaRouterItem();

  if (TranslateService::IsTranslatableURL(params_.page_url)) {
    std::unique_ptr<translate::TranslatePrefs> prefs(
        ChromeTranslateClient::CreateTranslatePrefs(
            GetPrefs(browser_context_)));
    if (prefs->IsTranslateAllowedByPolicy()) {
      language::LanguageModel* language_model =
          LanguageModelManagerFactory::GetForBrowserContext(browser_context_)
              ->GetPrimaryModel();
      std::string locale = translate::TranslateManager::GetTargetLanguage(
          prefs.get(), language_model);
      base::string16 language =
          l10n_util::GetDisplayNameForLocale(locale, locale, true);
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_TRANSLATE,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
      AddGoogleIconToLastMenuItem(&menu_model_);
    }
  }
}

void RenderViewContextMenu::AppendExitFullscreenItem() {
  Browser* browser = GetBrowser();
  if (!browser)
    return;

  // Only show item if in fullscreen mode.
  if (!browser->exclusive_access_manager()
           ->fullscreen_controller()
           ->IsControllerInitiatedFullscreen()) {
    return;
  }

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN,
                                  IDS_CONTENT_CONTEXT_EXIT_FULLSCREEN);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendCopyItem() {
  if (menu_model_.GetItemCount())
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendPrintItem() {
  if (GetPrefs(browser_context_)->GetBoolean(prefs::kPrintingEnabled) &&
      (params_.media_type == WebContextMenuData::kMediaTypeNone ||
       params_.media_flags & WebContextMenuData::kMediaCanPrint) &&
      params_.misspelled_word.empty()) {
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendMediaRouterItem() {
  if (media_router::MediaRouterEnabled(browser_context_)) {
    menu_model_.AddItemWithStringId(IDC_ROUTE_MEDIA,
                                    IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  }
}

void RenderViewContextMenu::AppendRotationItems() {
  if (params_.media_flags & WebContextMenuData::kMediaCanRotate) {
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
  if (params_.selection_text.empty())
    return;

  base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                     base::ASCIIToUTF16(" "), &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())->Classify(
      params_.selection_text,
      false,
      false,
      metrics::OmniboxEventProto::INVALID_SPEC,
      &match,
      NULL);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(GetProfile())
            ->GetDefaultSearchProvider();
    if (!default_provider)
      return;
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                   default_provider->short_name(),
                                   printable_selection_text));
    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(GetProfile());
    if (default_provider->url_ref().HasGoogleBaseURLs(
            service->search_terms_data())) {
      AddGoogleIconToLastMenuItem(&menu_model_);
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

void RenderViewContextMenu::AppendEditableItems() {
  const bool use_spelling = !chrome::IsRunningInForcedAppMode();
  if (use_spelling)
    AppendSpellingSuggestionItems();

  if (!params_.misspelled_word.empty()) {
    AppendSearchProvider();
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
  if (params_.misspelled_word.empty() &&
      DoesInputFieldTypeSupportEmoji(params_.input_field_type) &&
      ui::IsEmojiPanelSupported()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_EMOJI,
                                    IDS_CONTENT_CONTEXT_EMOJI);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

// 'Undo' and 'Redo' for text input with no suggestions and no text selected.
// We make an exception for OS X as context clicking will select the closest
// word. In this case both items are always shown.
#if defined(OS_MACOSX)
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
       menu_model_.GetIndexOfCommandId(IDC_CONTENT_CONTEXT_EMOJI) != -1)) {
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
  if (params_.misspelled_word.empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                                    IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SELECTALL,
                                    IDS_CONTENT_CONTEXT_SELECTALL);
  }

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendLanguageSettings() {
  const bool use_spelling = !chrome::IsRunningInForcedAppMode();
  if (!use_spelling)
    return;

#if defined(OS_MACOSX)
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

void RenderViewContextMenu::AppendProtocolHandlerSubMenu() {
  const ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;
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
  bool add_separator = false;

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetRenderFrameHost());
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

  if (add_separator)
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendPictureInPictureItem() {
  if (base::FeatureList::IsEnabled(media::kPictureInPicture))
    menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_PICTUREINPICTURE,
                                         IDS_CONTENT_CONTEXT_PICTUREINPICTURE);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
#if defined(OS_CHROMEOS)
  // Disable context menu in locked fullscreen mode (the menu is not really
  // disabled as the user can still open it, but all the individual context menu
  // entries are disabled / greyed out).
  if (GetBrowser() && ash::IsWindowTrustedPinned(GetBrowser()->window()))
    return false;
#endif

  {
    bool enabled = false;
    if (RenderViewContextMenuBase::IsCommandIdKnown(id, &enabled))
      return enabled;
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  int content_restrictions = 0;
  if (core_tab_helper)
    content_restrictions = core_tab_helper->content_restrictions();
  if (id == IDC_PRINT && (content_restrictions & CONTENT_RESTRICTION_PRINT))
    return false;

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
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdEnabled(id);

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return true;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return params_.link_url.is_valid();
  }

  switch (id) {
    case IDC_BACK:
      return embedder_web_contents_->GetController().CanGoBack();

    case IDC_FORWARD:
      return embedder_web_contents_->GetController().CanGoForward();

    case IDC_RELOAD:
      return IsReloadEnabled();

    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return IsViewSourceEnabled();

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      return IsDevCommandEnabled(id);

    case IDC_CONTENT_CONTEXT_TRANSLATE:
      return IsTranslateEnabled();

    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDC_CONTENT_CONTEXT_OPENLINKINPROFILE:
    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      return params_.link_url.is_valid();

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
    case IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE:
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      return params_.src_url.is_valid() &&
          (params_.src_url.scheme() != content::kChromeUIScheme);

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return params_.has_image_contents;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
    case IDC_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags & WebContextMenuData::kMediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDC_CONTENT_CONTEXT_MUTE:
      return (params_.media_flags & WebContextMenuData::kMediaHasAudio) != 0 &&
             (params_.media_flags & WebContextMenuData::kMediaInError) == 0;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags &
              WebContextMenuData::kMediaCanToggleControls) != 0;

    case IDC_CONTENT_CONTEXT_ROTATECW:
    case IDC_CONTENT_CONTEXT_ROTATECCW:
      return (params_.media_flags & WebContextMenuData::kMediaCanRotate) != 0;

    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
      return IsSaveAsEnabled();

    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      // Currently, a media element can be opened in a new tab iff it can
      // be saved. So rather than duplicating the MediaCanSave flag, we rely
      // on that here.
      return !!(params_.media_flags & WebContextMenuData::kMediaCanSave);

    case IDC_SAVE_PAGE:
      return IsSavePageEnabled();

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      return params_.frame_url.is_valid() &&
             params_.frame_url.GetOrigin() != chrome::kChromeUIPrintURL;

    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & WebContextMenuData::kCanUndo);

    case IDC_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & WebContextMenuData::kCanRedo);

    case IDC_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & WebContextMenuData::kCanCut);

    case IDC_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & WebContextMenuData::kCanCopy);

    case IDC_CONTENT_CONTEXT_PASTE:
      return IsPasteEnabled();

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      return IsPasteAndMatchStyleEnabled();

    case IDC_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & WebContextMenuData::kCanDelete);

    case IDC_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & WebContextMenuData::kCanSelectAll);

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return IsOpenLinkOTREnabled();

    case IDC_PRINT:
      return IsPrintPreviewEnabled();

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      return true;
    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);

#if !defined(OS_MACOSX) && defined(OS_POSIX)
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

    case IDC_CONTENT_CONTEXT_EXIT_FULLSCREEN:
      return true;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      return !!(params_.media_flags &
                WebContextMenuData::kMediaCanPictureInPicture);

    case IDC_CONTENT_CONTEXT_EMOJI:
      return params_.is_editable;

    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION2:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION3:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION4:
    case IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION5:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  if (RenderViewContextMenuBase::IsCommandIdChecked(id))
    return true;

  // See if the video is set to looping.
  if (id == IDC_CONTENT_CONTEXT_LOOP)
    return (params_.media_flags & WebContextMenuData::kMediaLoop) != 0;

  if (id == IDC_CONTENT_CONTEXT_CONTROLS)
    return (params_.media_flags & WebContextMenuData::kMediaControls) != 0;

  if (id == IDC_CONTENT_CONTEXT_PICTUREINPICTURE)
    return (params_.media_flags & WebContextMenuData::kMediaPictureInPicture) !=
           0;

  if (id == IDC_CONTENT_CONTEXT_EMOJI)
    return false;

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdChecked(id);

  return false;
}

bool RenderViewContextMenu::IsCommandIdVisible(int id) const {
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdVisible(id);
  return RenderViewContextMenuBase::IsCommandIdVisible(id);
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  RenderViewContextMenuBase::ExecuteCommand(id, event_flags);
  if (command_executed_)
    return;
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
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              WindowOpenDisposition::NEW_BACKGROUND_TAB,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURLWithExtraHeaders(params_.link_url, GetDocumentURL(params_),
                              WindowOpenDisposition::NEW_WINDOW,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURLWithExtraHeaders(params_.link_url, GURL(),
                              WindowOpenDisposition::OFF_THE_RECORD,
                              ui::PAGE_TRANSITION_LINK, "" /* extra_headers */,
                              true /* started_from_context_menu */);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP:
      ExecOpenBookmarkApp();
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      ExecSaveLinkAs();
      break;

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
      ExecSaveAs();
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
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
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      ExecSearchWebForImage();
      break;

    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
      OpenURLWithExtraHeaders(
          params_.src_url, GetDocumentURL(params_),
          WindowOpenDisposition::NEW_BACKGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          data_reduction_proxy::chrome_proxy_pass_through_header(), false);
      break;

    case IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE:
      ExecLoadOriginalImage();
      break;

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(params_.src_url, GetDocumentURL(params_),
              WindowOpenDisposition::NEW_BACKGROUND_TAB,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
      ExecPlayPause();
      break;

    case IDC_CONTENT_CONTEXT_MUTE:
      ExecMute();
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

    case IDC_BACK:
      embedder_web_contents_->GetController().GoBack();
      break;

    case IDC_FORWARD:
      embedder_web_contents_->GetController().GoForward();
      break;

    case IDC_SAVE_PAGE:
      embedder_web_contents_->OnSavePage();
      break;

    case IDC_RELOAD:
      embedder_web_contents_->GetController().Reload(
          content::ReloadType::NORMAL, true);
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
      ExecExitFullscreen();
      break;

    case IDC_VIEW_SOURCE:
      embedder_web_contents_->GetMainFrame()->ViewSource();
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

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      // We always obey the cache here.
      // TODO(evanm): Perhaps we could allow shift-clicking the menu item to do
      // a cache-ignoring reload of the frame.
      source_web_contents_->ReloadFocusedFrame(false);
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      if (GetRenderFrameHost())
        GetRenderFrameHost()->ViewSource();
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

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL:
      OpenURL(selection_navigation_url_, GURL(),
              ForceNewTabDispositionFromEventFlags(event_flags),
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      ExecLanguageSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
      ExecProtocolHandlerSettings(event_flags);
      break;

    case IDC_CONTENT_CONTEXT_GENERATEPASSWORD:
      password_manager_util::UserTriggeredManualGenerationFromContextMenu(
          ChromePasswordManagerClient::FromWebContents(source_web_contents_));
      break;

    case IDC_CONTENT_CONTEXT_SHOWALLSAVEDPASSWORDS:
      password_manager_util::UserTriggeredShowAllSavedPasswordsFromContextMenu(
          autofill::ChromeAutofillClient::FromWebContents(
              source_web_contents_));
      break;

    case IDC_CONTENT_CONTEXT_PICTUREINPICTURE:
      ExecPictureInPicture();
      break;

    case IDC_CONTENT_CONTEXT_EMOJI:
      ui::ShowEmojiPanel();
      break;

    default:
      NOTREACHED();
      break;
  }
}

void RenderViewContextMenu::AddSpellCheckServiceItem(bool is_checked) {
  AddSpellCheckServiceItem(&menu_model_, is_checked);
}

// static
void RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
    base::OnceCallback<void(RenderViewContextMenu*)> cb) {
  *GetMenuShownCallback() = std::move(cb);
}

ProtocolHandlerRegistry::ProtocolHandlerList
    RenderViewContextMenu::GetHandlersForLinkUrl() {
  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      protocol_handler_registry_->GetHandlersFor(params_.link_url.scheme());
  std::sort(handlers.begin(), handlers.end());
  return handlers;
}

void RenderViewContextMenu::NotifyMenuShown() {
  auto* cb = GetMenuShownCallback();
  if (!cb->is_null())
    std::move(*cb).Run(this);
}

base::string16 RenderViewContextMenu::PrintableSelectionText() {
  return gfx::TruncateString(params_.selection_text,
                             kMaxSelectionTextLength,
                             gfx::WORD_BREAK);
}

void RenderViewContextMenu::EscapeAmpersands(base::string16* text) {
  base::ReplaceChars(*text, base::ASCIIToUTF16("&"), base::ASCIIToUTF16("&&"),
                     text);
}

// Controller functions --------------------------------------------------------

bool RenderViewContextMenu::IsReloadEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  if (!core_tab_helper)
    return false;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  return !core_delegate ||
         core_delegate->CanReloadContents(embedder_web_contents_);
}

bool RenderViewContextMenu::IsViewSourceEnabled() const {
  if (!!extensions::MimeHandlerViewGuest::FromWebContents(
          source_web_contents_)) {
    return false;
  }
  return (params_.media_type != WebContextMenuData::kMediaTypePlugin) &&
         embedder_web_contents_->GetController().CanViewSource();
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) {
    PrefService* prefs = GetPrefs(browser_context_);
    if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
      return false;

    // Don't enable the web inspector if the developer tools are disabled via
    // the preference dev-tools-disabled.
    if (!DevToolsWindow::AllowDevToolsFor(GetProfile(), source_web_contents_))
      return false;
  }

  return true;
}

bool RenderViewContextMenu::IsTranslateEnabled() const {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  // If no |chrome_translate_client| attached with this WebContents or we're
  // viewing in a MimeHandlerViewGuest translate will be disabled.
  if (!chrome_translate_client ||
      !!extensions::MimeHandlerViewGuest::FromWebContents(
          source_web_contents_)) {
    return false;
  }
  std::string original_lang =
      chrome_translate_client->GetLanguageState().original_language();
  std::string target_lang = GetTargetLanguage();
  // Note that we intentionally enable the menu even if the original and
  // target languages are identical.  This is to give a way to user to
  // translate a page that might contains text fragments in a different
  // language.
  return ((params_.edit_flags & WebContextMenuData::kCanTranslate) != 0) &&
         !original_lang.empty() &&  // Did we receive the page language yet?
         !chrome_translate_client->GetLanguageState().IsPageTranslated() &&
         !embedder_web_contents_->GetInterstitialPage() &&
         // There are some application locales which can't be used as a
         // target language for translation. In that case GetTargetLanguage()
         // may return empty.
         !target_lang.empty() &&
         // Disable on the Instant Extended NTP.
         !search::IsInstantNTP(embedder_web_contents_);
}

bool RenderViewContextMenu::IsSaveLinkAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  return params_.link_url.is_valid() &&
      ProfileIOData::IsHandledProtocol(params_.link_url.scheme());
}

bool RenderViewContextMenu::IsSaveImageAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  return params_.has_image_contents;
}

bool RenderViewContextMenu::IsSaveAsEnabled() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  const GURL& url = params_.src_url;
  bool can_save = (params_.media_flags & WebContextMenuData::kMediaCanSave) &&
                  url.is_valid() &&
                  ProfileIOData::IsHandledProtocol(url.scheme());
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Do not save the preview PDF on the print preview page.
  can_save = can_save &&
      !(printing::PrintPreviewDialogController::IsPrintPreviewURL(url));
#endif
  return can_save;
}

bool RenderViewContextMenu::IsSavePageEnabled() const {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(embedder_web_contents_);
  if (!core_tab_helper)
    return false;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  if (core_delegate &&
      !core_delegate->CanSaveContents(embedder_web_contents_))
    return false;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Test if file-selection dialogs are forbidden by policy.
  if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
    return false;

  // We save the last committed entry (which the user is looking at), as
  // opposed to any pending URL that hasn't committed yet.
  NavigationEntry* entry =
      embedder_web_contents_->GetController().GetLastCommittedEntry();
  return content::IsSavableURL(entry ? entry->GetURL() : GURL());
}

bool RenderViewContextMenu::IsPasteEnabled() const {
  if (!(params_.edit_flags & WebContextMenuData::kCanPaste))
    return false;

  std::vector<base::string16> types;
  bool ignore;
  ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &types, &ignore);
  return !types.empty();
}

bool RenderViewContextMenu::IsPasteAndMatchStyleEnabled() const {
  if (!(params_.edit_flags & WebContextMenuData::kCanPaste))
    return false;

  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(),
      ui::CLIPBOARD_TYPE_COPY_PASTE);
}

bool RenderViewContextMenu::IsPrintPreviewEnabled() const {
  if (params_.media_type != WebContextMenuData::kMediaTypeNone &&
      !(params_.media_flags & WebContextMenuData::kMediaCanPrint)) {
    return false;
  }

  Browser* browser = GetBrowser();
  return browser && chrome::CanPrint(browser);
}

bool RenderViewContextMenu::IsRouteMediaEnabled() const {
  if (!media_router::MediaRouterEnabled(browser_context_))
    return false;

  Browser* browser = GetBrowser();
  if (!browser)
    return false;

  // Disable the command if there is an active modal dialog.
  // We don't use |source_web_contents_| here because it could be the
  // WebContents for something that's not the current tab (e.g., WebUI
  // modal dialog).
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return !manager || !manager->IsDialogActive();
}

bool RenderViewContextMenu::IsOpenLinkOTREnabled() const {
  if (browser_context_->IsOffTheRecord() || !params_.link_url.is_valid())
    return false;

  if (!IsURLAllowedInIncognito(params_.link_url, browser_context_))
    return false;

  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(GetPrefs(browser_context_));
  return incognito_avail != IncognitoModePrefs::DISABLED;
}

void RenderViewContextMenu::ExecOpenBookmarkApp() {
  const Extension* pwa = extensions::util::GetInstalledPwaForUrl(
      browser_context_, params_.link_url);
  // |pwa| could be null if it has been uninstalled since the user
  // opened the context menu.
  if (!pwa)
    return;

  AppLaunchParams launch_params(
      GetProfile(), pwa, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::CURRENT_TAB, extensions::SOURCE_CONTEXT_MENU);
  launch_params.override_url = params_.link_url;
  OpenApplication(launch_params);
}

void RenderViewContextMenu::ExecProtocolHandler(int event_flags,
                                                int handler_index) {
  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;

  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Open"));
  WindowOpenDisposition disposition =
      ForceNewTabDispositionFromEventFlags(event_flags);
  OpenURL(handlers[handler_index].TranslateUrl(params_.link_url),
          GetDocumentURL(params_),
          disposition,
          ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecOpenLinkInProfile(int profile_index) {
  DCHECK_GE(profile_index, 0);
  DCHECK_LE(profile_index, static_cast<int>(profile_link_paths_.size()));

  base::FilePath profile_path = profile_link_paths_[profile_index];

  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  UmaEnumOpenLinkAsUser profile_state;
  if (chrome::FindLastActiveWithProfile(profile)) {
    profile_state = OPEN_LINK_AS_USER_ACTIVE_PROFILE_ENUM_ID;
  } else if (multiple_profiles_open_) {
    profile_state =
        OPEN_LINK_AS_USER_INACTIVE_PROFILE_MULTI_PROFILE_SESSION_ENUM_ID;
  } else {
    profile_state =
        OPEN_LINK_AS_USER_INACTIVE_PROFILE_SINGLE_PROFILE_SESSION_ENUM_ID;
  }
  UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.OpenLinkAsUser",
                            profile_state, OPEN_LINK_AS_USER_LAST_ENUM_ID);

  profiles::SwitchToProfile(
      profile_path, false,
      base::Bind(OnProfileCreated, params_.link_url,
                 CreateReferrer(params_.link_url, params_)),
      ProfileMetrics::SWITCH_PROFILE_CONTEXT_MENU);
}

void RenderViewContextMenu::ExecInspectElement() {
  base::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  DevToolsWindow::InspectElement(render_frame_host, params_.x, params_.y);
}

void RenderViewContextMenu::ExecInspectBackgroundPage() {
  const Extension* platform_app = GetExtension();
  DCHECK(platform_app);
  DCHECK(platform_app->is_platform_app());

  extensions::devtools_util::InspectBackgroundPage(platform_app, GetProfile());
}

void RenderViewContextMenu::ExecSaveLinkAs() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;

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
      render_frame_host->GetRenderViewHost()->GetRoutingID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
  content::Referrer referrer = CreateReferrer(url, params_);
  dl_params->set_referrer(referrer.url);
  dl_params->set_referrer_policy(
      content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  dl_params->set_referrer_encoding(params_.frame_charset);
  dl_params->set_suggested_name(params_.suggested_filename);
  dl_params->set_prompt(true);
  dl_params->set_download_source(download::DownloadSource::CONTEXT_MENU);

  BrowserContext::GetDownloadManager(browser_context_)
      ->DownloadUrl(std::move(dl_params));
}

void RenderViewContextMenu::ExecSaveAs() {
  bool is_large_data_url = params_.has_image_contents &&
      params_.src_url.is_empty();
  if (params_.media_type == WebContextMenuData::kMediaTypeCanvas ||
      (params_.media_type == WebContextMenuData::kMediaTypeImage &&
       is_large_data_url)) {
    RenderFrameHost* frame_host = GetRenderFrameHost();
    if (frame_host)
      frame_host->SaveImageAt(params_.x, params_.y);
  } else {
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
    const GURL& url = params_.src_url;
    content::Referrer referrer = CreateReferrer(url, params_);

    std::string headers;
    DataReductionProxyChromeSettings* settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser_context_);
    if (params_.media_type == WebContextMenuData::kMediaTypeImage && settings &&
        settings->CanUseDataReductionProxy(params_.src_url)) {
      headers = data_reduction_proxy::chrome_proxy_pass_through_header();
    }

    source_web_contents_->SaveFrameWithHeaders(url, referrer, headers,
                                               params_.suggested_filename);
  }
}

void RenderViewContextMenu::ExecExitFullscreen() {
  Browser* browser = GetBrowser();
  if (!browser) {
    NOTREACHED();
    return;
  }

  browser->exclusive_access_manager()->ExitExclusiveAccess();
}

void RenderViewContextMenu::ExecCopyLinkText() {
  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  scw.WriteText(params_.link_text);
}

void RenderViewContextMenu::ExecCopyImageAt() {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (frame_host)
    frame_host->CopyImageAt(params_.x, params_.y);
}

void RenderViewContextMenu::ExecSearchWebForImage() {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper)
    return;
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  core_tab_helper->SearchByImageInNewTab(render_frame_host, params().src_url);
}

void RenderViewContextMenu::ExecLoadOriginalImage() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  chrome::mojom::ChromeRenderFrameAssociatedPtr chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  chrome_render_frame->RequestReloadImageForContextNode();
}

void RenderViewContextMenu::ExecPlayPause() {
  bool play = !!(params_.media_flags & WebContextMenuData::kMediaPaused);
  if (play)
    base::RecordAction(UserMetricsAction("MediaContextMenu_Play"));
  else
    base::RecordAction(UserMetricsAction("MediaContextMenu_Pause"));

  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      WebMediaPlayerAction(WebMediaPlayerAction::kPlay, play));
}

void RenderViewContextMenu::ExecMute() {
  bool mute = !(params_.media_flags & WebContextMenuData::kMediaMuted);
  if (mute)
    base::RecordAction(UserMetricsAction("MediaContextMenu_Mute"));
  else
    base::RecordAction(UserMetricsAction("MediaContextMenu_Unmute"));

  MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                      WebMediaPlayerAction(WebMediaPlayerAction::kMute, mute));
}

void RenderViewContextMenu::ExecLoop() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Loop"));
  MediaPlayerActionAt(
      gfx::Point(params_.x, params_.y),
      WebMediaPlayerAction(WebMediaPlayerAction::kLoop,
                           !IsCommandIdChecked(IDC_CONTENT_CONTEXT_LOOP)));
}

void RenderViewContextMenu::ExecControls() {
  base::RecordAction(UserMetricsAction("MediaContextMenu_Controls"));
  MediaPlayerActionAt(
      gfx::Point(params_.x, params_.y),
      WebMediaPlayerAction(WebMediaPlayerAction::kControls,
                           !IsCommandIdChecked(IDC_CONTENT_CONTEXT_CONTROLS)));
}

void RenderViewContextMenu::ExecRotateCW() {
  base::RecordAction(UserMetricsAction("PluginContextMenu_RotateClockwise"));
  PluginActionAt(gfx::Point(params_.x, params_.y),
                 WebPluginAction(WebPluginAction::kRotate90Clockwise, true));
}

void RenderViewContextMenu::ExecRotateCCW() {
  base::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateCounterclockwise"));
  PluginActionAt(
      gfx::Point(params_.x, params_.y),
      WebPluginAction(WebPluginAction::kRotate90Counterclockwise, true));
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
  if (params_.media_type != WebContextMenuData::kMediaTypeNone) {
    RenderFrameHost* render_frame_host = GetRenderFrameHost();
    if (render_frame_host) {
      render_frame_host->Send(new PrintMsg_PrintNodeUnderContextMenu(
          render_frame_host->GetRoutingID()));
    }
    return;
  }

  printing::StartPrint(
      source_web_contents_,
      GetPrefs(browser_context_)->GetBoolean(prefs::kPrintPreviewDisabled),
      !params_.selection_text.empty());
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

void RenderViewContextMenu::ExecRouteMedia() {
  if (!media_router::MediaRouterEnabled(browser_context_))
    return;

  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          embedder_web_contents_);
  if (!dialog_controller)
    return;

  dialog_controller->ShowMediaRouterDialog();
  media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
      media_router::MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU);
}

void RenderViewContextMenu::ExecTranslate() {
  // A translation might have been triggered by the time the menu got
  // selected, do nothing in that case.
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(embedder_web_contents_);
  if (!chrome_translate_client ||
      chrome_translate_client->GetLanguageState().IsPageTranslated() ||
      chrome_translate_client->GetLanguageState().translation_pending()) {
    return;
  }

  std::string original_lang =
      chrome_translate_client->GetLanguageState().original_language();
  std::string target_lang = GetTargetLanguage();

  // Since the user decided to translate for that language and site, clears
  // any preferences for not translating them.
  std::unique_ptr<translate::TranslatePrefs> prefs(
      ChromeTranslateClient::CreateTranslatePrefs(
          GetPrefs(browser_context_)));
  prefs->UnblockLanguage(original_lang);
  prefs->RemoveSiteFromBlacklist(params_.page_url.HostNoBrackets());

  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  DCHECK(manager);
  manager->TranslatePage(original_lang, target_lang, true);
}

void RenderViewContextMenu::ExecLanguageSettings(int event_flags) {
  WindowOpenDisposition disposition =
      ForceNewTabDispositionFromEventFlags(event_flags);
  GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
  OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecProtocolHandlerSettings(int event_flags) {
  base::RecordAction(
      UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
  WindowOpenDisposition disposition =
      ForceNewTabDispositionFromEventFlags(event_flags);
  GURL url = chrome::GetSettingsUrl(chrome::kHandlerSettingsSubPage);
  OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
}

void RenderViewContextMenu::ExecPictureInPicture() {
  if (!base::FeatureList::IsEnabled(media::kPictureInPicture))
    return;

  bool picture_in_picture_active =
      IsCommandIdChecked(IDC_CONTENT_CONTEXT_PICTUREINPICTURE);

  if (picture_in_picture_active) {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_ExitPictureInPicture"));
  } else {
    base::RecordAction(
        UserMetricsAction("MediaContextMenu_EnterPictureInPicture"));
  }

  MediaPlayerActionAt(
      gfx::Point(params_.x, params_.y),
      WebMediaPlayerAction(WebMediaPlayerAction::kPictureInPicture,
                           !picture_in_picture_active));
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  ::WriteURLToClipboard(url);
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const WebMediaPlayerAction& action) {
  RenderFrameHost* frame_host = GetRenderFrameHost();
  if (frame_host)
    frame_host->ExecuteMediaPlayerActionAtLocation(location, action);
}

void RenderViewContextMenu::PluginActionAt(
    const gfx::Point& location,
    const WebPluginAction& action) {
  source_web_contents_->GetRenderViewHost()->
      ExecutePluginActionAtLocation(location, action);
}

Browser* RenderViewContextMenu::GetBrowser() const {
  return chrome::FindBrowserWithWebContents(embedder_web_contents_);
}
