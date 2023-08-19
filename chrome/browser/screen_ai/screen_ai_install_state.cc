// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include <memory>

#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_LINUX)
#include "base/cpu.h"
#endif

namespace {
const int kScreenAICleanUpDelayInDays = 30;
const char kMinExpectedVersion[] = "114.0";

bool IsDeviceCompatible() {
  // Check if the CPU has the required instruction set to run the Screen AI
  // library.
#if BUILDFLAG(IS_LINUX)
  if (!base::CPU().has_sse41()) {
    return false;
  }
#endif
  return true;
}

}  // namespace

namespace screen_ai {

// ScreenAIInstallState is created through ScreenAIDownloader and we expect on
// and only one of it exists during browser's life time.
ScreenAIInstallState* g_instance = nullptr;

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  return g_instance;
}

// static
bool ScreenAIInstallState::VerifyLibraryVersion(const std::string& version) {
  if (version >= kMinExpectedVersion) {
    return true;
  }
  VLOG(0) << "Screen AI library version is expected to be at least "
          << kMinExpectedVersion << ", but it is: " << version;
  return false;
}

ScreenAIInstallState::ScreenAIInstallState() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

ScreenAIInstallState::~ScreenAIInstallState() {
  CHECK_NE(g_instance, nullptr);
  g_instance = nullptr;
}

// static
bool ScreenAIInstallState::ShouldInstall(PrefService* local_state) {
  if (!IsDeviceCompatible()) {
    return false;
  }

  base::Time last_used_time =
      local_state->GetTime(prefs::kScreenAILastUsedTimePrefName);

  if (last_used_time.is_null()) {
    return false;
  }

  if (base::Time::Now() >=
      last_used_time + base::Days(kScreenAICleanUpDelayInDays)) {
    local_state->ClearPref(prefs::kScreenAILastUsedTimePrefName);
    return false;
  }

  return true;
}

void ScreenAIInstallState::AddObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.push_back(observer);
  observer->StateChanged(state_);

  // Adding an observer indicates that we need the component.
  SetLastUsageTime();
  DownloadComponent();
}

void ScreenAIInstallState::DownloadComponent() {
  if (MayTryDownload()) {
    DownloadComponentInternal();
  }
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = base::ranges::find(observers_, observer);
  if (pos != observers_.end()) {
    observers_.erase(pos);
  }
}

void ScreenAIInstallState::SetComponentFolder(
    const base::FilePath& component_folder) {
  component_binary_path_ =
      component_folder.Append(GetComponentBinaryFileName());

  // A new component may be downloaded when an older version already exists and
  // is ready to use. We don't need to set the state again and call the
  // observers to tell this. If the older component is already in use, current
  // session will continue using that and the new one will be used after next
  // Chrome restart. Otherwise the new component will be used when a service
  // request arrives as its path is stored in |component_binary_path_|.
  if (state_ != State::kReady && state_ != State::kDownloaded) {
    SetState(State::kDownloaded);
  }
}

void ScreenAIInstallState::SetState(State state) {
  if (state == state_) {
    // Failed and ready state can be repeated as they come from different
    // profiles. Downloading can be repeated in ChromeOS tests that call
    // LoginManagerTest::AddUser() and reset UserSessionInitializer.
    // TODO(crbug.com/1278249): While the case is highly unexpected, add more
    // control logic if state is changed from failed to ready or vice versa.
    DCHECK(state == State::kReady || state == State::kFailed ||
           state == State::kDownloading);
    return;
  }

  // Switching state from `Ready` to `Fail` is unexpected and requires
  // investigation.
  // TODO(crbug.com/1443345): Remove after verifying this case does not happen.
  if (state == State::kFailed && state_ == State::kReady) {
    base::debug::DumpWithoutCrashing();
  }

  state_ = state;
  for (ScreenAIInstallState::Observer* observer : observers_) {
    observer->StateChanged(state_);
  }
}

void ScreenAIInstallState::SetDownloadProgress(double progress) {
  DCHECK_EQ(state_, State::kDownloading);
  for (ScreenAIInstallState::Observer* observer : observers_) {
    observer->DownloadProgressChanged(progress);
  }
}

bool ScreenAIInstallState::IsComponentAvailable() {
  return !get_component_binary_path().empty();
}

void ScreenAIInstallState::SetComponentReadyForTesting() {
  state_ = State::kReady;
}

bool ScreenAIInstallState::MayTryDownload() {
  switch (state_) {
    case State::kNotDownloaded:
    case State::kFailed:
      return true;

    case State::kDownloading:
    case State::kDownloaded:
    case State::kReady:
      return false;
  }
}

void ScreenAIInstallState::ResetForTesting() {
  state_ = State::kNotDownloaded;
  component_binary_path_.clear();
}

void ScreenAIInstallState::SetStateForTesting(State state) {
  state_ = state;
}

}  // namespace screen_ai
