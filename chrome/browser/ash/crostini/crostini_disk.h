// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_DISK_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_DISK_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace crostini {

struct CrostiniDiskInfo {
  CrostiniDiskInfo();
  CrostiniDiskInfo(CrostiniDiskInfo&&);
  CrostiniDiskInfo(const CrostiniDiskInfo&) = delete;
  CrostiniDiskInfo& operator=(CrostiniDiskInfo&&);
  CrostiniDiskInfo& operator=(const CrostiniDiskInfo&) = delete;
  ~CrostiniDiskInfo();
  bool can_resize{};
  bool is_user_chosen_size{};
  bool is_low_space_available{};
  int default_index{};
  std::vector<crostini::mojom::DiskSliderTickPtr> ticks;
};

namespace disk {

constexpr int64_t kGiB = 1024 * 1024 * 1024;
constexpr int64_t kDiskHeadroomBytes = 1 * kGiB;
constexpr int64_t kMinimumDiskSizeBytes = 2 * kGiB;
constexpr int64_t kRecommendedDiskSizeBytes = 10 * kGiB;

// A number which influences the interval size and number of ticks selected for
// a given range. At 400 >400 GiB gets 1 GiB ticks, smaller sizes get smaller
// intervals. 400 is arbitrary, chosen because it keeps ticks at least 1px each
// on sliders and feels nice.
constexpr int kGranularityFactor = 400;

// The size of the download for the VM image.
// As of 2020-01-10 the Termina files.zip is ~90MiB and the squashfs container
// is ~330MiB.
constexpr int64_t kDownloadSizeBytes = 450ll * 1024 * 1024;  // 450 MiB

using OnceDiskInfoCallback =
    base::OnceCallback<void(std::unique_ptr<CrostiniDiskInfo> info)>;

// Constructs a CrostiniDiskInfo for the requested vm under the given profile
// then calls callback with it once done. |full_info| requests extra disk info
// that is only available from a running VM.
void GetDiskInfo(OnceDiskInfoCallback callback,
                 Profile* profile,
                 std::string vm_name,
                 bool full_info);

// Callback for OnAmountOfFreeDiskSpace which passes off to the next step in the
// chain. Not intended to be called directly unless you're crostini_disk or
// tests.
void OnAmountOfFreeDiskSpace(OnceDiskInfoCallback callback,
                             Profile* profile,
                             std::string vm_name,
                             std::optional<int64_t> free_space);

// Combined callback for EnsureConciergeRunning or EnsureVmRunning which passes
// off to the next step in the chain. For getting full disk info, the VM must be
// running. Otherwise it is sufficient for Concierge to be running, but not
// necessarily the VM.Not intended to be called directly unless you're
// crostini_disk or tests.
void OnCrostiniSufficientlyRunning(OnceDiskInfoCallback callback,
                                   Profile* profile,
                                   std::string vm_name,
                                   int64_t free_space,
                                   CrostiniResult result);

// Callback for EnsureVmRunning which passes off to the next step in the chain.
// Not intended to be called directly unless you're crostini_disk or tests.
void OnVMRunning(base::OnceCallback<void(bool)> callback,
                 Profile* profile,
                 std::string vm_name,
                 int64_t free_space,
                 CrostiniResult result);

// Callback for OnListVmDisks which passes off to the next step in the chain.
// Not intended to be called directly unless you're crostini_disk or tests.
void OnListVmDisks(
    OnceDiskInfoCallback callback,
    std::string vm_name,
    int64_t free_space,
    std::optional<vm_tools::concierge::ListVmDisksResponse> response);

// Given a minimum, currently selected and maximum value, constructs a range of
// DiskSliderTicks spanning from min to max. Ensures that one of the ticks
// matches the current value and will write the index of that value to
// out_default_index.
std::vector<crostini::mojom::DiskSliderTickPtr>
GetTicks(int64_t min, int64_t current, int64_t max, int* out_default_index);

// Requests the disk for |vm_name| to be resized to |size_bytes|.
// Once complete |callback| is called with true (succeeded resizing) or false
// for any error.
void ResizeCrostiniDisk(Profile* profile,
                        std::string vm_name,
                        uint64_t size_bytes,
                        base::OnceCallback<void(bool)> callback);

// Callback provided to Concierge, not intended to be called unless you're
// crostini_disk or tests.
void OnResize(
    base::OnceCallback<void(bool)> callback,
    std::optional<vm_tools::concierge::ResizeDiskImageResponse> response);

// Splits the range between |min_size| and |available_space| into enough
// evenly-spaced intervals you can use them as ticks on a slider. Will return an
// empty set if the range is invalid (e.g. any numbers are negative).
std::vector<int64_t> GetTicksForDiskSize(
    int64_t min_size,
    int64_t available_space,
    int granularity_factor_for_testing = kGranularityFactor);

}  // namespace disk
}  // namespace crostini
#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_DISK_H_
