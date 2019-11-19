// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <memory>

#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/process/launch.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace base {

namespace {

// Not needed for OS_CHROMEOS.
#if defined(OS_LINUX)
enum LinuxDistroState {
  STATE_DID_NOT_CHECK  = 0,
  STATE_CHECK_STARTED  = 1,
  STATE_CHECK_FINISHED = 2,
};

// Helper class for GetLinuxDistro().
class LinuxDistroHelper {
 public:
  // Retrieves the Singleton.
  static LinuxDistroHelper* GetInstance() {
    return Singleton<LinuxDistroHelper>::get();
  }

  // The simple state machine goes from:
  // STATE_DID_NOT_CHECK -> STATE_CHECK_STARTED -> STATE_CHECK_FINISHED.
  LinuxDistroHelper() : state_(STATE_DID_NOT_CHECK) {}
  ~LinuxDistroHelper() = default;

  // Retrieve the current state, if we're in STATE_DID_NOT_CHECK,
  // we automatically move to STATE_CHECK_STARTED so nobody else will
  // do the check.
  LinuxDistroState State() {
    AutoLock scoped_lock(lock_);
    if (STATE_DID_NOT_CHECK == state_) {
      state_ = STATE_CHECK_STARTED;
      return STATE_DID_NOT_CHECK;
    }
    return state_;
  }

  // Indicate the check finished, move to STATE_CHECK_FINISHED.
  void CheckFinished() {
    AutoLock scoped_lock(lock_);
    DCHECK_EQ(STATE_CHECK_STARTED, state_);
    state_ = STATE_CHECK_FINISHED;
  }

 private:
  Lock lock_;
  LinuxDistroState state_;
};

#if !defined(OS_CHROMEOS)
std::string GetKeyValueFromOSReleaseFile(const std::string& input,
                                         const char* key) {
  StringPairs key_value_pairs;
  SplitStringIntoKeyValuePairs(input, '=', '\n', &key_value_pairs);
  for (const auto& pair : key_value_pairs) {
    const std::string& key_str = pair.first;
    const std::string& value_str = pair.second;
    if (key_str == key) {
      // It can contain quoted characters.
      std::stringstream ss;
      std::string pretty_name;
      ss << value_str;
      // Quoted with a single tick?
      if (value_str[0] == '\'')
        ss >> std::quoted(pretty_name, '\'');
      else
        ss >> std::quoted(pretty_name);

      return pretty_name;
    }
  }

  return "";
}

bool ReadDistroFromOSReleaseFile(const char* file) {
  static const char kPrettyName[] = "PRETTY_NAME";

  std::string os_release_content;
  if (!ReadFileToString(FilePath(file), &os_release_content))
    return false;

  std::string pretty_name =
      GetKeyValueFromOSReleaseFile(os_release_content, kPrettyName);
  if (pretty_name.empty())
    return false;

  SetLinuxDistro(pretty_name);
  return true;
}

// https://www.freedesktop.org/software/systemd/man/os-release.html
void GetDistroNameFromOSRelease() {
  static const char* const kFilesToCheck[] = {"/etc/os-release",
                                              "/usr/lib/os-release"};
  for (const char* file : kFilesToCheck) {
    if (ReadDistroFromOSReleaseFile(file))
      return;
  }
}
#endif  // if !defined(OS_CHROMEOS)
#endif  // if defined(OS_LINUX)

}  // namespace

// Account for the terminating null character.
static const int kDistroSize = 128 + 1;

// We use this static string to hold the Linux distro info. If we
// crash, the crash handler code will send this in the crash dump.
char g_linux_distro[kDistroSize] =
#if defined(OS_CHROMEOS)
    "CrOS";
#elif defined(OS_ANDROID)
    "Android";
#else  // if defined(OS_LINUX)
    "Unknown";
#endif

// This function is only supposed to be used in tests. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part
// of "base" where "UNIT_TEST" is not defined. So we need to specify
// "BASE_EXPORT" here again so that they are visible to tests.
BASE_EXPORT std::string GetKeyValueFromOSReleaseFileForTesting(
    const std::string& input,
    const char* key) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return GetKeyValueFromOSReleaseFile(input, key);
#else
  return "";
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
}

std::string GetLinuxDistro() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  return g_linux_distro;
#elif defined(OS_LINUX)
  LinuxDistroHelper* distro_state_singleton = LinuxDistroHelper::GetInstance();
  LinuxDistroState state = distro_state_singleton->State();
  if (STATE_CHECK_FINISHED == state)
    return g_linux_distro;
  if (STATE_CHECK_STARTED == state)
    return "Unknown"; // Don't wait for other thread to finish.
  DCHECK_EQ(state, STATE_DID_NOT_CHECK);
  // We do this check only once per process. If it fails, there's
  // little reason to believe it will work if we attempt to run it again.
  GetDistroNameFromOSRelease();
  distro_state_singleton->CheckFinished();
  return g_linux_distro;
#else
  NOTIMPLEMENTED();
  return "Unknown";
#endif
}

void SetLinuxDistro(const std::string& distro) {
  std::string trimmed_distro;
  TrimWhitespaceASCII(distro, TRIM_ALL, &trimmed_distro);
  strlcpy(g_linux_distro, trimmed_distro.c_str(), kDistroSize);
}

bool GetThreadsForProcess(pid_t pid, std::vector<pid_t>* tids) {
  // 25 > strlen("/proc//task") + strlen(std::to_string(INT_MAX)) + 1 = 22
  char buf[25];
  strings::SafeSPrintf(buf, "/proc/%d/task", pid);
  DirReaderPosix dir_reader(buf);

  if (!dir_reader.IsValid()) {
    DLOG(WARNING) << "Cannot open " << buf;
    return false;
  }

  while (dir_reader.Next()) {
    char* endptr;
    const unsigned long int tid_ul = strtoul(dir_reader.name(), &endptr, 10);
    if (tid_ul == ULONG_MAX || *endptr)
      continue;
    tids->push_back(tid_ul);
  }

  return true;
}

pid_t FindThreadIDWithSyscall(pid_t pid, const std::string& expected_data,
                              bool* syscall_supported) {
  if (syscall_supported != nullptr)
    *syscall_supported = false;

  std::vector<pid_t> tids;
  if (!GetThreadsForProcess(pid, &tids))
    return -1;

  std::unique_ptr<char[]> syscall_data(new char[expected_data.length()]);
  for (pid_t tid : tids) {
    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%d/task/%d/syscall", pid, tid);
    int fd = open(buf, O_RDONLY);
    if (fd < 0)
      continue;
    if (syscall_supported != nullptr)
      *syscall_supported = true;
    bool read_ret = ReadFromFD(fd, syscall_data.get(), expected_data.length());
    close(fd);
    if (!read_ret)
      continue;

    if (0 == strncmp(expected_data.c_str(), syscall_data.get(),
                     expected_data.length())) {
      return tid;
    }
  }
  return -1;
}

pid_t FindThreadID(pid_t pid, pid_t ns_tid, bool* ns_pid_supported) {
  if (ns_pid_supported)
    *ns_pid_supported = false;

  std::vector<pid_t> tids;
  if (!GetThreadsForProcess(pid, &tids))
    return -1;

  for (pid_t tid : tids) {
    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%d/task/%d/status", pid, tid);
    std::string status;
    if (!ReadFileToString(FilePath(buf), &status))
      return -1;
    StringTokenizer tokenizer(status, "\n");
    while (tokenizer.GetNext()) {
      StringPiece value_str(tokenizer.token_piece());
      if (!value_str.starts_with("NSpid"))
        continue;
      if (ns_pid_supported)
        *ns_pid_supported = true;
      std::vector<StringPiece> split_value_str = SplitStringPiece(
          value_str, "\t", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
      DCHECK_GE(split_value_str.size(), 2u);
      int value;
      // The last value in the list is the PID in the namespace.
      if (StringToInt(split_value_str.back(), &value) && value == ns_tid) {
        // The second value in the list is the real PID.
        if (StringToInt(split_value_str[1], &value))
          return value;
      }
      break;
    }
  }
  return -1;
}

}  // namespace base
