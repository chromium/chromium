// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_LINUX)
#include "base/cpu.h"
#include "base/files/file_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/native_library.h"
#endif

namespace {
const int kScreenAICleanUpDelayInDays = 30;
const char kMinExpectedVersion[] = "121.1";

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LibraryVerificationResult {
  kOk = 0,
  kVersionInvalid = 1,
  kVersionLow = 2,
  kPathUnexpected = 3,
  kLoadFailed = 4,
  kMaxValue = kLoadFailed,
};

void RecordLibraryVerificationResult(LibraryVerificationResult result) {
  base::UmaHistogramEnumeration(
      "Accessibility.ScreenAI.LibraryVerificationResult", result);
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
    RecordLibraryVerificationResult(LibraryVerificationResult::kVersionInvalid);
    return false;
  }

  if (version < min_version) {
    VLOG(0) << "Version is expected to be at least " << kMinExpectedVersion
            << ", but it is: " << version;
    RecordLibraryVerificationResult(LibraryVerificationResult::kVersionLow);
    return false;
  }

  return true;
}

// static
bool ScreenAIInstallState::VerifyLibraryAvailablity(
    const base::FilePath& install_dir) {
  // Check the file iterator heuristic to find the library in the sandbox
  // returns the same directory as `install_dir`.
  base::FilePath binary_path = screen_ai::GetLatestComponentBinaryPath();
  if (binary_path.DirName() != install_dir) {
    RecordLibraryVerificationResult(LibraryVerificationResult::kPathUnexpected);
    VLOG(0) << "Library is installed in an unexpected folder.";
    return false;
  }

#if !BUILDFLAG(IS_WIN)
  RecordLibraryVerificationResult(LibraryVerificationResult::kOk);
  return true;
#else
  // Sometimes the library cannot be loaded due to an installation error or OS
  // limitations.
  base::NativeLibraryLoadError lib_error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(binary_path, &lib_error);
  bool available = (library != nullptr);
  base::UmaHistogramSparse("Accessibility.ScreenAI.LibraryAccessResultOnVerify",
                           lib_error.code);
  if (available) {
    base::UnloadNativeLibrary(library);
    RecordLibraryVerificationResult(LibraryVerificationResult::kOk);
  } else {
    RecordLibraryVerificationResult(LibraryVerificationResult::kLoadFailed);
    VLOG(0) << "Library could not be loaded.";
  }

  return available;
#endif
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

// static
void ScreenAIInstallState::RecordComponentInstallationResult(bool install,
                                                             bool successful) {
  if (install) {
    base::UmaHistogramBoolean("Accessibility.ScreenAI.Component.Install",
                              successful);
  } else {
    base::UmaHistogramBoolean("Accessibility.ScreenAI.Component.Uninstall",
                              successful);
  }
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
  // TODO(crbug.com/1508404): Remove after crash root cause is found.
  if ((state == State::kDownloaded || state == State::kReady) &&
      !IsComponentAvailable()) {
    base::debug::Alias(&state);
    base::debug::DumpWithoutCrashing();
    state = State::kFailed;
  }

  if (state == state_) {
    // Failed and ready state can be repeated as they come from different
    // profiles. Downloading can be repeated in ChromeOS tests that call
    // LoginManagerTest::AddUser() and reset UserSessionInitializer.
    // TODO(crbug.com/1443341): While the case is highly unexpected, add more
    // control logic if state is changed from failed to ready or vice versa.
    DCHECK(state == State::kReady || state == State::kFailed ||
           state == State::kDownloading);
    return;
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

void ScreenAIInstallState::SetComponentFolderForTesting() {
  CHECK_IS_TEST();
#if BUILDFLAG(IS_LINUX)
  // Set the path to the ScreenAI test files. For more details, see the
  // `screen_ai_test_files` rule in the accessibility_common BUILD file.
  base::FilePath screenai_library_path =
      screen_ai::GetLatestComponentBinaryPath();
  CHECK(base::PathExists(screenai_library_path));
  SetComponentFolder(screenai_library_path.DirName());
#endif  // BUILDFLAG(IS_LINUX)
}

void ScreenAIInstallState::SetStateForTesting(State state) {
  state_ = state;
  for (ScreenAIInstallState::Observer* observer : observers_) {
    observer->StateChanged(state_);
  }
}

}  // namespace screen_ai
