// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
class MdTextButton;
}  // namespace views

namespace arc {

// This class creates a splash screen view as a bubble dialog. The view has a
// transparent background color, with a content box inserted in the middle. It
// also has a close button on the top right corner. This view is intended to be
// inserted into a window. The content container contains a logo, a heading
// text, a message box in vertical alignment.
class ArcSplashScreenDialogView : public views::BubbleDialogDelegateView,
                                  public wm::ActivationChangeObserver {
 public:
  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(ArcSplashScreenDialogView* view) : view_(view) {}
    ~TestApi() = default;

    views::MdTextButton* close_button() const { return view_->close_button_; }
    views::View* highlight_border() const { return view_->highlight_border_; }

   private:
    ArcSplashScreenDialogView* const view_;
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
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  class ArcSplashScreenWindowObserver;

  void OnCloseButtonClicked();

  views::View* const anchor_;
  views::View* highlight_border_{nullptr};

  base::OnceClosure close_callback_;
  views::MdTextButton* close_button_ = nullptr;
  std::unique_ptr<ArcSplashScreenWindowObserver> window_observer_;

  bool forwarding_activation_{false};
  base::ScopedObservation<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_observation_{this};

  base::WeakPtrFactory<ArcSplashScreenDialogView> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
