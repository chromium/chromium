// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_

#include "ash/style/ash_color_id.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class LabelButton;
}  // namespace views

namespace arc {

// This class creates a splash screen view as a bubble dialog. The view has a
// transparent background color, with a content box inserted in the middle. It
// also has a close button on the top right corner. This view is intended to be
// inserted into a window. The content container contains a logo, a heading
// text, a message box in vertical alignment.
class ArcSplashScreenDialogView : public views::BubbleDialogDelegateView,
                                  public views::ViewObserver,
                                  public wm::ActivationChangeObserver {
  METADATA_HEADER(ArcSplashScreenDialogView, views::BubbleDialogDelegateView)

 public:
  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(ArcSplashScreenDialogView* view) : view_(view) {}
    ~TestApi() = default;

    views::LabelButton* close_button() const { return view_->close_button_; }
    views::View* highlight_border() const { return view_->highlight_border_; }

   private:
    const raw_ptr<ArcSplashScreenDialogView> view_;
  };

  ArcSplashScreenDialogView(base::OnceClosure close_callback,
                            aura::Window* parent,
                            views::View* anchor,
                            bool is_for_unresizable);
  ArcSplashScreenDialogView(const ArcSplashScreenDialogView&) = delete;
  ArcSplashScreenDialogView& operator=(const ArcSplashScreenDialogView&) =
      delete;
  ~ArcSplashScreenDialogView() override;

  // Show a splash screen dialog to advertise resize lock feature
  static void Show(aura::Window* parent, bool is_for_unresizable);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // views::BubbleDialogDelegateView
  gfx::Rect GetBubbleBounds() override;

  // views::ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  class ArcSplashScreenWindowObserver;

  void OnCloseButtonClicked();

  raw_ptr<views::View> anchor_;
  raw_ptr<views::View> highlight_border_{nullptr};

  base::OnceClosure close_callback_;
  raw_ptr<views::LabelButton> close_button_ = nullptr;

  const ui::ColorId background_color_id_;

  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      anchor_highlight_observations_{this};

  std::unique_ptr<ArcSplashScreenWindowObserver> window_observer_;

  bool forwarding_activation_{false};
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_observation_{this};

  base::WeakPtrFactory<ArcSplashScreenDialogView> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
