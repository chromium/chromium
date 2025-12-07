// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "content/public/common/color_parser.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/icon_util.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/origin.h"

using content::WebContents;

namespace extensions {

namespace {

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";

bool g_report_error_for_invisible_icon = false;

// Returns true if the color values provided could be parsed into a color
// object out param.
bool ParseColor(const base::Value& color_value, SkColor& color) {
  if (color_value.is_string()) {
    return content::ParseCssColorString(color_value.GetString(), &color);
  }

  if (!color_value.is_list()) {
    return false;
  }

  const base::Value::List& color_list = color_value.GetList();
  if (color_list.size() != 4 ||
      std::ranges::any_of(color_list,
                          [](const auto& color) { return !color.is_int(); })) {
    return false;
  }

  color = SkColorSetARGB(color_list[3].GetInt(), color_list[0].GetInt(),
                         color_list[1].GetInt(), color_list[2].GetInt());
  return true;
}

}  // namespace

//
// ExtensionActionFunction
//

ExtensionActionFunction::ExtensionActionFunction()
    : details_(nullptr),
      tab_id_(ExtensionAction::kDefaultTabId),
      contents_(nullptr),
      extension_action_(nullptr) {}

ExtensionActionFunction::~ExtensionActionFunction() = default;

ExtensionFunction::ResponseAction ExtensionActionFunction::Run() {
  ExtensionActionManager* manager =
      ExtensionActionManager::Get(browser_context());
  extension_action_ = manager->GetExtensionAction(*extension());
  if (!extension_action_) {
    // TODO(kalman): Ideally the browserAction/pageAction APIs wouldn't even
    // exist for extensions that don't have one declared. This should come as
    // part of the Feature system.
    return RespondNow(Error(kNoExtensionActionError));
  }

  // Populates the tab_id_ and details_ members.
  EXTENSION_FUNCTION_VALIDATE(ExtractDataFromArguments());

  // Find the WebContents that contains this tab id if one is required.
  if (tab_id_ != ExtensionAction::kDefaultTabId) {
    content::WebContents* contents_out_param = nullptr;
    ExtensionTabUtil::GetTabById(tab_id_, browser_context(),
                                 include_incognito_information(),
                                 &contents_out_param);
    if (!contents_out_param) {
      return RespondNow(Error(kNoTabError, base::NumberToString(tab_id_)));
    }
    contents_ = contents_out_param;
  } else {
    // Page actions do not have a default tabId.
    EXTENSION_FUNCTION_VALIDATE(extension_action_->action_type() !=
                                ActionInfo::Type::kPage);
  }
  return RunExtensionAction();
}

bool ExtensionActionFunction::ExtractDataFromArguments() {
  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists), as the first
  // argument besides the action type (depends on the function), or be omitted
  // entirely.
  if (args().empty()) {
    return true;
  }

  const base::Value& first_arg = args()[0];

  switch (first_arg.type()) {
    case base::Value::Type::INTEGER:
      tab_id_ = first_arg.GetInt();
      break;

    case base::Value::Type::DICT: {
      // Found the details argument.
      details_ = &first_arg.GetDict();
      // Still need to check for the tabId within details.
      if (const base::Value* tab_id_value = details_->Find("tabId")) {
        switch (tab_id_value->type()) {
          case base::Value::Type::NONE:
            // OK; tabId is optional, leave it default.
            return true;
          case base::Value::Type::INTEGER:
            tab_id_ = tab_id_value->GetInt();
            return true;
          default:
            // Boom.
            return false;
        }
      }
      // Not found; tabId is optional, leave it default.
      break;
    }

    case base::Value::Type::NONE:
      // The tabId might be an optional argument.
      break;

    default:
      return false;
  }

  return true;
}

void ExtensionActionFunction::NotifyChange() {
  ExtensionActionDispatcher::Get(browser_context())
      ->NotifyChange(extension_action_, contents_, browser_context());
}

void ExtensionActionFunction::SetVisible(bool visible) {
  if (extension_action_->GetIsVisible(tab_id_) == visible) {
    return;
  }
  extension_action_->SetIsVisible(tab_id_, visible);
  NotifyChange();
}

ExtensionFunction::ResponseAction
ExtensionActionShowFunction::RunExtensionAction() {
  SetVisible(true);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionHideFunction::RunExtensionAction() {
  SetVisible(false);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ActionIsEnabledFunction::RunExtensionAction() {
  return RespondNow(WithArguments(
      extension_action_->GetIsVisibleIgnoringDeclarative(tab_id_)));
}

// static
void ExtensionActionSetIconFunction::SetReportErrorForInvisibleIconForTesting(
    bool value) {
  g_report_error_for_invisible_icon = value;
}

ExtensionFunction::ResponseAction
ExtensionActionSetIconFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  const base::Value::Dict* canvas_set = details_->FindDict("imageData");
  if (canvas_set) {
    gfx::ImageSkia icon;

    extensions::IconParseResult parse_result =
        extensions::ParseIconFromCanvasDictionary(*canvas_set, &icon);
    EXTENSION_FUNCTION_VALIDATE(parse_result ==
                                extensions::IconParseResult::kSuccess);

    if (icon.isNull()) {
      return RespondNow(Error("Icon invalid."));
    }

    gfx::Image icon_image(icon);
    const SkBitmap bitmap = icon_image.AsBitmap();
    const bool is_visible = image_util::IsIconSufficientlyVisible(bitmap);
    if (!is_visible && g_report_error_for_invisible_icon) {
      return RespondNow(Error("Icon not sufficiently visible."));
    }

    extension_action_->SetIcon(tab_id_, icon_image);
  } else if (details_->FindInt("iconIndex")) {
    // Obsolete argument: ignore it.
    return RespondNow(NoArguments());
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetTitleFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const std::string* title = details_->FindString("title");
  EXTENSION_FUNCTION_VALIDATE(title);
  extension_action_->SetTitle(tab_id_, *title);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetPopupFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const std::string* popup_string = details_->FindString("popup");
  EXTENSION_FUNCTION_VALIDATE(popup_string);
  GURL popup_url;

  // If an empty string is passed, remove the explicitly set popup. Setting it
  // back to an empty string (URL) will cause it to fall back to the default set
  // in the manifest.
  if (!popup_string->empty()) {
    popup_url = extension()->ResolveExtensionURL(*popup_string);
    if (!popup_url.is_valid()) {
      return RespondNow(Error(manifest_errors::kInvalidExtensionPopupPath));
    }
  }

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);

  const std::string* badge_text = details_->FindString("text");
  if (badge_text) {
    extension_action_->SetBadgeText(tab_id_, *badge_text);
  } else {
    extension_action_->ClearBadgeText(tab_id_);
  }

  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color)) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }
  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ActionSetBadgeTextColorFunction::RunExtensionAction() {
  EXTENSION_FUNCTION_VALIDATE(details_);
  const base::Value* color_value = details_->Find("color");
  EXTENSION_FUNCTION_VALIDATE(color_value);
  SkColor color = 0;
  if (!ParseColor(*color_value, color)) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }

  if (SkColorGetA(color) == SK_AlphaTRANSPARENT) {
    return RespondNow(Error(extension_misc::kInvalidColorError));
  }
  extension_action_->SetBadgeTextColor(tab_id_, color);
  NotifyChange();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionActionGetTitleFunction::RunExtensionAction() {
  return RespondNow(WithArguments(extension_action_->GetTitle(tab_id_)));
}

ExtensionFunction::ResponseAction
ExtensionActionGetPopupFunction::RunExtensionAction() {
  return RespondNow(
      WithArguments(extension_action_->GetPopupUrl(tab_id_).spec()));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  declarative_net_request::PrefsHelper helper(
      *ExtensionPrefs::Get(browser_context()));
  bool is_dnr_action_count_active =
      helper.GetUseActionCountAsBadgeText(extension_id()) &&
      !extension_action_->HasBadgeText(tab_id_);

  // Ensure that the placeholder string is returned if this extension is
  // displaying action counts for the badge labels and the extension doesn't
  // have permission to view the action count for this tab. Note that
  // tab-specific badge text takes priority over the action count.
  if (is_dnr_action_count_active &&
      !declarative_net_request::HasDNRFeedbackPermission(extension(),
                                                         tab_id_)) {
    return RespondNow(WithArguments(
        std::move(declarative_net_request::kActionCountPlaceholderBadgeText)));
  }

  return RespondNow(
      WithArguments(extension_action_->GetDisplayBadgeText(tab_id_)));
}

ExtensionFunction::ResponseAction
ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  base::Value::List list;
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list.Append(static_cast<int>(SkColorGetR(color)));
  list.Append(static_cast<int>(SkColorGetG(color)));
  list.Append(static_cast<int>(SkColorGetB(color)));
  list.Append(static_cast<int>(SkColorGetA(color)));
  return RespondNow(WithArguments(std::move(list)));
}

ExtensionFunction::ResponseAction
ActionGetBadgeTextColorFunction::RunExtensionAction() {
  base::Value::List list;
  SkColor color = extension_action_->GetBadgeTextColor(tab_id_);
  list.Append(static_cast<int>(SkColorGetR(color)));
  list.Append(static_cast<int>(SkColorGetG(color)));
  list.Append(static_cast<int>(SkColorGetB(color)));
  list.Append(static_cast<int>(SkColorGetA(color)));
  return RespondNow(WithArguments(std::move(list)));
}

ActionGetUserSettingsFunction::ActionGetUserSettingsFunction() = default;
ActionGetUserSettingsFunction::~ActionGetUserSettingsFunction() = default;

ExtensionFunction::ResponseAction ActionGetUserSettingsFunction::Run() {
  DCHECK(extension());
  ExtensionActionManager* const action_manager =
      ExtensionActionManager::Get(browser_context());
  ExtensionAction* const action =
      action_manager->GetExtensionAction(*extension());

  // This API is only available to extensions with the "action" key in the
  // manifest, so they should always have an action.
  DCHECK(action);
  DCHECK_EQ(ActionInfo::Type::kAction, action->action_type());

  const bool is_pinned =
      ToolbarActionsModel::Get(Profile::FromBrowserContext(browser_context()))
          ->IsActionPinned(extension_id());

  // TODO(crbug.com/360916928): Today, no action APIs are compiled.
  // Unfortunately, this means we miss out on the compiled types, which would be
  // rather helpful here.
  base::Value::Dict ui_settings;
  ui_settings.Set("isOnToolbar", is_pinned);

  return RespondNow(WithArguments(std::move(ui_settings)));
}

// ActionOpenPopupFunction and BrowserActionOpenPopupFunction have separate
// Android and non-Android implementations. See extension_action_api_android.cc
// and extension_action_api_non_android.cc.

}  // namespace extensions
