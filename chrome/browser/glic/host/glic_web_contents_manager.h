// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "content/public/browser/visibility.h"

namespace content {
class WebContents;
}

namespace glic {
class Host;
class GlicWebClientManager;

// Abstract interface managing the lifecycle, presentation, and client routing
// of the WebContents backing a Glic panel instance.
//
// There are two concrete implementations:
// 1. `GlicWebUIContentsManager`: Hosts the guest client inside a nested
//    `<webview>` tag within a single WebUI WebContents (`chrome://glic`).
// 2. `GlicNoWebviewContentsManager`: Uses a standalone PrivilegedWebContents
//    (PWC) for the guest and an independent WebUI WebContents
//    (`chrome://glic/overlay`) for loading/error overlays that are swapped as
//    needed.
class GlicWebContentsManager {
 public:
  using WebContentsChangedCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  virtual ~GlicWebContentsManager() = default;

  // Attaches this manager's WebContents to the provided Host. This must be
  // called exactly once when the manager is connected to a live panel host.
  virtual void AttachToHost(Host* host) = 0;

  // Sets the visibility state of the managed WebContents.
  virtual void SetVisibility(content::Visibility visibility) = 0;

  // Returns the active WebContents currently presented in the view hierarchy
  // (e.g. the primary WebUI contents, the guest contents, or the
  // loading/error overlay).
  //
  // Returns nullptr when no WebContents is currently active for display (i.e.
  // while warming in the background pool, or when attached to a host but
  // hidden). Once about to be shown or showing, this is guaranteed non-null.
  virtual content::WebContents* active_web_contents() const = 0;

  // Notifies the manager when actuation state changes.
  virtual void OnActuatingChanged(bool actuating) = 0;

  // Notifies the manager when task tabs visibility changes.
  virtual void OnTaskTabsVisibilityChanged(bool has_visible_tab) = 0;

  // Releases ownership of the underlying WebContents, transferring it to the
  // caller.
  // TODO(b/555365681): Only used for tab embedders and slated for removal once
  // tab embedders are deleted.
  virtual std::unique_ptr<content::WebContents> ReleaseWebContents() = 0;

  // Reclaims ownership of a previously released WebContents.
  // TODO(b/555365681): Only used for tab embedders and slated for removal once
  // tab embedders are deleted.
  virtual void ReclaimWebContents(
      std::unique_ptr<content::WebContents> web_contents) = 0;

  // Registers a callback to be notified when the active WebContents returned by
  // `web_contents()` changes (e.g. when swapping from loading overlay to
  // guest).
  virtual base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) = 0;

  // Returns the manager responsible for Mojo routing to the guest web client.
  virtual GlicWebClientManager& web_client_manager() = 0;

  // Returns true if the manager encountered an error or crash from which it
  // may be able to recover by reloading (creating a fresh manager instance)
  // when shown or taken from the warming pool.
  //
  // Returns false if the manager is healthy or if it is in an unrecoverable
  // state (such as a sign-in or policy error panel) that should be presented
  // to the user rather than reloaded.
  virtual bool ShouldReloadOnShow() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_MANAGER_H_
