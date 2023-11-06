// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_cards_manager.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr int kMaxSurroundingTextLength = 300;

}  // namespace

QuickAnswersMenuObserver::QuickAnswersMenuObserver(
    RenderViewContextMenuProxy* proxy,
    Profile* profile)
    : proxy_(proxy), profile_(profile) {}

QuickAnswersMenuObserver::~QuickAnswersMenuObserver() = default;

void QuickAnswersMenuObserver::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& bounds_in_screen) {
  chromeos::ReadWriteCardsManager* cards_manager =
      chromeos::ReadWriteCardsManager::Get();
  CHECK(cards_manager);

  read_write_card_controller_ =
      cards_manager->GetController(params, proxy_->GetBrowserContext());
  if (!read_write_card_controller_) {
    return;
  }

  bounds_in_screen_ = bounds_in_screen;
  content::RenderFrameHost* focused_frame =
      proxy_->GetWebContents()->GetFocusedFrame();
  if (focused_frame) {
    read_write_card_controller_->OnContextMenuShown(profile_);
    focused_frame->RequestTextSurroundingSelection(
        base::BindOnce(
            &QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable,
            weak_factory_.GetWeakPtr(), params.selection_text),
        kMaxSurroundingTextLength);
  }
}

void QuickAnswersMenuObserver::OnContextMenuViewBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  if (!read_write_card_controller_) {
    return;
  }

  bounds_in_screen_ = bounds_in_screen;
  read_write_card_controller_->OnAnchorBoundsChanged(bounds_in_screen);
}

void QuickAnswersMenuObserver::OnMenuClosed() {
  if (!read_write_card_controller_) {
    return;
  }

  read_write_card_controller_->OnDismiss(is_other_command_executed_);
  read_write_card_controller_ = nullptr;
  is_other_command_executed_ = false;
}

void QuickAnswersMenuObserver::CommandWillBeExecuted(int command_id) {
  is_other_command_executed_ = true;
}

void QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable(
    const std::u16string& selected_text,
    const std::u16string& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  if (!read_write_card_controller_) {
    return;
  }

  read_write_card_controller_->OnTextAvailable(
      bounds_in_screen_, base::UTF16ToUTF8(selected_text),
      base::UTF16ToUTF8(surrounding_text));
}
