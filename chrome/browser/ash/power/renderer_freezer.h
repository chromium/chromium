// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_RENDERER_FREEZER_H_
#define CHROME_BROWSER_ASH_POWER_RENDERER_FREEZER_H_

#include <memory>
#include <set>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace ash {

// Freezes the chrome renderers when the system is about to suspend and thaws
// them after the system fully resumes.  This class registers itself as a
// PowerManagerClient::Observer on creation and unregisters itself on
// destruction.
class RendererFreezer
    : public chromeos::PowerManagerClient::RenderProcessManagerDelegate,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  class Delegate {
   public:
    using ResultCallback = base::OnceCallback<void(bool)>;

    virtual ~Delegate() {}

    // If |frozen| is true, marks the renderer process |handle| to be frozen
    // when FreezeRenderers() is called; otherwise marks it to remain unfrozen.
    // Performs the operation asynchronously on the FILE thread.
    virtual void SetShouldFreezeRenderer(base::ProcessHandle handle,
                                         bool frozen) = 0;

    // Freezes the renderers marked for freezing by SetShouldFreezeRenderer().
    // Performs the operation asynchronously on the FILE thread.
    virtual void FreezeRenderers() = 0;

    // Thaws the chrome renderers that were frozen by the call to
    // FreezeRenderers().  Performs the operation asynchronously on the FILE
    // thread and runs |callback| with the result on the UI thread.
    virtual void ThawRenderers(ResultCallback callback) = 0;

    // Asynchronously checks on the FILE thread if the delegate can freeze
    // renderers and runs |callback| on the UI thread with the result.
    virtual void CheckCanFreezeRenderers(ResultCallback callback) = 0;
  };

  explicit RendererFreezer(std::unique_ptr<Delegate> delegate);

  RendererFreezer(const RendererFreezer&) = delete;
  RendererFreezer& operator=(const RendererFreezer&) = delete;

  ~RendererFreezer() override;

  // PowerManagerClient::RenderProcessManagerDelegate implementation.
  void SuspendImminent() override;
  void SuspendDone() override;

  // content::RenderProcessHostCreationObserver implementation.
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver overrides.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // Called after checking if the delegate is capable of freezing renderers.
  void OnCheckCanFreezeRenderersComplete(bool can_freeze);

  // Called after thawing the renderers has completed.
  void OnThawRenderersComplete(bool success);

  // Called whenever a new renderer process is created.
  void OnRenderProcessCreated(content::RenderProcessHost* rph);

  // Delegate that takes care of actually freezing and thawing renderers for us.
  std::unique_ptr<Delegate> delegate_;

  // Records whether the delegate is able to freeze renderers. If not,
  // RenderProcessHosts are not tracked for freezing.
  bool can_freeze_renderers_ = false;

  // Set that keeps track of the RenderProcessHosts for processes that are
  // hosting GCM extensions.
  std::set<int> gcm_extension_processes_;

  base::WeakPtrFactory<RendererFreezer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_RENDERER_FREEZER_H_
