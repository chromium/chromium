// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/hwid_checker.h"

#include <cstdio>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/common/content_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/zlib/zlib.h"

namespace {

unsigned CalculateCRC32(const std::string& data) {
  return static_cast<unsigned>(
      crc32(0, reinterpret_cast<const Bytef*>(data.c_str()), data.length()));
}

std::string CalculateHWIDv2Checksum(const std::string& data) {
  unsigned crc32 = CalculateCRC32(data);
  // We take four least significant decimal digits of CRC-32.
  char checksum[5];
  int snprintf_result = snprintf(checksum, 5, "%04u", crc32 % 10000);
  LOG_ASSERT(snprintf_result == 4);
  return checksum;
}

bool IsCorrectHWIDv2(const std::string& hwid) {
  std::string body;
  std::string checksum;
  if (!RE2::FullMatch(hwid, "([\\s\\S]*) (\\d{4})", &body, &checksum))
    return false;
  return CalculateHWIDv2Checksum(body) == checksum;
}

bool IsExceptionalHWID(const std::string& hwid) {
  return RE2::PartialMatch(hwid, "^(SPRING [A-D])|(FALCO A)");
}

std::string CalculateExceptionalHWIDChecksum(const std::string& data) {
  static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  unsigned crc32 = CalculateCRC32(data);
  // We take 10 least significant bits of CRC-32 and encode them in 2 characters
  // using Base32 alphabet.
  std::string checksum;
  checksum += base32_alphabet[(crc32 >> 5) & 0x1f];
  checksum += base32_alphabet[crc32 & 0x1f];
  return checksum;
}

bool IsCorrectExceptionalHWID(const std::string& hwid) {
  if (!IsExceptionalHWID(hwid))
    return false;
  std::string bom;
  if (!RE2::FullMatch(hwid, "[A-Z0-9]+ ((?:[A-Z2-7]{4}-)*[A-Z2-7]{1,4})", &bom))
    return false;
  if (bom.length() < 2)
    return false;
  std::string hwid_without_dashes;
  base::RemoveChars(hwid, "-", &hwid_without_dashes);
  LOG_ASSERT(hwid_without_dashes.length() >= 2);
  std::string not_checksum =
      hwid_without_dashes.substr(0, hwid_without_dashes.length() - 2);
  std::string checksum =
      hwid_without_dashes.substr(hwid_without_dashes.length() - 2);
  return CalculateExceptionalHWIDChecksum(not_checksum) == checksum;
}

std::string CalculateHWIDv3Checksum(const std::string& data) {
  static const char base8_alphabet[] = "23456789";
  static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  unsigned crc32 = CalculateCRC32(data);
  // We take 8 least significant bits of CRC-32 and encode them in 2 characters.
  std::string checksum;
  checksum += base8_alphabet[(crc32 >> 5) & 0x7];
  checksum += base32_alphabet[crc32 & 0x1f];
  return checksum;
}

bool IsCorrectHWIDv3(const std::string& hwid) {
  if (IsExceptionalHWID(hwid))
    return false;

  // HWIDv3 format:
  //   <MODEL>[-<RLZ>] [CONFIGLESS] <COMPONENT><CHECKSUM>
  // Fields in [] are optional.
  std::vector<std::string> parts =
      base::SplitString(hwid, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_ALL);
  std::string not_checksum, checksum;

  // <MODEL> or <MODEL>-<RLZ>
  constexpr char model[] = "[-A-Z0-9]+";
  // Configless field is composed by dash "-" separated hex numbers.
  constexpr char configless_field[] = "(?:[[:xdigit:]]+-)+[[:xdigit:]]+";
  constexpr char component_and_checksum[] =
      "(?:[A-Z2-7][2-9][A-Z2-7]-)*[A-Z2-7][2-9][A-Z2-7]";

  int component_field_index;

  if (parts.size() == 2) {
    // <MODEL>[-RLZ] <COMPONENT><CHECKSUM>
    if (!RE2::FullMatch(parts[0], model) ||
        !RE2::FullMatch(parts[1], component_and_checksum)) {
      return false;
    }
    component_field_index = 1;
  } else if (parts.size() == 3) {
    // <MODEL>-<RLZ> <CONFIGLESS> <COMPONENT><CHECKSUM>
    if (!RE2::FullMatch(parts[0], model) ||
        !RE2::FullMatch(parts[1], configless_field) ||
        !RE2::FullMatch(parts[2], component_and_checksum)) {
      return false;
    }
    component_field_index = 2;
  } else {
    return false;
  }

  // Modify component_field before computing checksum.
  std::string& component_field = parts[component_field_index];
  // Last 2 characters are checksum.
  checksum = component_field.substr(component_field.size() - 2);
  component_field = component_field.substr(0, component_field.size() - 2);
  // When computing checksum, "-" is removed from component field.
  base::RemoveChars(component_field, "-", &component_field);

  // Construct not_checksum to compute checksum.
  not_checksum = base::JoinString(parts, " ");
  return CalculateHWIDv3Checksum(not_checksum) == checksum;
}

}  // anonymous namespace

namespace chromeos {

bool IsHWIDCorrect(const std::string& hwid) {
  return IsCorrectHWIDv2(hwid) || IsCorrectExceptionalHWID(hwid) ||
         IsCorrectHWIDv3(hwid);
}

bool IsMachineHWIDCorrect() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#endif
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(::switches::kTestType))
    return true;
  if (!base::SysInfo::IsRunningOnChromeOS())
    return true;

  chromeos::system::StatisticsProvider* stats =
      chromeos::system::StatisticsProvider::GetInstance();
  if (stats->IsRunningOnVm())
    return true;

  std::string hwid;
  if (!stats->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid)) {
    LOG(ERROR) << "Couldn't get machine statistic 'hardware_class'.";
    return false;
  }
  if (!chromeos::IsHWIDCorrect(hwid)) {
    LOG(ERROR) << "Machine has malformed HWID '" << hwid << "'. ";
    return false;
  }
  return true;
}

}  // namespace chromeos
