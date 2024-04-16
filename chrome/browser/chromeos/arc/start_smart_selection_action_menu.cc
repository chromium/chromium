// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/start_smart_selection_action_menu.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia_operations.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#endif

namespace arc {

namespace {

apps::IntentPtr CreateIntent(
    arc::ArcIntentHelperMojoDelegate::IntentInfo arc_intent,
    arc::ArcIntentHelperMojoDelegate::ActivityName activity) {
  auto intent = std::make_unique<apps::Intent>(arc_intent.action);
  intent->data = std::move(arc_intent.data);
  intent->mime_type = std::move(arc_intent.type);
  if (arc_intent.categories.has_value())
    intent->categories = std::move(arc_intent.categories.value());
  intent->ui_bypassed = arc_intent.ui_bypassed;
  if (arc_intent.extras.has_value())
    intent->extras = std::move(arc_intent.extras.value());

  intent->activity_name = std::move(activity.activity_name);

  return intent;
}

}  // namespace

// The maximum number of smart actions to show.
constexpr size_t kMaxMainMenuCommands = 5;

StartSmartSelectionActionMenu::StartSmartSelectionActionMenu(
    content::BrowserContext* context,
    RenderViewContextMenuProxy* proxy,
    std::unique_ptr<ArcIntentHelperMojoDelegate> delegate)
    : context_(context), proxy_(proxy), delegate_(std::move(delegate)) {}

StartSmartSelectionActionMenu::~StartSmartSelectionActionMenu() = default;

void StartSmartSelectionActionMenu::InitMenu(
    const content::ContextMenuParams& params) {
  const std::string converted_text = base::UTF16ToUTF8(params.selection_text);
  if (converted_text.empty())
    return;

  DCHECK(delegate_);
  if (!delegate_->IsRequestTextSelectionActionsAvailable()) {
    // RequestTextSelectionActions is either not supported or not yet ready.
    // In this case, immediately stop menu initialization instead of calling
    // callback HandleTextSelectionActions with empty result.
    //
    // This conditions is required to avoid accessing to null menu due to the
    // timing issue.
    //
    // In Lacros, RequestTextSelectionActions API will return false
    // synchronously when mojo API is not supported in Lacros-side. In this
    // case, the context menu is not initialized yet, so Lacros tries to access
    // to null menu in HandleTextSelectionActions. To avoid this, we skip the
    // following operation when RequestTextSelectionActions API is not supported
    // in Lacros-side. Note that we can ignore the case where
    // RequestTextSelectionActions API fails remotely since it runs
    // asynchronously anyway.
    //
    // In Ash, it will always return false synchronously when mojo API is not
    // supported in Ash-side or ARC-side. In both cases, we need to skip the
    // following operation with the same reason above.
    return;
  }

  if (!delegate_->RequestTextSelectionActions(
          converted_text, ui::GetMaxSupportedResourceScaleFactor(),
          base::BindOnce(
              &StartSmartSelectionActionMenu::HandleTextSelectionActions,
              weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40808069): Take metrics in Lacros as well.
  base::RecordAction(base::UserMetricsAction("Arc.SmartTextSelection.Request"));
#endif

  // Add placeholder items.
  for (size_t i = 0; i < kMaxMainMenuCommands; ++i) {
    proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
                        /*title=*/std::u16string());
  }
}

bool StartSmartSelectionActionMenu::IsCommandIdSupported(int command_id) {
  return command_id >= IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 &&
         command_id <= IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION_LAST;
}

bool StartSmartSelectionActionMenu::IsCommandIdChecked(int command_id) {
  return false;
}

bool StartSmartSelectionActionMenu::IsCommandIdEnabled(int command_id) {
  return true;
}

void StartSmartSelectionActionMenu::ExecuteCommand(int command_id) {
  if (!IsCommandIdSupported(command_id))
    return;

  size_t index = command_id - IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1;
  if (actions_.size() <= index)
    return;

  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);

  Profile* profile = Profile::FromBrowserContext(context_);
  if (actions_[index].activity.package_name ==
      arc::kArcIntentHelperPackageName) {
    // The intent_helper app can't be launched as a regular app that then
    // handles this smart action intent because it is not a launcher app.
    // Instead, directly request that the intent be handled by it without
    // really launching the app.
    // This is necessary for "generic" smart selection actions that aren't
    // created for specific apps such as opening a URL or a street address.
    delegate_->HandleIntent(std::move(actions_[index].action_intent),
                            internal::ActivityIconLoader::ActivityName(
                                actions_[index].activity.package_name,
                                actions_[index].activity.activity_name));
    return;
  }
  // The app that this intent points to is able to handle it, launch it.
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      actions_[index].app_id, ui::EF_NONE,
      CreateIntent(std::move(actions_[index].action_intent),
                   std::move(actions_[index].activity)),
      apps::LaunchSource::kFromSmartTextContextMenu,
      std::make_unique<apps::WindowInfo>(display.id()), base::DoNothing());
}

void StartSmartSelectionActionMenu::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& rect) {
  // Since entries are kept as place holders, make them non editable and hidden.
  for (size_t i = 0; i < kMaxMainMenuCommands; i++) {
    proxy_->UpdateMenuItem(
        IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
        /*enabled=*/false,
        /*hidden=*/true,
        /*title=*/std::u16string());
  }
}

void StartSmartSelectionActionMenu::HandleTextSelectionActions(
    std::vector<ArcIntentHelperMojoDelegate::TextSelectionAction> actions) {
  actions_ = std::move(actions);

  for (size_t i = 0; i < actions_.size(); ++i) {
    proxy_->UpdateMenuItem(
        IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
        /*enabled=*/true,
        /*hidden=*/false,
        /*title=*/base::UTF8ToUTF16(actions_[i].title));

    proxy_->UpdateMenuIcon(
        IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
        ui::ImageModel::FromImageSkia(std::move(actions_[i].icon)));
  }

  for (size_t i = actions_.size(); i < kMaxMainMenuCommands; ++i) {
    // There were fewer actions returned than placeholder slots, remove the
    // empty menu item.
    proxy_->RemoveMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 +
                           i);
  }

  // The asynchronous nature of adding smart actions means that sometimes,
  // depending on whether actions were found and if extensions menu items were
  // added synchronously, there could be extra (adjacent) separators in the
  // context menu that must be removed once we've finished loading everything.
  proxy_->RemoveAdjacentSeparators();
}

}  // namespace arc
