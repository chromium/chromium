// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_
#define ASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class PropertyConverter;
class Window;
}  // namespace aura

namespace gfx {
class Insets;
}

namespace ws {
namespace mojom {
enum class WindowType;
}
}  // namespace ws

namespace ash {

// Provides the non-client frame and contents view for windows created by remote
// app processes.
class ASH_EXPORT NonClientFrameController : public views::WidgetDelegate,
                                            public aura::WindowObserver {
 public:
  // Creates a new NonClientFrameController and window to render the non-client
  // frame decorations. This deletes itself when |window| is destroyed. |parent|
  // is the parent to place the newly created window in, and may be null. If
  // |parent| is null, |context| is used to determine the parent Window. One of
  // |parent| or |context| must be non-null. |window_manager_client| may be
  // null for now. |bounds| is screen coordinates when |parent| is null,
  // otherwise local coordinates, see views::Widget::InitParams::bounds.
  NonClientFrameController(
      aura::Window* parent,
      aura::Window* context,
      const gfx::Rect& bounds,
      ws::mojom::WindowType window_type,
      aura::PropertyConverter* property_converter,
      std::map<std::string, std::vector<uint8_t>>* properties);

  // Returns the NonClientFrameController for the specified window, null if
  // one was not created.
  static NonClientFrameController* Get(aura::Window* window);

  // Returns the preferred client area insets.
  static gfx::Insets GetPreferredClientAreaInsets();

  // Returns the width needed to display the standard set of buttons on the
  // title bar.
  static int GetMaxTitleBarButtonWidth();

  aura::Window* window() { return window_; }

  // Stores |cursor| as this window's active cursor. It does not actually update
  // the active cursor by calling into CursorManager, but will update the return
  // value provided by the associated window's aura::WindowDelegate::GetCursor.
  void StoreCursor(const ui::Cursor& cursor);

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  bool ShouldShowWindowTitle() const override;
  void DeleteDelegate() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  ~NonClientFrameController() override;

  views::Widget* widget_;
  views::View* contents_view_ = nullptr;

  // WARNING: as widget delays destruction there is a portion of time when this
  // is null.
  aura::Window* window_;

  bool did_init_native_widget_ = false;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameController);
};

}  // namespace ash

#endif  // ASH_WM_NON_CLIENT_FRAME_CONTROLLER_H_
