// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {
class GlicFocusedTabManager;
class GlicProfileManager;
class GlicWindowController;

class GlicKeyedService : public KeyedService {
 public:
  explicit GlicKeyedService(content::BrowserContext* browser_context,
                            GlicProfileManager* profile_manager);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

  // Launches the Glic UI.
  void LaunchUI();

  GlicWindowController* window_controller() { return window_controller_.get(); }

  // Private API for the glic WebUI.
  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t>& window_id,
                 glic::mojom::WebClientHandler::CreateTabCallback callback);
  virtual void ClosePanel();
  std::optional<gfx::Size> ResizePanel(const gfx::Size& size);

  void GetContextFromFocusedTab(
      bool include_inner_text,
      bool include_viewport_screenshot,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

 private:
  raw_ptr<content::BrowserContext> browser_context_;

  std::unique_ptr<GlicWindowController> window_controller_;
  GlicFocusedTabManager focused_tab_manager_;
  // Unowned
  raw_ptr<GlicProfileManager> profile_manager_;
  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
