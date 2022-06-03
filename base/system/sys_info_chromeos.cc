// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/utsname.h>

#include "base/cxx17_backports.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"

namespace base {

namespace {

const char* const kLinuxStandardBaseVersionKeys[] = {
    "CHROMEOS_RELEASE_VERSION", "GOOGLE_RELEASE", "DISTRIB_RELEASE",
};

const char kChromeOsReleaseNameKey[] = "CHROMEOS_RELEASE_NAME";

const char* const kChromeOsReleaseNames[] = {
    "Chrome OS", "Chromium OS",
};

const char kLinuxStandardBaseReleaseFile[] = "/etc/lsb-release";

const char kLsbReleaseKey[] = "LSB_RELEASE";
const char kLsbReleaseTimeKey[] = "LSB_RELEASE_TIME";  // Seconds since epoch

const char kLsbReleaseSourceKey[] = "lsb-release";
const char kLsbReleaseSourceEnv[] = "env";
const char kLsbReleaseSourceFile[] = "file";

class ChromeOSVersionInfo {
 public:
  ChromeOSVersionInfo() {
    std::string lsb_release, lsb_release_time_str;
    std::unique_ptr<Environment> env(Environment::Create());
    bool parsed_from_env =
        env->GetVar(kLsbReleaseKey, &lsb_release) &&
        env->GetVar(kLsbReleaseTimeKey, &lsb_release_time_str);
    if (parsed_from_env) {
      double us = 0;
      if (StringToDouble(lsb_release_time_str, &us))
        lsb_release_time_ = Time::FromDoubleT(us);
    } else {
      // If the LSB_RELEASE and LSB_RELEASE_TIME environment variables are not
      // set, fall back to a blocking read of the lsb_release file. This should
      // only happen in non Chrome OS environments.
      ThreadRestrictions::ScopedAllowIO allow_io;
      FilePath path(kLinuxStandardBaseReleaseFile);
      ReadFileToString(path, &lsb_release);
      File::Info fileinfo;
      if (GetFileInfo(path, &fileinfo))
        lsb_release_time_ = fileinfo.creation_time;
    }
    ParseLsbRelease(lsb_release);
    // For debugging:
    lsb_release_map_[kLsbReleaseSourceKey] =
        parsed_from_env ? kLsbReleaseSourceEnv : kLsbReleaseSourceFile;
  }

  // The test-only instance should not parse the lsb-release file, because that
  // file exists on the linux test bots, but contains irrelevant values.
  enum ForTest { FOR_TEST };
  explicit ChromeOSVersionInfo(ForTest for_test) {}

  bool GetLsbReleaseValue(const std::string& key, std::string* value) {
    LsbReleaseMap::const_iterator iter = lsb_release_map_.find(key);
    if (iter == lsb_release_map_.end())
      return false;
    *value = iter->second;
    return true;
  }

  void GetVersionNumbers(int32_t* major_version,
                         int32_t* minor_version,
                         int32_t* bugfix_version) {
    *major_version = major_version_;
    *minor_version = minor_version_;
    *bugfix_version = bugfix_version_;
  }

  const Time& lsb_release_time() const { return lsb_release_time_; }
  void set_lsb_release_time(const Time& time) { lsb_release_time_ = time; }

  bool is_running_on_chromeos() const { return is_running_on_chromeos_; }

  void ParseLsbRelease(const std::string& lsb_release) {
    // Parse and cache lsb_release key pairs. There should only be a handful
    // of entries so the overhead for this will be small, and it can be
    // useful for debugging.
    base::StringPairs pairs;
    SplitStringIntoKeyValuePairs(lsb_release, '=', '\n', &pairs);
    for (size_t i = 0; i < pairs.size(); ++i) {
      std::string key, value;
      TrimWhitespaceASCII(pairs[i].first, TRIM_ALL, &key);
      TrimWhitespaceASCII(pairs[i].second, TRIM_ALL, &value);
      if (key.empty())
        continue;
      lsb_release_map_[key] = value;
    }
    // Parse the version from the first matching recognized version key.
    std::string version;
    for (size_t i = 0; i < base::size(kLinuxStandardBaseVersionKeys); ++i) {
      std::string key = kLinuxStandardBaseVersionKeys[i];
      if (GetLsbReleaseValue(key, &version) && !version.empty())
        break;
    }
    StringTokenizer tokenizer(version, ".");
    if (tokenizer.GetNext()) {
      StringToInt(tokenizer.token_piece(), &major_version_);
    }
    if (tokenizer.GetNext()) {
      StringToInt(tokenizer.token_piece(), &minor_version_);
    }
    if (tokenizer.GetNext()) {
      StringToInt(tokenizer.token_piece(), &bugfix_version_);
    }

    // Check release name for Chrome OS.
    std::string release_name;
    if (GetLsbReleaseValue(kChromeOsReleaseNameKey, &release_name)) {
      for (size_t i = 0; i < base::size(kChromeOsReleaseNames); ++i) {
        if (release_name == kChromeOsReleaseNames[i]) {
          is_running_on_chromeos_ = true;
          break;
        }
      }
    }
  }

 private:
  using LsbReleaseMap = std::map<std::string, std::string>;
  Time lsb_release_time_;
  LsbReleaseMap lsb_release_map_;
  int32_t major_version_ = 0;
  int32_t minor_version_ = 0;
  int32_t bugfix_version_ = 0;
  bool is_running_on_chromeos_ = false;
};

ChromeOSVersionInfo* g_chromeos_version_info_for_test = nullptr;

ChromeOSVersionInfo& GetChromeOSVersionInfo() {
  // ChromeOSVersionInfo only stores the parsed lsb-release values, not the full
  // contents of the lsb-release file. Therefore, use a second instance for
  // overrides in tests so we can cleanly restore the original lsb-release.
  if (g_chromeos_version_info_for_test)
    return *g_chromeos_version_info_for_test;

  static base::NoDestructor<ChromeOSVersionInfo> version_info;
  return *version_info;
}

}  // namespace

// static
std::string SysInfo::HardwareModelName() {
  std::string board = GetLsbReleaseBoard();
  // GetLsbReleaseBoard() may be suffixed with a "-signed-" and other extra
  // info. Strip it.
  const size_t index = board.find("-signed-");
  if (index != std::string::npos)
    board.resize(index);

  return base::ToUpperASCII(board);
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version) {
  return GetChromeOSVersionInfo().GetVersionNumbers(
      major_version, minor_version, bugfix_version);
}

// static
std::string SysInfo::OperatingSystemVersion() {
  int32_t major, minor, bugfix;
  GetChromeOSVersionInfo().GetVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
}

// static
std::string SysInfo::KernelVersion() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  return std::string(info.release);
}

// static
bool SysInfo::GetLsbReleaseValue(const std::string& key, std::string* value) {
  return GetChromeOSVersionInfo().GetLsbReleaseValue(key, value);
}

// static
std::string SysInfo::GetLsbReleaseBoard() {
  const char kMachineInfoBoard[] = "CHROMEOS_RELEASE_BOARD";
  std::string board;
  if (!GetLsbReleaseValue(kMachineInfoBoard, &board))
    board = "unknown";
  return board;
}

// static
Time SysInfo::GetLsbReleaseTime() {
  return GetChromeOSVersionInfo().lsb_release_time();
}

// static
bool SysInfo::IsRunningOnChromeOS() {
  return GetChromeOSVersionInfo().is_running_on_chromeos();
}

// static
void SysInfo::SetChromeOSVersionInfoForTest(const std::string& lsb_release,
                                            const Time& lsb_release_time) {
  DCHECK(!g_chromeos_version_info_for_test) << "Nesting is not allowed";
  g_chromeos_version_info_for_test =
      new ChromeOSVersionInfo(ChromeOSVersionInfo::FOR_TEST);
  g_chromeos_version_info_for_test->ParseLsbRelease(lsb_release);
  g_chromeos_version_info_for_test->set_lsb_release_time(lsb_release_time);
}

// static
void SysInfo::ResetChromeOSVersionInfoForTest() {
  DCHECK(g_chromeos_version_info_for_test);
  delete g_chromeos_version_info_for_test;
  g_chromeos_version_info_for_test = nullptr;
}

// static
void SysInfo::CrashIfChromeOSNonTestImage() {
  if (!IsRunningOnChromeOS())
    return;

  // On the test images etc/lsb-release has a line:
  // CHROMEOS_RELEASE_TRACK=testimage-channel.
  const char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  const char kTestImageRelease[] = "testimage-channel";

  std::string track;
  CHECK(SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &track));

  // Crash if can't find test-image marker in the release track.
  CHECK_NE(track.find(kTestImageRelease), std::string::npos);
}

}  // namespace base
