// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace glic {
class GlicInstance;
class GlicSharingManagerInternal;

class GlicExperimentalTriggeringManager {
 public:
  // Why the owning GlicInstance is going away mid-session. A user closing the
  // panel never tears the instance down while a session is active (it is kept
  // alive and the invoke is cancelled with GlicInvokeError::kPanelClosed
  // instead), so that case is attributed by the invoke error, not here.
  enum class InstanceTeardownReason {
    kTabClosed,
    kInstanceDestroyed,
  };

  GlicExperimentalTriggeringManager(
      GlicInstance* instance,
      GlicSharingManagerInternal* sharing_manager);
  GlicExperimentalTriggeringManager(const GlicExperimentalTriggeringManager&) =
      delete;
  GlicExperimentalTriggeringManager& operator=(
      const GlicExperimentalTriggeringManager&) = delete;
  virtual ~GlicExperimentalTriggeringManager();

  void Bind(
      mojo::PendingRemote<glic::mojom::ExperimentalTriggeringClient> client);

  // Returns true if the web client's ExperimentalTriggeringClient remote is
  // currently bound and connected.
  virtual bool IsClientBound() const;

  // Subscribes `callback` to be notified whenever a new web client binds its
  // ExperimentalTriggeringClient remote (e.g. after a guest navigation/reload).
  base::CallbackListSubscription AddClientBoundCallback(
      base::RepeatingClosure callback);

  // Tracks whether at least one experimental triggering actuation session is
  // actively using the owning GlicInstance. The returned ScopedClosureRunner
  // releases the session registration when run or destroyed. Returns a null
  // runner (and never reports an active session) when
  // features::kGlicExperimentalTriggeringKeepInstanceOnUnbind is disabled.
  bool HasActiveActuationSession() const {
    return active_actuation_sessions_ > 0;
  }
  base::ScopedClosureRunner RegisterActiveActuationSession();
  base::CallbackListSubscription AddActiveActuationSessionChangedCallback(
      base::RepeatingCallback<void(bool)> callback);

  // Called by GlicInstanceImpl::UnbindEmbedder() when it would otherwise unpin
  // an unused `tab_handle`. While an actuation session is active, remembers the
  // tab and returns true so the caller leaves it pinned (keeping the instance
  // alive); the tab is unpinned when the last session ends if it is still
  // unused and not bound to the instance again. Returns false otherwise.
  bool DeferUnusedTabUnpinOnUnbind(tabs::TabHandle tab_handle);

  // Returns the most recently unbound tab whose unpin is still deferred, if it
  // is still open, still pinned, and not bound to another instance. While the
  // panel is closed this is the surface the session's turns continue on.
  tabs::TabInterface* GetLastDeferredUnpinTab() const;

  // Synchronously notifies subscribers when the owning GlicInstance is about to
  // be closed and destroyed (or shut down), before its WebUI/guest WebContents
  // are torn down. Fires at most once; the reason is derived from whether the
  // instance's last pinned tab was closed.
  base::CallbackListSubscription AddInstanceWillBeDestroyedCallback(
      base::RepeatingCallback<void(InstanceTeardownReason)> callback);
  void NotifyInstanceWillBeDestroyed(InstanceTeardownReason reason);

  virtual void CaptureAndUploadEncryptedScreenshot(
      const std::vector<uint8_t>& public_key,
      const std::vector<uint8_t>& auth_secret,
      base::OnceCallback<void(
          base::expected<std::string, ScreenshotResult::Status>)> callback);

  virtual void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
          handler,
      base::OnceCallback<void(bool)> success_status_callback);

  // Forwards an opaque, serialized confirmation response to the web client so
  // that it can resolve a pending confirmation dialog. `callback` is run with
  // true if the web client applied the response, and false otherwise
  // (including if the web client is not connected or drops the reply).
  virtual void SubmitConfirmationResponse(
      const std::vector<uint8_t>& response,
      base::OnceCallback<void(bool accepted)> callback);

 protected:
  void NotifyClientBound();

 private:
  void OnPageContextFetchedForEncryption(
      GlicMetricReporter reporter,
      std::vector<uint8_t> public_key,
      std::vector<uint8_t> auth_secret,
      base::OnceCallback<
          void(base::expected<std::string, ScreenshotResult::Status>)> callback,
      glic::GlicGetContextResult result);
  void OnScreenshotEncrypted(
      GlicMetricReporter reporter,
      uint32_t width_pixels,
      uint32_t height_pixels,
      std::string mime_type,
      glic::mojom::ImageOriginAnnotationsPtr origin_annotations,
      base::OnceCallback<
          void(base::expected<std::string, ScreenshotResult::Status>)> callback,
      std::optional<std::vector<uint8_t>> encrypted_payload_bytes);

  void OnActiveActuationSessionEnded();
  void OnInstanceWillBeDestroyed(GlicInstance* instance);
  void OnTabPinningStatusEvent(tabs::TabInterface* tab,
                               GlicPinningStatusEvent event);
  // Unpins tabs deferred by DeferUnusedTabUnpinOnUnbind() that are still
  // pinned, still unused, and no longer bound to `instance_`.
  void UnpinDeferredTabs();

  raw_ptr<glic::GlicInstance> instance_;
  raw_ptr<glic::GlicSharingManagerInternal> sharing_manager_;
  mojo::Remote<glic::mojom::ExperimentalTriggeringClient> client_;
  base::RepeatingClosureList client_bound_callbacks_;
  int active_actuation_sessions_ = 0;
  // True when the last tab pinned to `instance_` was unpinned because it was
  // closed, and nothing has been pinned since. Decides between kTabClosed and
  // kInstanceDestroyed when the instance is torn down.
  bool last_pinned_tab_closed_ = false;
  bool instance_will_be_destroyed_notified_ = false;
  // Tabs whose unused-tab unpin was deferred by DeferUnusedTabUnpinOnUnbind(),
  // most recently unbound last.
  std::vector<tabs::TabHandle> deferred_unpin_tabs_;
  base::CallbackListSubscription instance_will_be_destroyed_subscription_;
  base::CallbackListSubscription tab_pinning_status_subscription_;
  base::RepeatingCallbackList<void(bool)>
      active_actuation_session_changed_callbacks_;
  base::RepeatingCallbackList<void(InstanceTeardownReason)>
      instance_will_be_destroyed_callbacks_;
  base::WeakPtrFactory<GlicExperimentalTriggeringManager> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_
