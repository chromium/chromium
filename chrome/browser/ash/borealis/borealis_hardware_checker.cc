// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_hardware_checker.h"

#include "ash/constants/ash_features.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "third_party/re2/src/re2/re2.h"

namespace borealis {

namespace {

constexpr uint64_t kGibi = 1024ull * 1024 * 1024;

// Regex used for CPU checks on intel processors, this means "any i{3,5,7}
// processor". e.g.:
//  - Valid:   11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz
//  - Valid:   Intel(R) Core(TM) 5 ...
//  - Invalid: Intel(R) Pentium(R) Gold 7505
constexpr char kIntelCpuRegex[] = "((i[357]-)|(Core.* [357]))";

// As above, for AMD processors, e.g. "AMD Ryzen 3 5125C with Radeon Graphics".
constexpr char kAmdCpuRegex[] = "Ryzen [357]";

std::string* g_cpu_brand_for_test_ = nullptr;

// Returns the Board's name according to /etc/lsb-release. Strips any variant
// except the "-borealis" variant.
//
// Note: the comment on GetLsbReleaseBoard() (rightly) points out that we're
// not supposed to use LsbReleaseBoard directly, but rather set a flag in
// the overlay. We are not doing that as the following check is only a
// temporary hack necessary while we release borealis, but will be removed
// shortly afterwards. This check can fail in either direction and we won't
// be too upset.
std::string GetBoardName() {
  // In a developer build, the name "volteer" or "volteer-borealis" will become
  // "volteer-signed-mp-blahblah" and "volteer-borealis-signed..." on a signed
  // build, so we want to stop everything after the "-" unless its "-borealis".
  //
  // This means a variant like "volteer-kernelnext" will be treated as "volteer"
  // by us.
  std::vector<std::string> pieces =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() >= 2 && pieces[1] == "borealis") {
    return pieces[0] + "-" + pieces[1];
  }
  DCHECK(!pieces.empty());
  return pieces[0];
}

bool IsBoard(const std::string& board) {
  return GetBoardName() == board;
}

bool BoardIn(const base::flat_set<std::string>& boards) {
  return boards.contains(GetBoardName());
}

bool CpuRegexMatches(const std::string& cpu_regex) {
  return RE2::PartialMatch(
      g_cpu_brand_for_test_ ? *g_cpu_brand_for_test_
                            : base::CPU::GetInstanceNoAllocation().cpu_brand(),
      cpu_regex);
}

bool HasMemory(uint64_t mem_bytes) {
  return base::SysInfo::AmountOfPhysicalMemory() >= mem_bytes;
}

bool HasSufficientHardware(const std::string& cpu_regex) {
  return HasMemory(7 * kGibi) && CpuRegexMatches(cpu_regex);
}

bool InTargetSegment() {
  return base::FeatureList::IsEnabled(
      ash::features::kFeatureManagementBorealis);
}

bool Check() {
  if (BoardIn({"hatch", "drallion", "puff"})) {
    return HasSufficientHardware(kIntelCpuRegex);
  }
  if (IsBoard("volteer")) {
    return HasSufficientHardware(kIntelCpuRegex);
  }
  if (BoardIn({"brya", "adlrvp", "brask", "brox"})) {
    return HasSufficientHardware(kIntelCpuRegex);
  }
  if (BoardIn({"guybrush", "majolica"})) {
    return HasSufficientHardware(kAmdCpuRegex);
  }
  if (BoardIn({"aurora", "myst"})) {
    return true;
  }
  if (IsBoard("nissa")) {
    return HasSufficientHardware(kIntelCpuRegex) && InTargetSegment();
  }
  if (IsBoard("skyrim")) {
    return HasSufficientHardware(kAmdCpuRegex) && InTargetSegment();
  }
  if (IsBoard("rex")) {
    // TODO(307825451): .* allows any CPU, add the correct cpu regex once we
    // know what that is.
    return HasSufficientHardware(".*");
  }
  return false;
}

}  // namespace

void HasSufficientHardware(base::OnceCallback<void(bool)> callback) {
  ash::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
          [](base::OnceCallback<void(bool)> callback) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, base::MayBlock(), base::BindOnce(&Check),
                std::move(callback));
          },
          std::move(callback)));
}

void SetCpuForTesting(std::string* cpu_brand) {
  g_cpu_brand_for_test_ = cpu_brand;
}

}  // namespace borealis
