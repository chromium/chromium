// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_connected_view.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {

class AppLoadingIcon;
class PhoneHubMoreAppsButton;

namespace phonehub {
class PhoneHubManager;
}

// A view in Phone Hub bubble that allows user to relaunch a streamed app from
// the recent apps list.
class ASH_EXPORT PhoneHubRecentAppsView
    : public views::View,
      public phonehub::RecentAppsInteractionHandler::Observer {
  METADATA_HEADER(PhoneHubRecentAppsView, views::View)

 public:
  explicit PhoneHubRecentAppsView(
      phonehub::RecentAppsInteractionHandler* recent_apps_interaction_handler,
      phonehub::PhoneHubManager* phone_hub_manager,
      PhoneConnectedView* connected_view);
  ~PhoneHubRecentAppsView() override;
  PhoneHubRecentAppsView(PhoneHubRecentAppsView&) = delete;
  PhoneHubRecentAppsView operator=(PhoneHubRecentAppsView&) = delete;

  // phonehub::RecentAppsInteractionHandler::Observer:
  void OnRecentAppsUiStateUpdated() override;

 protected:
  friend class RecentAppButtonsViewTest;

 private:
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest, TaskViewVisibility);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           TaskViewVisibility_NetworkConnectionFlagDisabled);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest, LoadingStateVisibility);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           ConnectionFailedStateVisibility);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           SingleRecentAppButtonsView);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           MultipleRecentAppButtonsView);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           MultipleRecentAppButtonsWithMoreAppsButtonView);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           LogRecentAppsTransitionToFailedLatency);
  FRIEND_TEST_ALL_PREFIXES(RecentAppButtonsViewTest,
                           LogRecentAppsTransitionToSuccessLatency);

  class PlaceholderView;

  class HeaderView : public views::View {
    METADATA_HEADER(HeaderView, views::View)

   public:
    explicit HeaderView(views::ImageButton::PressedCallback callback);
    ~HeaderView() override = default;
    HeaderView(HeaderView&) = delete;
    HeaderView operator=(HeaderView&) = delete;

    void SetErrorButtonVisible(bool is_visible);

    views::ImageButton* get_error_button_for_test() { return error_button_; }

   private:
    raw_ptr<views::ImageButton> error_button_;
  };

  class RecentAppButtonsView : public views::View {
    METADATA_HEADER(RecentAppButtonsView, views::View)

   public:
    RecentAppButtonsView();
    ~RecentAppButtonsView() override;
    RecentAppButtonsView(RecentAppButtonsView&) = delete;
    RecentAppButtonsView operator=(RecentAppButtonsView&) = delete;

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;
    void Layout(PassKey) override;

    views::View* AddRecentAppButton(
        std::unique_ptr<views::View> recent_app_button);
    void Reset();

    base::WeakPtr<RecentAppButtonsView> GetWeakPtr();

   private:
    base::WeakPtrFactory<RecentAppButtonsView> weak_ptr_factory_{this};
  };

  class LoadingView : public views::BoxLayoutView {
    METADATA_HEADER(LoadingView, views::BoxLayoutView)

   public:
    LoadingView();
    ~LoadingView() override;
    LoadingView(LoadingView&) = delete;
    LoadingView operator=(LoadingView&) = delete;

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;
    void Layout(PassKey) override;

    void StartLoadingAnimation();
    void StopLoadingAnimation();

    base::WeakPtr<LoadingView> GetWeakPtr();

   private:
    std::vector<raw_ptr<AppLoadingIcon, VectorExperimental>> app_loading_icons_;
    raw_ptr<PhoneHubMoreAppsButton> more_apps_button_ = nullptr;
    base::WeakPtrFactory<LoadingView> weak_ptr_factory_{this};
  };

  // Update the view to reflect the most recently opened apps.
  void Update();

  // Switch to full apps list view.
  void SwitchToFullAppsList();

  void ShowConnectionErrorDialog();

  // Apply an opacity animation when swapping out the LoadingView for the
  // RecentAppButtonsView and vice-versa.
  void FadeOutLoadingView();
  void FadeOutRecentAppsButtonView();

  // Generate more apps button.
  std::unique_ptr<views::View> GenerateMoreAppsButton();

  views::ImageButton* get_error_button_for_test() {
    return header_view_->get_error_button_for_test();
  }
  LoadingView* get_loading_view_for_test() { return loading_view_; }

  // Timers to measure the latency between loading to error, loading to app
  // icons, and error to app icons.
  base::TimeTicks loading_animation_start_time_ = base::TimeTicks();
  base::TimeTicks error_button_start_time_ = base::TimeTicks();

  raw_ptr<RecentAppButtonsView> recent_app_buttons_view_ = nullptr;
  std::vector<raw_ptr<views::View, VectorExperimental>> recent_app_button_list_;
  raw_ptr<phonehub::RecentAppsInteractionHandler>
      recent_apps_interaction_handler_ = nullptr;
  raw_ptr<phonehub::PhoneHubManager> phone_hub_manager_ = nullptr;
  raw_ptr<PlaceholderView> placeholder_view_ = nullptr;
  raw_ptr<HeaderView> header_view_ = nullptr;
  raw_ptr<LoadingView> loading_view_ = nullptr;
  raw_ptr<PhoneConnectedView> connected_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APPS_VIEW_H_
