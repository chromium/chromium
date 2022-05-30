// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "ui/aura/window.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class WindowManagementImpl : public blink::mojom::CrosWindowManagement {
 public:
  explicit WindowManagementImpl(int32_t render_process_host_id);
  ~WindowManagementImpl() override = default;

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

  void Focus(const base::UnguessableToken& id, FocusCallback callback) override;

  void Close(const base::UnguessableToken& id, CloseCallback callback) override;

 private:
  // Returns profile attached to the render process host id.
  Profile* GetProfile();

  // Returns ptr to top level window from window at given id.
  aura::Window* GetWindow(const base::UnguessableToken& id);

  // Returns ptr to top level widget from window at given id.
  views::Widget* GetWidget(const base::UnguessableToken& id);

  int32_t render_process_host_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
