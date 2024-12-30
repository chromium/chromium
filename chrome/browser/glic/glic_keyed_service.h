// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_profile_configuration.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/views/view.h"

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

  // Launches the Glic UI anchored at the given View object. When started from
  // the launcher, no anchor view is provided.
  void LaunchUI(views::View* glic_button_view);

  GlicWindowController& window_controller() { return window_controller_; }

  // Private API for the glic WebUI.
  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t>& window_id,
                 glic::mojom::WebClientHandler::CreateTabCallback callback);
  virtual void ClosePanel();
  std::optional<gfx::Size> ResizePanel(const gfx::Size& size);
  void SetPanelDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  // Callback for changes to focused tab. When there is no focused tab,
  // |WebContents| will be nullptr.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const content::WebContents*)>;

  // Registers |callback| to be called whenever the focused tab changes. This
  // includes when the active/selected tab for the profile changes (including
  // those resulting from browser/window changes) as well as when the primary
  // page of the focused tab has changed internally. Subscribers can filter for
  // specific subsets of changes they care about by holding on to their own
  // internal state. When the focused tab changes to nothing, this will be
  // called with nullptr as the supplied WebContents argument.
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

  // Returns the currently focused tab or nullptr if there is none.
  content::WebContents* GetFocusedTab();

  void GetContextFromFocusedTab(
      bool include_inner_text,
      bool include_viewport_screenshot,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

 private:
  void OnFocusedTabChanged(const content::WebContents* focused_tab);

  raw_ptr<content::BrowserContext> browser_context_;

  GlicProfileConfiguration configuration_;
  GlicWindowController window_controller_;
  GlicFocusedTabManager focused_tab_manager_;
  // Unowned
  raw_ptr<GlicProfileManager> profile_manager_;

  base::CallbackListSubscription focused_tab_changed_subscription_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
