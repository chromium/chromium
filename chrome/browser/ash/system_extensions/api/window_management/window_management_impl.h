// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_

#include "ash/wm/wm_default_layout_manager.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"

namespace views {
class Widget;
}  // namespace views

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class WindowManagementImpl : public WmDefaultLayoutManager,
                             public blink::mojom::CrosWindowManagement {
 public:
  explicit WindowManagementImpl(
      int32_t render_process_host_id,
      mojo::PendingAssociatedRemote<blink::mojom::CrosWindowManagementObserver>
          observer_pending_remote);
  ~WindowManagementImpl() override;

  // Sends a 'start' event to the renderer through the
  // blink::mojom::CrosWindowManagementObserver interface.
  void DispatchStartEvent();

  void DispatchWindowClosedEvent(const base::UnguessableToken& id);

  void DispatchWindowOpenedEvent(const base::UnguessableToken& id);

  // Sends an AcceleratorEvent to the renderer through the
  // blink::mojom::CrosWindowManagementObserver interface.
  void DispatchAcceleratorEvent(blink::mojom::AcceleratorEventPtr event_ptr);

  // blink::mojom::CrosWindowManagement
  void GetAllWindows(GetAllWindowsCallback callback) override;
  void MoveTo(const base::UnguessableToken& id,
              int32_t x,
              int32_t y,
              MoveToCallback callback) override;
  void MoveBy(const base::UnguessableToken& id,
              int32_t delta_x,
              int32_t delta_y,
              MoveByCallback callback) override;
  void ResizeTo(const base::UnguessableToken& id,
                int32_t width,
                int32_t height,
                ResizeToCallback callback) override;
  void ResizeBy(const base::UnguessableToken& id,
                int32_t delta_width,
                int32_t delta_height,
                ResizeByCallback callback) override;
  void SetFullscreen(const base::UnguessableToken& id,
                     bool fullscreen,
                     SetFullscreenCallback callback) override;
  void Maximize(const base::UnguessableToken& id,
                MaximizeCallback callback) override;
  void Minimize(const base::UnguessableToken& id,
                MinimizeCallback callback) override;
  void Restore(const base::UnguessableToken& id,
               RestoreCallback callback) override;
  void Focus(const base::UnguessableToken& id, FocusCallback callback) override;
  void Close(const base::UnguessableToken& id, CloseCallback callback) override;
  void GetAllScreens(GetAllScreensCallback callback) override;

 private:
  // Returns profile attached to the render process host id.
  Profile* GetProfile();

  // Returns ptr to top level window from window at given id.
  aura::Window* GetWindow(const base::UnguessableToken& id);

  // Returns ptr to top level widget from window at given id.
  views::Widget* GetWidget(const base::UnguessableToken& id);

  int32_t render_process_host_id_;

  // Used to send events to the renderer.
  mojo::AssociatedRemote<blink::mojom::CrosWindowManagementObserver> observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
