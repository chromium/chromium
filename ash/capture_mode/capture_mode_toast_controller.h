// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TOAST_CONTROLLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TOAST_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class CaptureModeSession;
class SystemToastView;

// Defines the capture toast type that Capture Mode is currently using.
enum class CaptureToastType {
  kUserNudge,
  kCameraPreview,
};

// Controls the capture mode toast shown conditionally in the capture session.
class ASH_EXPORT CaptureModeToastController : public views::WidgetObserver {
 public:
  explicit CaptureModeToastController(CaptureModeSession* session);
  CaptureModeToastController(const CaptureModeToastController&) = delete;
  CaptureModeToastController& operator=(const CaptureModeToastController&) =
      delete;
  ~CaptureModeToastController() override;

  const CaptureToastType* current_toast_type() const {
    return current_toast_type_ ? &(*current_toast_type_) : nullptr;
  }
  views::Widget* capture_toast_widget() const {
    return capture_toast_widget_.get();
  }

  // Shows `capture_toast_widget_` with label text defined by the given
  // `capture_toast_type`; if `capture_toast_widget_` doesn't exist, creates
  // one. Otherwise, just updates the label text for it if needed.
  void ShowCaptureToast(CaptureToastType capture_toast_type);

  void MaybeDismissCaptureToast(CaptureToastType capture_toast_type,
                                bool animate = true);

  // Called when we need to dismiss the current toast in spite of the toast
  // type. For example, when the settings menu is shown, the toast should be
  // dismissed no matter what type it is.
  void DismissCurrentToastIfAny();

  void MaybeRepositionCaptureToast();

  // Return the layer of `capture_toast_widget_` if it exists.
  ui::Layer* MaybeGetToastLayer();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::OneShotTimer* capture_toast_dismiss_timer_for_test() {
    return &capture_toast_dismiss_timer_;
  }

 private:
  // Initializes the toast widget and its contents.
  void BuildCaptureToastWidget(const std::u16string& text);

  gfx::Rect CalculateToastWidgetBoundsInScreen() const;

  // The session that owns `this`. Guaranteed to be not null for the lifetime of
  // `this`.
  const raw_ptr<CaptureModeSession> capture_session_;

  // The capture toast widget and its contents view.
  views::UniqueWidgetPtr capture_toast_widget_;
  raw_ptr<SystemToastView> toast_contents_view_ = nullptr;

  // Stores the toast type of the `capture_toast_widget_` after it's created.
  std::optional<CaptureToastType> current_toast_type_;

  // Started when `capture_toast_widget_` is shown for `kCameraPreview`. Runs
  // MaybeDismissCaptureToast() to fade out the capture toast if possible.
  base::OneShotTimer capture_toast_dismiss_timer_;

  base::WeakPtrFactory<CaptureModeToastController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TOAST_CONTROLLER_H_
