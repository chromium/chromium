// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicInstanceImpl;

class SequentialTaskGroup;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GlicInstance*, GlicInvokeHandler*)>;

  struct ResolvedTarget {
    raw_ptr<tabs::TabInterface> tab = nullptr;
    bool is_new = false;
  };

  // Resolves the target surface to a specific tab.
  static ResolvedTarget ResolveTargetSurface(Profile* profile,
                                             const Target& target);

  GlicInvokeHandler(
      GlicInstanceImpl& instance,
      ResolvedTarget resolved_target,
      GlicInvokeOptions options,
      std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
      CompletionCallback completion_callback);
  ~GlicInvokeHandler();

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

  // Kicks off the invocation process.
  void Invoke();

  // Ends the invocation process with the given error.
  // May delete this.
  void OnError(GlicInvokeError error);

 private:
  mojom::InvokeOptionsPtr CreateMojoOptions();
  bool RequiresAutoSubmitIncompatibleFre() const;
  bool RequiresOverrideIncompatibleFre() const;

  // May delete this.
  void OnSuccess();
  void OnTabClosed(tabs::TabInterface* tab);

  const base::raw_ref<GlicInstanceImpl> instance_;
  raw_ptr<tabs::TabInterface> tab_;
  GlicInvokeOptions options_;
  std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey_;
  CompletionCallback completion_callback_;

  bool should_wait_for_load_ = false;
  base::CallbackListSubscription tab_destruction_subscription_;
  base::OneShotTimer timeout_timer_;

  std::unique_ptr<SequentialTaskGroup> main_task_;

  base::WeakPtrFactory<GlicInvokeHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
