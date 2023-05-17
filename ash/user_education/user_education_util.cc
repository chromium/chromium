// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_util.h"

#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_types.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/window.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

namespace ash::user_education_util {
namespace {

// Keys used in `user_education::HelpBubbleParams::ExtendedProperties`.
constexpr char kHelpBubbleIdKey[] = "helpBubbleId";
constexpr char kHelpBubbleStyleKey[] = "helpBubbleStyle";

// Helpers ---------------------------------------------------------------------

AccountId GetActiveAccountId(const SessionControllerImpl* session_controller) {
  return session_controller ? session_controller->GetActiveAccountId()
                            : AccountId();
}

const AccountId& GetPrimaryAccountId() {
  const auto* session_controller = Shell::Get()->session_controller();
  return session_controller
             ? GetAccountId(session_controller->GetPrimaryUserSession())
             : EmptyAccountId();
}

aura::Window* GetRootWindowForDisplayId(int64_t display_id) {
  auto* window_tree_host_manager = Shell::Get()->window_tree_host_manager();
  return window_tree_host_manager
             ? window_tree_host_manager->GetRootWindowForDisplayId(display_id)
             : nullptr;
}

session_manager::SessionState GetSessionState(
    const SessionControllerImpl* session_controller) {
  return session_controller ? session_controller->GetSessionState()
                            : session_manager::SessionState::UNKNOWN;
}

}  // namespace

// Utilities -------------------------------------------------------------------

user_education::HelpBubbleParams::ExtendedProperties CreateExtendedProperties(
    HelpBubbleId help_bubble_id) {
  user_education::HelpBubbleParams::ExtendedProperties extended_properties;
  extended_properties.values().Set(kHelpBubbleIdKey,
                                   static_cast<int>(help_bubble_id));
  return extended_properties;
}

user_education::HelpBubbleParams::ExtendedProperties CreateExtendedProperties(
    HelpBubbleStyle help_bubble_style) {
  user_education::HelpBubbleParams::ExtendedProperties extended_properties;
  extended_properties.values().Set(kHelpBubbleStyleKey,
                                   static_cast<int>(help_bubble_style));
  return extended_properties;
}

const AccountId& GetAccountId(const UserSession* user_session) {
  return user_session ? user_session->user_info.account_id : EmptyAccountId();
}

HelpBubbleId GetHelpBubbleId(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties) {
  return static_cast<HelpBubbleId>(
      extended_properties.values().FindInt(kHelpBubbleIdKey).value());
}

absl::optional<HelpBubbleStyle> GetHelpBubbleStyle(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties) {
  if (absl::optional<int> help_bubble_style =
          extended_properties.values().FindInt(kHelpBubbleStyleKey)) {
    return static_cast<HelpBubbleStyle>(help_bubble_style.value());
  }
  return absl::nullopt;
}

views::View* GetMatchingViewInRootWindow(int64_t display_id,
                                         ui::ElementIdentifier element_id) {
  aura::Window* root_window = GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return nullptr;
  }

  const std::vector<views::View*> matching_views =
      views::ElementTrackerViews::GetInstance()
          ->GetAllMatchingViewsInAnyContext(element_id);

  for (views::View* matching_view : matching_views) {
    if (root_window->Contains(matching_view->GetWidget()->GetNativeWindow())) {
      return matching_view;
    }
  }

  return nullptr;
}

bool IsPrimaryAccountActive() {
  const auto* session_controller = Shell::Get()->session_controller();
  return IsPrimaryAccountId(GetActiveAccountId(session_controller)) &&
         GetSessionState(session_controller) ==
             session_manager::SessionState::ACTIVE;
}

bool IsPrimaryAccountId(const AccountId& account_id) {
  return account_id.is_valid() ? GetPrimaryAccountId() == account_id : false;
}

std::string ToString(TutorialId tutorial_id) {
  switch (tutorial_id) {
    case TutorialId::kCaptureModeTourPrototype1:
      return "AshCaptureModeTourPrototype1";
    case TutorialId::kCaptureModeTourPrototype2:
      return "AshCaptureModeTourPrototype2";
    case TutorialId::kHoldingSpaceTourPrototype1:
      return "AshHoldingSpaceTourPrototype1";
    case TutorialId::kHoldingSpaceTourPrototype2:
      return "AshHoldingSpaceTourPrototype2";
    case TutorialId::kTest:
      return "AshTest";
    case TutorialId::kWelcomeTourPrototype1:
      return "AshWelcomeTourPrototype1";
  }
}

}  // namespace ash::user_education_util
