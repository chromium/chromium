// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_
#define CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_library_provider.h"

namespace screen_ai {

// Provides the Screen AI library that `ScreenAIInstallState` keeps track of.
class ScreenAILibraryProviderImpl : public ScreenAILibraryProvider,
                                    public ScreenAIInstallState::Observer {
 public:
  ScreenAILibraryProviderImpl(const ScreenAILibraryProviderImpl&) = delete;
  ScreenAILibraryProviderImpl& operator=(const ScreenAILibraryProviderImpl&) =
      delete;

  // Returns the instance that the browser uses. Like the install state, it is
  // never destroyed.
  static ScreenAILibraryProviderImpl* GetInstance();

  // ScreenAILibraryProvider:
  base::FilePath GetBinaryPath() const override;
  void RecordUsage() override;
  base::CallbackListSubscription EnsureLibraryAvailable(
      base::OnceCallback<void(bool)> callback) override;

 private:
  friend class base::NoDestructor<ScreenAILibraryProviderImpl>;
  friend class ScreenAILibraryProviderImplTest;

  using PendingCallbacks = base::OnceCallbackList<void(bool)>;

  // The browser uses `GetInstance()`. Only tests create other instances.
  ScreenAILibraryProviderImpl();
  ~ScreenAILibraryProviderImpl() override;

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;
  void OnScreenAIInstallStateDestroying() override;

  // Returns an empty list of callbacks that tells this object when callbacks
  // are removed from it.
  std::unique_ptr<PendingCallbacks> CreatePendingCallbacks();

  // Starts the download of the library for the pending callbacks, and observes
  // the install state to know when it ends.
  void StartDownload();

  // Does the same for the requests that were made while the pending callbacks
  // were run, if they still wait.
  void StartDeferredDownload();

  // Stops observing the install state and runs the pending callbacks.
  void RunPendingCallbacks(bool available);

  // Called when callbacks are removed from `pending_callbacks_`.
  void OnPendingCallbacksRemoved();

  // The callbacks of `EnsureLibraryAvailable()` that wait for the download.
  // Never null.
  std::unique_ptr<PendingCallbacks> pending_callbacks_;

  // True while `RunPendingCallbacks()` runs the callbacks. Most of the time
  // the install state is telling its observers about a change then, and its
  // state must not be changed again until it is done.
  bool running_pending_callbacks_ = false;

  // Observes the install state while callbacks are pending.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      install_state_observation_{this};

  base::WeakPtrFactory<ScreenAILibraryProviderImpl> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_SCREEN_AI_SCREEN_AI_LIBRARY_PROVIDER_IMPL_H_
