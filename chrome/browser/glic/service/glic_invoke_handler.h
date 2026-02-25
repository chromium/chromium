// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInstanceImpl;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler : public Host::Observer {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GlicInstance*, GlicInvokeHandler*)>;

  GlicInvokeHandler(GlicInstanceImpl& instance,
                    tabs::TabInterface* tab,
                    GlicInvokeOptions options,
                    CompletionCallback completion_callback);
  ~GlicInvokeHandler() override;

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

  // Kicks off the invocation process.
  void Invoke();

  // Ends the invocation process with the given error.
  // May delete this.
  void OnError(GlicInvokeError error);

  // Host::Observer:
  void ClientReadyToShow(const mojom::OpenPanelInfo&) override;

 private:
  void SendToClient();
  mojom::InvokeOptionsPtr CreateMojoOptions();
  // May delete this.
  void OnSuccess();
  void OnTabClosed(tabs::TabInterface* tab);

  const base::raw_ref<GlicInstanceImpl> instance_;
  GlicInvokeOptions options_;
  CompletionCallback completion_callback_;

  base::CallbackListSubscription tab_destruction_subscription_;
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};
  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<GlicInvokeHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
