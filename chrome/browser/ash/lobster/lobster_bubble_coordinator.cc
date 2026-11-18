// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_bubble_coordinator.h"

#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/lobster/lobster_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "net/base/url_util.h"

namespace ash {

LobsterBubbleCoordinator::LobsterBubbleCoordinator(
    base::RepeatingClosure on_widget_closed_callback)
    : on_widget_closed_callback_(std::move(on_widget_closed_callback)) {}

LobsterBubbleCoordinator::~LobsterBubbleCoordinator() {
  CloseUI();
}

void LobsterBubbleCoordinator::LoadUI(Profile* profile,
                                      std::optional<std::string_view> query,
                                      LobsterMode mode,
                                      const gfx::Rect& caret_bounds,
                                      bool should_show_feedback_ui) {
  CloseUI();

  GURL url = GURL(kChromeUILobsterURL);

  if (query.has_value()) {
    url = net::AppendOrReplaceQueryParameter(url, kLobsterPromptParamKey,
                                             query.value());
  }

  url = net::AppendOrReplaceQueryParameter(url, kLobsterModeParamKey,
                                           mode == LobsterMode::kInsert
                                               ? kLobsterInsertModeValue
                                               : kLobsterDownloadModeValue);

  url = net::AppendOrReplaceQueryParameter(
      url, kLobsterFeedbackEnabledParamKey,
      should_show_feedback_ui ? "true" : "false");

  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<MakoUntrustedUI>>(
      url, profile, IDS_ACCNAME_ORCA,
      /*esc_closes_ui=*/false);

  std::unique_ptr<LobsterView> lobster_view =
      std::make_unique<LobsterView>(contents_wrapper_.get(), caret_bounds);
  auto bubble = lobster_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(lobster_view));

  if (bubble->GetWidget()) {
    widget_observation_.Observe(bubble->GetWidget());
  }
}

void LobsterBubbleCoordinator::ShowUI() {
  if (contents_wrapper_) {
    contents_wrapper_->ShowUI();
  }
}

void LobsterBubbleCoordinator::CloseUI() {
  widget_observation_.Reset();
  if (contents_wrapper_) {
    contents_wrapper_->CloseUI();
    contents_wrapper_ = nullptr;
  }
}

void LobsterBubbleCoordinator::OnWidgetDestroying(views::Widget* widget) {
  CloseUI();
  if (on_widget_closed_callback_) {
    on_widget_closed_callback_.Run();
  }
}

}  // namespace ash
