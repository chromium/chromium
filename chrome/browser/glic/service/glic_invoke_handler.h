// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_

#include <memory>
#include <variant>
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
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace glic {

class GlicInstanceImpl;

class SequentialTaskGroup;

// Handles an invocation of Glic, parsing options and communicating with the
// instance's host.
class GlicInvokeHandler {
 public:
  using CompletionCallback =
      base::OnceCallback<void(GlicInstance*, GlicInvokeHandler*)>;

  struct TabSurface {
    raw_ptr<tabs::TabInterface> tab;
    bool is_new = false;
  };

  using ResolvedTarget = std::variant<TabSurface, Floating>;

  // Resolves the target surface to a specific tab.
  static ResolvedTarget ResolveTargetSurface(Profile* profile,
                                             const Target& target);

  // `tab` must be non-nullptr.
  // `completion_callback` should be called exactly once and results in
  // destruction of `this`.
  GlicInvokeHandler(
      GlicInstanceImpl& instance,
      ResolvedTarget resolved_target,
      GlicInvokeOptions options,
      GlicInvokeWithAutoSubmitOptions auto_submit_options,
      std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
      CompletionCallback completion_callback);
  ~GlicInvokeHandler();

  GlicInvokeHandler(const GlicInvokeHandler&) = delete;
  GlicInvokeHandler& operator=(const GlicInvokeHandler&) = delete;

  // Kicks off the invocation process.
  void Invoke();

 private:
  bool IsFloatingTarget() const;
  bool IsTabTarget() const;
  tabs::TabInterface& GetTab() const;
  mojom::InvokeOptionsPtr CreateMojoOptions();
  bool IsActuatingFeatureMode() const;

  // Deletes `this`. Exactly one of these methods will be called.
  void OnSuccess();
  void OnError(GlicInvokeError error);

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnInstanceWillBeDestroyed(GlicInstance* instance);
  void OnConversationInfoChanged(const mojom::ConversationInfo& info);
  const base::raw_ref<GlicInstanceImpl> instance_;
  ResolvedTarget resolved_target_;
  GlicInvokeOptions options_;
  std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey_;
  // Calling this synchronously destroys `this`.
  GlicInvokeWithAutoSubmitOptions auto_submit_options_;
  CompletionCallback completion_callback_;

  bool should_wait_for_load_ = false;
  base::CallbackListSubscription instance_destruction_subscription_;
  base::CallbackListSubscription tab_destruction_subscription_;
  base::CallbackListSubscription conversation_subscription_;
  base::OneShotTimer timeout_timer_;

  std::unique_ptr<SequentialTaskGroup> main_task_;

  base::WeakPtrFactory<GlicInvokeHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INVOKE_HANDLER_H_
