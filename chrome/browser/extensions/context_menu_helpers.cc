// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_helpers.h"

#include <stddef.h>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using blink::mojom::ContextMenuDataMediaType;

namespace extensions {
namespace context_menu_helpers {
namespace {

// Helper function to determine if a given URL matches a URLPatternSet.
// This is primarily a wrapper around URLPatternSet::MatchesURL but includes an
// important special case: An empty pattern set is considered a match.
// This convention is used throughout the extension system to mean that an item
// has no URL-specific restrictions.
//
// @param patterns The set of URL patterns to check against.
// @param url The URL to be tested.
// @return true if the URL is matched by the patterns or if the pattern set
//     is empty, false otherwise.
bool ExtensionPatternMatch(const URLPatternSet& patterns, const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty()) {
    return true;
  }
  return patterns.MatchesURL(url);
}

// Escapes ampersands in a string by replacing each `&` with `&&`.
// This is necessary for strings that will be displayed in UI elements like
// menus or labels, as a single ampersand is often interpreted as a prefix for
// a mnemonic character (e.g., "S&ave" would display as "Save" with 'a'
// underlined). Doubling the ampersand displays a literal '&'. The string is
// modified in-place.
//
// @param text The string to modify.
void EscapeAmpersands(std::u16string* text) {
  base::ReplaceChars(*text, u"&", u"&&", text);
}

}  // namespace

const char kActionNotAllowedError[] =
    "Only extensions are allowed to use action contexts";
const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kDuplicateIDError[] =
    "Cannot create item with duplicate id *";
const char kGeneratedIdKey[] = "generatedId";
const char kLauncherNotAllowedError[] =
    "Only packaged apps are allowed to use 'launcher' context";
const char kOnclickDisallowedError[] =
    "Extensions using event pages or "
    "Service Workers cannot pass an onclick parameter to "
    "chrome.contextMenus.create. Instead, use the "
    "chrome.contextMenus.onClicked event.";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";
const char kTooManyMenuItems[] =
    "An extension can create a maximum of * menu items.";

std::string GetIDString(const MenuItem::Id& id) {
  if (id.uid == 0) {
    return id.string_uid;
  } else {
    return base::NumberToString(id.uid);
  }
}

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error) {
  MenuItem* parent = menu_manager->GetItemById(parent_id);
  if (!parent) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return nullptr;
  }
  if (parent->type() != MenuItem::NORMAL) {
    *error = kParentsMustBeNormalError;
    return nullptr;
  }
  return parent;
}

MenuItem::ContextList GetContexts(
    const std::vector<api::context_menus::ContextType>& in_contexts) {
  MenuItem::ContextList contexts;
  for (auto context : in_contexts) {
    switch (context) {
      case api::context_menus::ContextType::kAll:
        contexts.Add(MenuItem::ALL);
        break;
      case api::context_menus::ContextType::kPage:
        contexts.Add(MenuItem::PAGE);
        break;
      case api::context_menus::ContextType::kSelection:
        contexts.Add(MenuItem::SELECTION);
        break;
      case api::context_menus::ContextType::kLink:
        contexts.Add(MenuItem::LINK);
        break;
      case api::context_menus::ContextType::kEditable:
        contexts.Add(MenuItem::EDITABLE);
        break;
      case api::context_menus::ContextType::kImage:
        contexts.Add(MenuItem::IMAGE);
        break;
      case api::context_menus::ContextType::kVideo:
        contexts.Add(MenuItem::VIDEO);
        break;
      case api::context_menus::ContextType::kAudio:
        contexts.Add(MenuItem::AUDIO);
        break;
      case api::context_menus::ContextType::kFrame:
        contexts.Add(MenuItem::FRAME);
        break;
      case api::context_menus::ContextType::kLauncher:
        // Not available for <webview>.
        contexts.Add(MenuItem::LAUNCHER);
        break;
      case api::context_menus::ContextType::kBrowserAction:
        // Not available for <webview>.
        contexts.Add(MenuItem::BROWSER_ACTION);
        break;
      case api::context_menus::ContextType::kPageAction:
        // Not available for <webview>.
        contexts.Add(MenuItem::PAGE_ACTION);
        break;
      case api::context_menus::ContextType::kAction:
        // Not available for <webview>.
        contexts.Add(MenuItem::ACTION);
        break;
      case api::context_menus::ContextType::kNone:
        NOTREACHED();
    }
  }
  return contexts;
}

MenuItem::Type GetType(api::context_menus::ItemType type,
                       MenuItem::Type default_type) {
  switch (type) {
    case api::context_menus::ItemType::kNone:
      return default_type;
    case api::context_menus::ItemType::kNormal:
      return MenuItem::NORMAL;
    case api::context_menus::ItemType::kCheckbox:
      return MenuItem::CHECKBOX;
    case api::context_menus::ItemType::kRadio:
      return MenuItem::RADIO;
    case api::context_menus::ItemType::kSeparator:
      return MenuItem::SEPARATOR;
  }
  return MenuItem::NORMAL;
}

bool ExtensionContextAndPatternMatch(const content::ContextMenuParams& params,
                                     const MenuItem::ContextList& contexts,
                                     const URLPatternSet& target_url_patterns) {
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

bool MenuItemMatchesParams(const content::ContextMenuParams& params,
                           const MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match) {
    return false;
  }

  return ExtensionPatternMatch(item->document_url_patterns(), params.frame_url);
}

std::u16string PrintableSelectionText(const std::u16string& selection_text) {
  std::u16string result = gfx::TruncateString(
      selection_text, RenderViewContextMenuBase::kMaxSelectionTextLength,
      gfx::WORD_BREAK);
  EscapeAmpersands(&result);
  return result;
}

void PopulateExtensionItems(content::BrowserContext* browser_context,
                            const content::ContextMenuParams& params,
                            ContextMenuMatcher& matcher) {
  base::ElapsedTimer timer;
  matcher.Clear();

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  MenuManager* menu_manager = MenuManager::Get(browser_context);

  if (!menu_manager || !registry) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Extensions.ContextMenuHelpers.PopulateExtensionItems.Duration",
        base::Microseconds(timer.Elapsed().InMicrosecondsF()),
        base::Microseconds(1), base::Microseconds(2000), 100);
    return;
  }

  std::u16string printable_selection_text =
      context_menu_helpers::PrintableSelectionText(params.selection_text);

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
      std::u16string menu_title =
          matcher.GetTopLevelContextMenuTitle(id, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }

  if (sorted_menu_titles.empty()) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Extensions.ContextMenuHelpers.PopulateExtensionItems.Duration",
        base::Microseconds(timer.Elapsed().InMicrosecondsF()),
        base::Microseconds(1), base::Microseconds(2000), 100);
    return;
  }

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  for (const auto& title : sorted_menu_titles) {
    const std::vector<const Extension*>& extensions =
        title_to_extensions_map[title];
    for (const Extension* extension : extensions) {
      MenuItem::ExtensionKey extension_key(extension->id());
      matcher.AppendExtensionItems(extension_key, printable_selection_text,
                                   &index,
                                   /*is_action_menu=*/false);
    }
  }
  base::UmaHistogramCustomMicrosecondsTimes(
      "Extensions.ContextMenuHelpers.PopulateExtensionItems.Duration",
      base::Microseconds(timer.Elapsed().InMicrosecondsF()),
      base::Microseconds(1), base::Microseconds(2000), 100);
}

}  // namespace context_menu_helpers
}  // namespace extensions
