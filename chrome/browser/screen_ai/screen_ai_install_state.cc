// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include <optional>
#include "base/check_is_test.h"
#include "base/cpu.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
// From 140.3, the proto has the new `whitespace_bounding_box` field.
const char kMinExpectedVersion[] = "140.3";
const int kScreenAICleanUpDelayInDays = 30;

std::optional<bool> g_device_compatible_for_testing;

}  // namespace

namespace screen_ai {

// ScreenAIInstallState is created through ScreenAIDownloader and we expect
// only one instance of it exists during browser's life time.
ScreenAIInstallState* g_instance = nullptr;

// static
bool ScreenAIInstallState::IsDeviceCompatible() {
  if (g_device_compatible_for_testing.has_value()) {
    return *g_device_compatible_for_testing;
  }

#if defined(ARCH_CPU_X86_FAMILY)
  // Check if the CPU has the required instruction set to run the Screen AI
  // library.
  static const bool device_compatible = base::CPU().has_sse42();
#elif BUILDFLAG(IS_LINUX)
  // On Linux, the library is only built for X86 CPUs.
  static constexpr bool device_compatible = false;
#else
  static constexpr bool device_compatible = true;
#endif

  return device_compatible;
}

// static
void ScreenAIInstallState::SetDeviceCompatibleForTesting(bool compatible) {
  CHECK_IS_TEST();
  g_device_compatible_for_testing = compatible;
}

// static
void ScreenAIInstallState::ResetDeviceCompatibilityForTesting() {
  CHECK_IS_TEST();
  g_device_compatible_for_testing.reset();
}

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  if (!g_instance) {
    g_instance = ScreenAIInstallState::Create();
  }
  return g_instance;
}

// static
bool ScreenAIInstallState::VerifyLibraryVersion(const base::Version& version) {
  // As a defense-in-depth guard, reject the library if the device is not
  // compatible, preventing it from being marked ready or loaded.
  if (!IsDeviceCompatible()) {
    VLOG(0) << "Device is not compatible with Screen AI requirements.";
    return false;
  }

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
  if (g_instance) {
    // Only tests may override the existing `base::NoDestructor` singleton with
    // a custom `ScreenAIInstallState` subclass.
    CHECK_IS_TEST();
  }
  previous_instance_ = g_instance;
  g_instance = this;
}

ScreenAIInstallState::~ScreenAIInstallState() {
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.OnScreenAIInstallStateDestroying();
  }
  CHECK_EQ(g_instance, this);
  g_instance = previous_instance_;
  g_device_compatible_for_testing.reset();
}

// static
bool ScreenAIInstallState::ShouldInstall(PrefService* local_state) {
  bool device_compatible = IsDeviceCompatible();
  base::UmaHistogramBoolean("Accessibility.ScreenAI.DeviceCompatible",
                            device_compatible);
  if (!device_compatible) {
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
  OnStateOrObserversChanged();
}

void ScreenAIInstallState::DownloadComponent() {
  if (!MayTryDownload()) {
    return;
  }

  if (!features::IsScreenAIOCREnabled() &&
      !features::IsScreenAIMainContentExtractionEnabled()) {
    SetState(State::kDownloadFailed);
    return;
  }

  // Although verification rejects incompatible binaries post-install, this
  // check avoids initiating an unnecessary network download when a feature
  // triggers an on-demand download request.
  if (!IsDeviceCompatible()) {
    VLOG(0) << "Device is not compatible with Screen AI requirements.";
    SetState(State::kDownloadFailed);
    return;
  }

  DownloadComponentInternal();
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.RemoveObserver(observer);
  OnStateOrObserversChanged();
}

bool ScreenAIInstallState::HasObservers() const {
  return !observers_.empty();
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
  OnStateOrObserversChanged();
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
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.OnScreenAIInstallStateDestroying();
  }
  observers_.Clear();
  state_ = State::kNotDownloaded;
  component_binary_path_.clear();
  ResetDeviceCompatibilityForTesting();
  OnStateOrObserversChanged();
}

void ScreenAIInstallState::SetStateForTesting(State state) {
  state_ = state;
  for (ScreenAIInstallState::Observer& observer : observers_) {
    observer.StateChanged(state_);
  }
  OnStateOrObserversChanged();
}

}  // namespace screen_ai
