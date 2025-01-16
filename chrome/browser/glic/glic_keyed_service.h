// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_profile_configuration.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/views/view.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace glic {
class GlicFocusedTabManager;
class GlicProfileManager;
class GlicWindowController;

// The GlicKeyedService is created for each eligible (i.e. non-incognito,
// non-system, etc.) browser profile if Glic flags are enabled, regardless of
// whether the profile is enabled or disabled at runtime (currently possible via
// enterprise policy). This is required on disabled profiles since pieces of
// this service are the ones that monitor this runtime preference for changes
// and cause the UI to respond to it.
class GlicKeyedService : public KeyedService {
 public:
  explicit GlicKeyedService(content::BrowserContext* browser_context,
                            signin::IdentityManager* identity_manager,
                            GlicProfileManager* profile_manager);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

  // KeyedService
  void Shutdown() override;

  // Launches the Glic UI anchored at the given View object. When started from
  // the launcher, no anchor view is provided.
  void LaunchUI(views::View* glic_button_view);

  GlicWindowController& window_controller() { return window_controller_; }

  // Private API for the glic WebUI.
  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t>& window_id,
                 glic::mojom::WebClientHandler::CreateTabCallback callback);
  void OpenGlicSettingsPage();
  virtual void ClosePanel();
  void AttachPanel();
  void DetachPanel();
  std::optional<gfx::Size> ResizePanel(const gfx::Size& size);
  void SetPanelDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);
  void SetContextAccessIndicator(bool show);

  // Callback for changes to focused tab. When there is no focused tab,
  // |WebContents| will be nullptr.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const content::WebContents*)>;
  // Callback for changes to the context access indicator status.
  using ContextAccessIndicatorChangedCallback =
      base::RepeatingCallback<void(bool)>;

  // Registers |callback| to be called whenever the focused tab changes. This
  // includes when the active/selected tab for the profile changes (including
  // those resulting from browser/window changes) as well as when the primary
  // page of the focused tab has changed internally. Subscribers can filter for
  // specific subsets of changes they care about by holding on to their own
  // internal state. When the focused tab changes to nothing, this will be
  // called with nullptr as the supplied WebContents argument.
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

  // Registers a callback to be called any time the context access indicator
  // status changes. This is used to update UI effects on the focused tab
  // depending on whether the client has requested the indicators or not.
  base::CallbackListSubscription AddContextAccessIndicatorStatusChangedCallback(
      ContextAccessIndicatorChangedCallback callback);

  // Returns the currently focused tab or nullptr if there is none.
  content::WebContents* GetFocusedTab();

  // Returns whether the context access indicator should be shown for the web
  // contents. True iff the web contents is considered focused by
  // GlicFocusedTabManager and the web client has enabled the context access
  // indicator.
  bool IsContextAccessIndicatorShown(const content::WebContents* contents);

  bool is_context_access_indicator_enabled() const {
    return is_context_access_indicator_enabled_;
  }

  void GetContextFromFocusedTab(
      bool include_inner_text,
      bool include_viewport_screenshot,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  void SyncWebviewCookies(
      mojom::PageHandler::SyncWebviewCookiesCallback callback);

  void WebClientCreated();

  base::CallbackListSubscription AddWebClientCreatedCallback(
      base::OnceCallback<void()> callback);

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

 private:
  void OnFocusedTabChanged(const content::WebContents* focused_tab);

  // List of callbacks to be notified when the client requests a change to the
  // context access indicator status.
  base::RepeatingCallbackList<void(bool)>
      context_access_indicator_callback_list_;
  // The state of the context access indicator as set by the client.
  bool is_context_access_indicator_enabled_ = false;

  raw_ptr<content::BrowserContext> browser_context_;

  GlicProfileConfiguration configuration_;
  GlicWindowController window_controller_;
  GlicFocusedTabManager focused_tab_manager_;
  GlicCookieSynchronizer cookie_synchronizer_;
  // Unowned
  raw_ptr<GlicProfileManager> profile_manager_;
  base::OnceCallbackList<void()> web_client_created_callbacks_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
