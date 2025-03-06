// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_page_handler.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserWindowInterface;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace glic {
class AuthController;
class GlicEnabling;
class GlicFocusedTabManager;
class GlicMetrics;
class GlicProfileManager;
class GlicWindowController;
class GlicWindowController;
class GlicScreenshotCapturer;
struct FocusedTabData;

// The GlicKeyedService is created for each eligible (i.e. non-incognito,
// non-system, etc.) browser profile if Glic flags are enabled, regardless
// of whether the profile is enabled or disabled at runtime (currently
// possible via enterprise policy). This is required on disabled profiles
// since pieces of this service are the ones that monitor this runtime
// preference for changes and cause the UI to respond to it.
class GlicKeyedService : public KeyedService {
 public:
  explicit GlicKeyedService(Profile* profile,
                            signin::IdentityManager* identity_manager,
                            GlicProfileManager* profile_manager);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

  // Convenience method, may return nullptr.
  static GlicKeyedService* Get(content::BrowserContext* context);

  // KeyedService
  void Shutdown() override;

  // Show, summon or activate the panel, or close it if it's already active and
  // prevent_close is false. If glic_button_view is non-null, attach the panel
  // to that view's Browser.
  void ToggleUI(BrowserWindowInterface* bwi,
                bool prevent_close,
                InvocationSource source);

  GlicEnabling& enabling() { return *enabling_.get(); }
  GlicMetrics* metrics() { return metrics_.get(); }
  GlicWindowController& window_controller() { return *window_controller_; }

  // Called when a webview guest is created within a chrome://glic WebUI.
  void GuestAdded(content::WebContents* guest_contents);

  // Called when a `GlicPageHandler` is created.
  void PageHandlerAdded(GlicPageHandler* page_handler);

  // Called when a `GlicPageHandler` is about to be destroyed.
  void PageHandlerRemoved(GlicPageHandler* page_handler);

  bool IsWindowShowing() const;

  // Private API for the glic WebUI.

  // CreateTab is used by both the FRE page and the glic web client to open a
  // URL in a new tab.
  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t>& window_id,
                 glic::mojom::WebClientHandler::CreateTabCallback callback);
  void OpenGlicSettingsPage();
  virtual void ClosePanel();
  void AttachPanel();
  void DetachPanel();
  void ResizePanel(const gfx::Size& size,
                   base::TimeDelta duration,
                   base::OnceClosure callback);
  void SetPanelDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);
  void SetContextAccessIndicator(bool show);
  void NotifyWindowIntentToShow();

  // Callback for changes to focused tab data.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(FocusedTabData)>;
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

  // Returns the currently focused tab data union which contains the focused
  // tab's web contents, or the candidate for the focused tab and why it was
  // deemed invalid for focus, or an error stating why no candidate was
  // available.
  FocusedTabData GetFocusedTabData();

  // Returns whether the context access indicator should be shown for the web
  // contents. True iff the web contents is considered focused by
  // GlicFocusedTabManager and the web client has enabled the context access
  // indicator.
  bool IsContextAccessIndicatorShown(const content::WebContents* contents);

  bool is_context_access_indicator_enabled() const {
    return is_context_access_indicator_enabled_;
  }

  void GetContextFromFocusedTab(
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback);

  AuthController& GetAuthController() { return *auth_controller_; }

  void WebClientCreated();

  base::CallbackListSubscription AddWebClientCreatedCallback(
      base::OnceCallback<void()> callback);

  bool IsActiveWebContents(content::WebContents* contents);

  virtual void TryPreload();
  void Reload();

  Profile* profile() const { return profile_; }

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

 private:
  GlicPageHandler* GetPageHandler(const content::WebContents* webui_contents);

  // List of callbacks to be notified when the client requests a change to the
  // context access indicator status.
  base::RepeatingCallbackList<void(bool)>
      context_access_indicator_callback_list_;
  // The state of the context access indicator as set by the client.
  bool is_context_access_indicator_enabled_ = false;

  raw_ptr<Profile> profile_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
  std::unique_ptr<GlicWindowController> window_controller_;
  GlicFocusedTabManager focused_tab_manager_;
  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;
  std::unique_ptr<AuthController> auth_controller_;
  // Unowned
  raw_ptr<GlicProfileManager> profile_manager_;
  base::OnceCallbackList<void()> web_client_created_callbacks_;
  // The set of live `GlicPageHandler`s.
  base::flat_set<raw_ptr<GlicPageHandler>> page_handlers_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
