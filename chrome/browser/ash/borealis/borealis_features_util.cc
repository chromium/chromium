// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_features_util.h"

#include "base/base64.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "crypto/sha2.h"
#include "third_party/re2/src/re2/re2.h"

namespace borealis {

namespace {

// Returns the Board's name according to /etc/lsb-release. Strips any variant
// except the "-borealis" variant.
//
// Note: the comment on GetLsbReleaseBoard() (rightly) points out that we're
// not supposed to use LsbReleaseBoard directly, but rather set a flag in
// the overlay. I am not doing that as the following check is only a
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

// Returns the model name of this device (either from its CustomizationId or by
// parsing its hardware class). Returns "" if it fails.
std::string GetModelName() {
  if (const std::optional<base::StringPiece> ret =
          ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
              ash::system::kCustomizationIdKey)) {
    return std::string(ret.value());
  }
  LOG(WARNING)
      << "CustomizationId unavailable, attempting to parse hardware class";

  // As a fallback when the CustomizationId is not available, we try to parse it
  // out of the hardware class. If The hardware class is unavailable, all bets
  // are off.
  const std::optional<base::StringPiece> hardware_class_statistic =
      ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          ash::system::kHardwareClassKey);
  if (!hardware_class_statistic) {
    return "";
  }

  // Hardware classes for the "modelname" model might look like this:
  //
  //    MODELNAME-FFFF DEAD-BEEF-HEX-JUNK
  //
  // (or "unknown" if we can't find it). So we only care about converting the
  // stuff before the first "-" into lowercase.
  //
  // Naively searching for the first hyphen is fine until we start caring about
  // models with hyphens in the name.
  base::StringPiece hardware_class = hardware_class_statistic.value();
  size_t hyphen_pos = hardware_class.find('-');
  if (hyphen_pos != std::string::npos) {
    hardware_class = hardware_class.substr(0, hyphen_pos);
  }
  return base::ToLowerASCII(hardware_class);
}

std::string RemoveNonBorealisSuffix(const std::string& board) {
  if (board.ends_with("-borealis")) {
    return board;
  }
  size_t last_hyphen_pos = board.rfind('-');
  if (last_hyphen_pos == std::string::npos) {
    return board;
  }
  return board.substr(0, last_hyphen_pos);
}

}  // namespace

TokenHardwareChecker::Data::Data(std::string board,
                                 std::string model,
                                 std::string cpu,
                                 uint64_t memory)
    : board(std::move(board)),
      model(std::move(model)),
      cpu(std::move(cpu)),
      memory(memory) {}

TokenHardwareChecker::Data::Data(const Data& other) = default;

TokenHardwareChecker::Data::~Data() = default;

void TokenHardwareChecker::GetData(base::OnceCallback<void(Data)> callback) {
  ash::system::StatisticsProvider::GetInstance()
      ->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
          [](base::OnceCallback<void(Data)> callback) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, base::MayBlock(), base::BindOnce([]() -> Data {
                  return Data(GetBoardName(), GetModelName(),
                              base::CPU::GetInstanceNoAllocation().cpu_brand(),
                              base::SysInfo::AmountOfPhysicalMemory());
                }),
                std::move(callback));
          },
          std::move(callback)));
}

TokenHardwareChecker::TokenHardwareChecker(
    TokenHardwareChecker::Data token_hardware)
    : token_hardware_(std::move(token_hardware)) {}

bool TokenHardwareChecker::IsBoard(const std::string& board) const {
  return RemoveNonBorealisSuffix(token_hardware_.board) == board;
}

bool TokenHardwareChecker::BoardIn(base::flat_set<std::string> boards) const {
  return boards.contains(RemoveNonBorealisSuffix(token_hardware_.board));
}

bool TokenHardwareChecker::IsModel(const std::string& model) const {
  return token_hardware_.model == model;
}

bool TokenHardwareChecker::ModelIn(base::flat_set<std::string> models) const {
  return models.contains(token_hardware_.model);
}

bool TokenHardwareChecker::CpuRegexMatches(const std::string& cpu_regex) const {
  return RE2::PartialMatch(token_hardware_.cpu, cpu_regex);
}

bool TokenHardwareChecker::HasMemory(uint64_t mem_bytes) const {
  return token_hardware_.memory >= mem_bytes;
}

}  // namespace borealis
