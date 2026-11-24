// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_library_provider_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"

namespace screen_ai {

ScreenAILibraryProviderImpl::ScreenAILibraryProviderImpl()
    : pending_callbacks_(CreatePendingCallbacks()) {}

ScreenAILibraryProviderImpl::~ScreenAILibraryProviderImpl() = default;

// static
ScreenAILibraryProviderImpl* ScreenAILibraryProviderImpl::GetInstance() {
  static base::NoDestructor<ScreenAILibraryProviderImpl> instance;
  return instance.get();
}

base::FilePath ScreenAILibraryProviderImpl::GetBinaryPath() const {
  return ScreenAIInstallState::GetInstance()->get_component_binary_path();
}

void ScreenAILibraryProviderImpl::RecordUsage() {
  ScreenAIInstallState::GetInstance()->SetLastUsageTime();
}

base::CallbackListSubscription
ScreenAILibraryProviderImpl::EnsureLibraryAvailable(
    base::OnceCallback<void(bool)> callback) {
  if (ScreenAIInstallState::GetInstance()->get_state() ==
      ScreenAIInstallState::State::kDownloaded) {
    std::move(callback).Run(true);
    return {};
  }

  base::CallbackListSubscription subscription =
      pending_callbacks_->Add(std::move(callback));

  if (running_pending_callbacks_) {
    // This request comes from a callback that was just told how a download
    // ended. The install state may be telling its observers about that right
    // now, and its state must not be changed until it is done. So the download
    // for this request starts later.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ScreenAILibraryProviderImpl::StartDeferredDownload,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartDownload();
  }

  return subscription;
}

void ScreenAILibraryProviderImpl::StateChanged(
    ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
    case ScreenAIInstallState::State::kDownloading:
      return;

    case ScreenAIInstallState::State::kDownloadFailed:
      RunPendingCallbacks(false);
      return;

    case ScreenAIInstallState::State::kDownloaded:
      RunPendingCallbacks(true);
      return;
  }
}

void ScreenAILibraryProviderImpl::OnScreenAIInstallStateDestroying() {
  // The install state forgets its observers, so it does not tell when the
  // library is downloaded.
  RunPendingCallbacks(false);
}

std::unique_ptr<ScreenAILibraryProviderImpl::PendingCallbacks>
ScreenAILibraryProviderImpl::CreatePendingCallbacks() {
  auto callbacks = std::make_unique<PendingCallbacks>();
  // `base::Unretained` is safe, as this object owns the list.
  callbacks->set_removal_callback(base::BindRepeating(
      &ScreenAILibraryProviderImpl::OnPendingCallbacksRemoved,
      base::Unretained(this)));
  return callbacks;
}

void ScreenAILibraryProviderImpl::StartDownload() {
  auto* install_state = ScreenAIInstallState::GetInstance();

  // If download has previously failed, reset it.
  if (install_state->get_state() ==
      ScreenAIInstallState::State::kDownloadFailed) {
    install_state->SetState(ScreenAIInstallState::State::kNotDownloaded);
  }

  // Observe component state if not already observed, otherwise trigger
  // download. (Adding observer also triggers download.)
  if (!install_state_observation_.IsObserving()) {
    install_state_observation_.Observe(install_state);
  } else {
    install_state->DownloadComponent();
  }
}

void ScreenAILibraryProviderImpl::StartDeferredDownload() {
  // The requests may have been canceled or answered in the meantime.
  if (pending_callbacks_->empty()) {
    return;
  }

  // Something else may have downloaded the library in the meantime.
  if (ScreenAIInstallState::GetInstance()->get_state() ==
      ScreenAIInstallState::State::kDownloaded) {
    RunPendingCallbacks(true);
    return;
  }

  StartDownload();
}

void ScreenAILibraryProviderImpl::RunPendingCallbacks(bool available) {
  // No need to observe after library is downloaded or download has failed.
  install_state_observation_.Reset();

  // A callback can ask for the library again, for example to try once more
  // after a failure. Such a request has to wait for a new download, so it must
  // not be added to the list that is told about this one.
  std::unique_ptr<PendingCallbacks> callbacks =
      std::exchange(pending_callbacks_, CreatePendingCallbacks());
  callbacks->set_removal_callback(base::RepeatingClosure());

  base::AutoReset<bool> running(&running_pending_callbacks_, true);
  callbacks->Notify(available);
}

void ScreenAILibraryProviderImpl::OnPendingCallbacksRemoved() {
  // Stop observing the install state when nobody waits for the download
  // anymore. As this object is never destroyed, it could otherwise observe the
  // install state at shutdown, which must not have observers then (see
  // `ScreenAIDownloaderNonChromeOS::OnStateOrObserversChanged()`).
  if (pending_callbacks_->empty()) {
    install_state_observation_.Reset();
  }
}

}  // namespace screen_ai
