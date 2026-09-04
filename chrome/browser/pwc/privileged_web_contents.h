// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_
#define CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace pwc {

class PwcApiBinder;

// Owns a WebContents that hosts remote content for a blessed component with
// elevated browser capabilities.
//
// Structural properties (see the PWC design doc):
// - A PrivilegedWebContents is not a tab and can never become one: it owns
//   its WebContents exclusively and exposes no ownership-transfer API.
// - The WebContents lives in the profile's default StoragePartition, so the
//   component shares the live cookie jar with ordinary browsing.
// - PrivilegedWebContents is the WebContentsDelegate for its WebContents.
//
// This is the skeleton: enforcement (process isolation, service worker
// controls, navigation policy, capability bridge) is added by later CLs.
class PrivilegedWebContents : public content::WebContentsDelegate,
                              public content::WebContentsObserver {
 public:
  // Creates a PrivilegedWebContents for `component` in `browser_context`.
  // `policy_delegate` supplies the component's origin allowlists and is
  // implemented by the component owner (see PwcPolicyDelegate); it is
  // consulted through the structural guardrails of PwcComponentPolicy. Must
  // only be called when pwc::mojom::features::kPrivilegedWebContents is
  // enabled.
  static std::unique_ptr<PrivilegedWebContents> Create(
      PrivilegedComponent component,
      content::BrowserContext* browser_context,
      std::unique_ptr<PwcPolicyDelegate> policy_delegate);

  // Returns the owning PrivilegedWebContents if `web_contents` is owned by
  // one, otherwise nullptr.
  static PrivilegedWebContents* FromWebContents(
      content::WebContents* web_contents);

  PrivilegedWebContents(const PrivilegedWebContents&) = delete;
  PrivilegedWebContents& operator=(const PrivilegedWebContents&) = delete;
  ~PrivilegedWebContents() override;

  content::WebContents* web_contents() { return web_contents_.get(); }
  const PwcComponentPolicy& policy() const { return policy_; }
  PrivilegedComponent component() const { return policy_.component(); }

  // The browser-side host for the capability bridge exposed to this PWC's
  // qualifying main frame. Owned by (and lives as long as) this
  // PrivilegedWebContents.
  PwcApiBinder& bridge() { return *bridge_; }

  // Registry for the serving component's PWC-scoped browser-side state. The
  // component (e.g. //chrome/browser/geic) creates and owns its own helper --
  // typically alongside the PrivilegedWebContents it creates -- and registers
  // it here via ScopedUnownedUserData; consumers with the PrivilegedWebContents
  // retrieve it by type (e.g. GeicHost::Get(pwc.unowned_user_data_host())).
  // PrivilegedWebContents does not own or depend on the component's helper, so
  // there is no dependency cycle. The host must outlive everything registered
  // in it, which holds because the component scopes its helper to this PWC.
  ui::UnownedUserDataHost& unowned_user_data_host() {
    return unowned_user_data_host_;
  }

  // Optional embedder delegate for forwarding non-security WebContentsDelegate
  // callbacks (e.g. keyboard events and zoom changes) to UI embedders.
  // The registered delegate must either outlive `PrivilegedWebContents` or call
  // `SetEmbedderDelegate(nullptr)` prior to its destruction.
  class EmbedderDelegate {
   public:
    virtual ~EmbedderDelegate() = default;
    virtual bool HandleKeyboardEvent(
        content::WebContents* source,
        const input::NativeWebKeyboardEvent& event);
    virtual void ContentsZoomChange(bool zoom_in);
  };

  void SetEmbedderDelegate(EmbedderDelegate* delegate) {
    embedder_delegate_ = delegate;
  }
  EmbedderDelegate* embedder_delegate() const { return embedder_delegate_; }

  // content::WebContentsDelegate:
  // Privileged content never prerenders: a prerendered page is activated into
  // the primary main frame without running navigation throttles, which would
  // let an off-allowlist page bypass PwcNavigationThrottle. Disabling
  // prerendering outright is the primary defense (the throttle also covers the
  // prerendered main frame as defense in depth).
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents,
      content::PreloadingTriggerType trigger_type) override;
  // A privileged WebContents never creates related windows
  // (ChromeContentBrowserClient::CanCreateWindow denies them), so this must be
  // unreachable.
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void ContentsZoomChange(bool zoom_in) override;

  // content::WebContentsObserver:
  // Disables the back-forward cache for every committed document, so a
  // privileged page is never restored from bfcache (which would skip the
  // navigation gauntlet). Cross-document back/forward becomes a fresh load.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  PrivilegedWebContents(PrivilegedComponent component,
                        content::BrowserContext* browser_context,
                        std::unique_ptr<PwcPolicyDelegate> policy_delegate);

  const PwcComponentPolicy policy_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<PwcApiBinder> bridge_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  raw_ptr<EmbedderDelegate> embedder_delegate_ = nullptr;
};

}  // namespace pwc

#endif  // CHROME_BROWSER_PWC_PRIVILEGED_WEB_CONTENTS_H_
