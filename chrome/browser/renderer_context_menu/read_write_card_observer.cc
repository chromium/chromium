// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/read_write_card_observer.h"

#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr int kMaxSurroundingTextLength = 300;

void SetUiControllerContextMenuBounds(const gfx::Rect& bounds_in_screen) {
  chromeos::ReadWriteCardsManager* cards_manager =
      chromeos::ReadWriteCardsManager::Get();
  if (cards_manager) {
    cards_manager->SetContextMenuBounds(
        /*context_menu_bounds=*/bounds_in_screen);
  } else {
    // `cards_manager` should only be null in a test environment (since in some
    // tests the global `ReadWriteCardsManager` is not constructed).
    CHECK_IS_TEST();
  }
}

}  // namespace

ReadWriteCardObserver::ReadWriteCardObserver(RenderViewContextMenuProxy* proxy,
                                             Profile* profile)
    : proxy_(proxy), profile_(profile) {}

ReadWriteCardObserver::~ReadWriteCardObserver() = default;

void ReadWriteCardObserver::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& bounds_in_screen) {
  bounds_in_screen_ = bounds_in_screen;

  chromeos::ReadWriteCardsManager* cards_manager =
      chromeos::ReadWriteCardsManager::Get();
  CHECK(cards_manager);

  // Before cards_manager executes FetchController, sends a request to create
  // the editor session.
  cards_manager->TryCreatingEditorSession(params, profile_);
  cards_manager->FetchController(
      params, profile_,
      base::BindOnce(&ReadWriteCardObserver::OnFetchControllers,
                     weak_factory_.GetWeakPtr(), params));
}

void ReadWriteCardObserver::OnContextMenuViewBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  bounds_in_screen_ = bounds_in_screen;

  for (auto controller : read_write_card_controllers_) {
    if (!controller) {
      continue;
    }
    SetUiControllerContextMenuBounds(bounds_in_screen_);
    controller->OnAnchorBoundsChanged(bounds_in_screen_);
  }
}

void ReadWriteCardObserver::OnMenuClosed() {
  for (auto controller : read_write_card_controllers_) {
    if (!controller) {
      continue;
    }

    controller->OnDismiss(is_other_command_executed_);
  }

  read_write_card_controllers_.clear();
  is_other_command_executed_ = false;
}

void ReadWriteCardObserver::CommandWillBeExecuted(int command_id) {
  is_other_command_executed_ = true;
}

void ReadWriteCardObserver::OnTextSurroundingSelectionAvailable(
    const std::u16string& selected_text,
    const std::u16string& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  for (auto controller : read_write_card_controllers_) {
    if (!controller) {
      continue;
    }

    controller->OnTextAvailable(bounds_in_screen_,
                                base::UTF16ToUTF8(selected_text),
                                base::UTF16ToUTF8(surrounding_text));
  }
}

void ReadWriteCardObserver::OnFetchControllers(
    const content::ContextMenuParams& params,
    std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> controllers) {
  if (controllers.empty()) {
    read_write_card_controllers_.clear();
    return;
  }

  SetUiControllerContextMenuBounds(bounds_in_screen_);

  content::RenderFrameHost* focused_frame =
      proxy_->GetWebContents()->GetFocusedFrame();

  for (auto controller : controllers) {
    if (!controller) {
      continue;
    }

    if (focused_frame) {
      controller->OnContextMenuShown(profile_);
    }
    read_write_card_controllers_.emplace_back(controller.get());
  }

  if (focused_frame) {
    focused_frame->RequestTextSurroundingSelection(
        base::BindOnce(
            &ReadWriteCardObserver::OnTextSurroundingSelectionAvailable,
            weak_factory_.GetWeakPtr(), params.selection_text),
        kMaxSurroundingTextLength);
  }
}
