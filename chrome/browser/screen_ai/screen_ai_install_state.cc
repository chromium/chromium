// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_LINUX)
#include "base/cpu.h"
#include "base/files/file_util.h"
#endif

namespace {
const int kScreenAICleanUpDelayInDays = 30;
const char kMinExpectedVersion[] = "124.2";

bool IsDeviceCompatible() {
#if BUILDFLAG(IS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
  // Check if the CPU has the required instruction set to run the Screen AI
  // library.
  static const bool has_sse41 = base::CPU().has_sse41();
#else
  static constexpr bool has_sse41 = false;
#endif  // defined(ARCH_CPU_X86_FAMILY)
  if (!has_sse41) {
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX)
  return true;
}

}  // namespace

namespace screen_ai {

// ScreenAIInstallState is created through ScreenAIDownloader and we expect on
// and only one of it exists during browser's life time.
ScreenAIInstallState* g_instance = nullptr;

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  if (g_instance) {
    return g_instance;
  }
  // `!g_instance` only happens in unit tests in which a browser instance is
  // not created. Assert that this code path is only taken in tests.
  CHECK_IS_TEST();
  return ScreenAIInstallState::CreateForTesting();
}

// static
bool ScreenAIInstallState::VerifyLibraryVersion(const base::Version& version) {
  base::Version min_version(kMinExpectedVersion);
  CHECK(min_version.IsValid());

  if (!version.IsValid()) {
    VLOG(0) << "Cannot verify library version.";
    return false;
  }

  if (version < min_version) {
    VLOG(0) << "Version is expected to be at least " << kMinExpectedVersion
            << ", but it is: " << version;
    return false;
  }

  return true;
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
  observers_.AddObserver(observer);
  observer->StateChanged(state_);

  // Adding an observer indicates that we need the component.
  SetLastUsageTime();
  DownloadComponent();
}

void ScreenAIInstallState::DownloadComponent() {
  if (!features::IsScreenAIOCREnabled() &&
      !features::IsScreenAIMainContentExtractionEnabled()) {
    SetState(State::kDownloadFailed);
    return;
  }

  if (MayTryDownload()) {
    DownloadComponentInternal();
  }
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.RemoveObserver(observer);
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
  if (state_ != State::kDownloaded) {
    SetState(State::kDownloaded);
  }
}

void ScreenAIInstallState::SetState(State state) {
  if (state == state_) {
    // `kDownloadFailed` state can be repeated as download can be retriggered.
    // `kDownloading` can be repeated in ChromeOS tests that call
    // LoginManagerTest::AddUser() and reset UserSessionInitializer.
    DCHECK(state == State::kDownloadFailed || state == State::kDownloading);
    return;
  }

  state_ = state;
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.StateChanged(state_);
  }
}

void ScreenAIInstallState::SetDownloadProgress(double progress) {
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.DownloadProgressChanged(progress);
  }
}

bool ScreenAIInstallState::IsComponentAvailable() {
  return !get_component_binary_path().empty();
}

bool ScreenAIInstallState::MayTryDownload() {
  switch (state_) {
    case State::kNotDownloaded:
    case State::kDownloadFailed:
      return true;

    case State::kDownloading:
    case State::kDownloaded:
      return false;
  }
}

void ScreenAIInstallState::ResetForTesting() {
  state_ = State::kNotDownloaded;
  component_binary_path_.clear();
}

void ScreenAIInstallState::SetStateForTesting(State state) {
  state_ = state;
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.StateChanged(state_);
  }
}

}  // namespace screen_ai
