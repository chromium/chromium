// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_APP_BASE_WINDOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_APP_BASE_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace extensions {

class AppWindow;
class NativeAppWindow;

// A custom ui::BaseWindow to be given to a WindowController. It
// allows us to constrain some operations on application windows (like
// SetBounds).
class AppBaseWindow : public ui::BaseWindow {
 public:
  explicit AppBaseWindow(AppWindow* app_window);

  AppBaseWindow(const AppBaseWindow&) = delete;
  AppBaseWindow& operator=(const AppBaseWindow&) = delete;

  virtual ~AppBaseWindow();

 private:
  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void ShowInactive() override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

  NativeAppWindow* GetBaseWindow() const;

  raw_ptr<AppWindow> app_window_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_APP_BASE_WINDOW_H_
