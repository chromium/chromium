// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_

#include "base/unguessable_token.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "ui/aura/window.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}  // namespace views

namespace ash {

class WindowManagementImpl : public blink::mojom::CrosWindowManagement {
 public:
  explicit WindowManagementImpl(content::BrowserContext* browser_context);
  ~WindowManagementImpl() override = default;

  void GetAllWindows(GetAllWindowsCallback callback) override;

  void SetWindowBounds(const base::UnguessableToken& id,
                       int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height,
                       SetWindowBoundsCallback callback) override;

  void SetFullscreen(const base::UnguessableToken& id,
                     bool fullscreen,
                     SetFullscreenCallback callback) override;

  void Maximize(const base::UnguessableToken& id,
                MaximizeCallback callback) override;

  void Minimize(const base::UnguessableToken& id,
                MinimizeCallback callback) override;

  void Focus(const base::UnguessableToken& id, FocusCallback callback) override;

  void Close(const base::UnguessableToken& id) override;

 private:
  // Returns ptr to top level window from window at given id.
  aura::Window* GetWindow(const base::UnguessableToken& id);

  // Returns ptr to top level widget from window at given id.
  views::Widget* GetWidget(const base::UnguessableToken& id);

  content::BrowserContext* browser_context_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_WINDOW_MANAGEMENT_WINDOW_MANAGEMENT_IMPL_H_
