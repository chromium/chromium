// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_util.h"

#include <map>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_types.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/unguessable_token.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_education/common/events.h"
#include "components/user_education/common/help_bubble.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"

namespace ash::user_education_util {
namespace {

// Keys used in `user_education::HelpBubbleParams::ExtendedProperties`.
constexpr char kHelpBubbleBodyIconKey[] = "helpBubbleBodyIcon";
constexpr char kHelpBubbleIdKey[] = "helpBubbleId";
constexpr char kHelpBubbleModalTypeKey[] = "helpBubbleModalType";
constexpr char kHelpBubbleStyleKey[] = "helpBubbleStyle";

// Helpers ---------------------------------------------------------------------

AccountId GetActiveAccountId(const SessionControllerImpl* session_controller) {
  return session_controller ? session_controller->GetActiveAccountId()
                            : AccountId();
}

std::map<std::string, raw_ptr<const gfx::VectorIcon>>&
GetHelpBubbleBodyIconRegistry() {
  static base::NoDestructor<
      std::map<std::string, raw_ptr<const gfx::VectorIcon>>>
      registry;
  return *registry;
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
    const gfx::VectorIcon& body_icon) {
  auto& registry = GetHelpBubbleBodyIconRegistry();

  auto it = base::ranges::find(
      registry, &body_icon,
      &std::pair<const std::string, raw_ptr<const gfx::VectorIcon>>::second);

  if (it == registry.end()) {
    const auto token = base::UnguessableToken::Create();
    it = registry.emplace(token.ToString(), &body_icon).first;
  }

  user_education::HelpBubbleParams::ExtendedProperties extended_properties;
  extended_properties.values().Set(kHelpBubbleBodyIconKey, it->first);
  return extended_properties;
}

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

user_education::HelpBubbleParams::ExtendedProperties CreateExtendedProperties(
    ui::ModalType modal_type) {
  user_education::HelpBubbleParams::ExtendedProperties extended_properties;
  extended_properties.values().Set(kHelpBubbleModalTypeKey,
                                   static_cast<int>(modal_type));
  return extended_properties;
}

const AccountId& GetAccountId(const UserSession* user_session) {
  return user_session ? user_session->user_info.account_id : EmptyAccountId();
}

absl::optional<std::reference_wrapper<const gfx::VectorIcon>>
GetHelpBubbleBodyIcon(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties) {
  if (const std::string* body_icon =
          extended_properties.values().FindString(kHelpBubbleBodyIconKey)) {
    auto& registry = GetHelpBubbleBodyIconRegistry();
    auto it = registry.find(*body_icon);
    CHECK(it != registry.end());
    return *it->second;
  }
  return absl::nullopt;
}

HelpBubbleId GetHelpBubbleId(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties) {
  return static_cast<HelpBubbleId>(
      extended_properties.values().FindInt(kHelpBubbleIdKey).value());
}

ui::ModalType GetHelpBubbleModalType(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties) {
  if (const absl::optional<int> model_type =
          extended_properties.values().FindInt(kHelpBubbleModalTypeKey)) {
    return static_cast<ui::ModalType>(model_type.value());
  }
  return ui::MODAL_TYPE_NONE;
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

absl::optional<user_manager::UserType> GetUserType(
    const AccountId& account_id) {
  if (const auto* ctrlr = Shell::Get()->session_controller()) {
    if (const auto* session = ctrlr->GetUserSessionByAccountId(account_id)) {
      return session->user_info.type;
    }
  }
  return absl::nullopt;
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
    case TutorialId::kTest1:
      return "AshTest1";
    case TutorialId::kTest2:
      return "AshTest2";
    case TutorialId::kWelcomeTour:
      return "AshWelcomeTour";
  }
}

}  // namespace ash::user_education_util
