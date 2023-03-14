// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_

#include <array>
#include <memory>
#include <string>

#include "ash/components/arc/session/arc_client_adapter.h"
#include "ash/components/arc/session/file_system_status.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
struct SystemMemoryInfoKB;
class TimeDelta;
}  // namespace base

namespace arc {

struct UpgradeParams;

// Enum that describes which native bridge mode is used to run arm binaries on
// x86.
enum class ArcBinaryTranslationType {
  NONE,
  HOUDINI,
  NDK_TRANSLATION,
};

// The maximum size of VM memory when crosvm is a 32-bit process.
//
// A 32-bit process has 4GB address space, and some parts are not usable for
// various reasons including address space layout randomization (ASLR).
// In 32-bit crosvm address space, only ~3370MB is usable:
// - 256MB is not usable because of executable load bias ASLR.
// - 4MB is used for crosvm executable.
// - 32MB it not usable because of heap ASLR.
// - 16MB is used for mapped shared libraries.
// - 256MB is not usable because of mmap base address ASLR.
// - 132MB is used for gaps in the memory layout.
// - 30MB is used for other allocations.
//
// 3328 is chosen because it's a rounded number (i.e. 3328 % 256 == 0).
constexpr size_t k32bitVmRamMaxMib = 3328;

// Names of Upstart jobs that are managed in the ARCVM boot sequence.
// The "_2d" in job names below corresponds to "-". Upstart escapes characters
// that aren't valid in D-Bus object paths with underscore followed by its
// ascii code in hex. So "arc_2dcreate_2ddata" becomes "arc-create-data".
constexpr char kArcVmDataMigratorJobName[] = "arcvm_2ddata_2dmigrator";
constexpr char kArcVmMediaSharingServicesJobName[] =
    "arcvm_2dmedia_2dsharing_2dservices";
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr char kArcVmPreLoginServicesJobName[] =
    "arcvm_2dpre_2dlogin_2dservices";
constexpr char kArcVmPostLoginServicesJobName[] =
    "arcvm_2dpost_2dlogin_2dservices";
constexpr char kArcVmPostVmStartServicesJobName[] =
    "arcvm_2dpost_2dvm_2dstart_2dservices";

// List of Upstart jobs that can outlive ARC sessions (e.g. after Chrome crash,
// Chrome restart on a feature flag change) and thus should be stopped at the
// beginning of the ARCVM boot sequence.
constexpr std::array<const char*, 5> kArcVmUpstartJobsToBeStoppedOnRestart = {
    kArcVmDataMigratorJobName,         kArcVmPreLoginServicesJobName,
    kArcVmPostLoginServicesJobName,    kArcVmPostVmStartServicesJobName,
    kArcVmMediaSharingServicesJobName,
};

// For better unit-testing.
class ArcVmClientAdapterDelegate {
 public:
  ArcVmClientAdapterDelegate() = default;
  ArcVmClientAdapterDelegate(const ArcVmClientAdapterDelegate&) = delete;
  ArcVmClientAdapterDelegate& operator=(const ArcVmClientAdapterDelegate&) =
      delete;
  virtual ~ArcVmClientAdapterDelegate() = default;
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info);

  // Returns if crosvm is a 32-bit process.
  virtual bool IsCrosvm32bit();
};

// Returns an adapter for arcvm.
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter();

using FileSystemStatusRewriter =
    base::RepeatingCallback<void(FileSystemStatus*)>;
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter);

// Sets the path of the boot notification server socket for testing.
void SetArcVmBootNotificationServerAddressForTesting(
    const std::string& path,
    base::TimeDelta connect_timeout_limit,
    base::TimeDelta connect_sleep_duration_initial);

// Sets the an FD ConnectToArcVmBootNotificationServer() returns for testing.
void SetArcVmBootNotificationServerFdForTesting(absl::optional<int> fd);

// Generates a list of props from |upgrade_params|, each of which takes the form
// "prefix.prop_name=value"
std::vector<std::string> GenerateUpgradePropsForTesting(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix);

void SetArcVmClientAdapterDelegateForTesting(
    ArcClientAdapter* adapter,
    std::unique_ptr<ArcVmClientAdapterDelegate> delegate);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
