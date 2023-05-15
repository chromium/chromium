// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_DIALOG_H_
#define ASH_STYLE_SYSTEM_DIALOG_H_

#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

class SystemDialogDelegateView;

// SystemDialog creates a dialog widget with given system dialog delegate view
// as contents and parented to the host window. The dialog widget's bounds are
// adjusted according to the host window bounds.
class SystemDialog : public aura::WindowObserver {
 public:
  SystemDialog(std::unique_ptr<SystemDialogDelegateView> dialog_view,
               aura::Window* host_window);
  SystemDialog(const SystemDialog&) = delete;
  SystemDialog& operator=(const SystemDialog&) = delete;
  ~SystemDialog() override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

 protected:
  // Adjusts the dialog bounds according to the bounds of `host_window_`.
  virtual void UpdateDialogBounds();

 private:
  // The parent window of the dialog.
  const raw_ptr<aura::Window> host_window_;
  // The dialog widget owned by its native widget.
  raw_ptr<views::Widget> widget_ = nullptr;

  // The observation observing the dialog's native window and host window.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_DIALOG_H_
