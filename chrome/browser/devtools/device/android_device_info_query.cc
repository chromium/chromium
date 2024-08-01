// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/android_device_manager.h"

namespace {

#define SEPARATOR "======== output separator ========"

const char kAllCommands[] = "shell:"
    "getprop ro.product.model\n"
    "echo " SEPARATOR "\n"
    "dumpsys window policy\n"
    "echo " SEPARATOR "\n"
    "ps\n"
    "echo " SEPARATOR "\n"
    "cat /proc/net/unix\n"
    "echo " SEPARATOR "\n"
    "dumpsys user\n";

const char kSeparator[] = SEPARATOR;

#undef SEPARATOR

const char kScreenSizePrefix[] = "mStable=";
const char kUserInfoPrefix[] = "UserInfo{";

const char kDevToolsSocketSuffix[] = "_devtools_remote";

const char kChromeDefaultName[] = "Chrome";
const char kChromeDefaultSocket[] = "chrome_devtools_remote";

const char kWebViewSocketPrefix[] = "webview_devtools_remote";
const char kWebViewNameTemplate[] = "WebView in %s";

struct BrowserDescriptor {
  const char* package;
  const char* socket;
  const char* display_name;
};

const BrowserDescriptor kBrowserDescriptors[] = {
  {
    "com.google.android.apps.chrome",
    kChromeDefaultSocket,
    "Chromium"
  },
  {
    "com.chrome.canary",
    kChromeDefaultSocket,
    "Chrome Canary"
  },
  {
    "com.chrome.dev",
    kChromeDefaultSocket,
    "Chrome Dev"
  },
  {
    "com.chrome.beta",
    kChromeDefaultSocket,
    "Chrome Beta"
  },
  {
    "com.android.chrome",
    kChromeDefaultSocket,
    kChromeDefaultName
  },
  {
    "org.chromium.android_webview.shell",
    "webview_devtools_remote",
    "WebView Test Shell"
  },
  {
    "org.chromium.content_shell_apk",
    "content_shell_devtools_remote",
    "Content Shell"
  },
  {
    "org.chromium.chrome",
    kChromeDefaultSocket,
    "Chromium"
  },
};

const BrowserDescriptor* FindBrowserDescriptor(const std::string& package) {
  size_t count = std::size(kBrowserDescriptors);
  for (size_t i = 0; i < count; i++) {
    if (kBrowserDescriptors[i].package == package)
      return &kBrowserDescriptors[i];
  }
  return nullptr;
}

bool BrowserCompare(const AndroidDeviceManager::BrowserInfo& a,
                    const AndroidDeviceManager::BrowserInfo& b) {
  size_t count = std::size(kBrowserDescriptors);
  for (size_t i = 0; i < count; i++) {
    bool isA = kBrowserDescriptors[i].display_name == a.display_name;
    bool isB = kBrowserDescriptors[i].display_name == b.display_name;
    if (isA != isB)
      return isA;
    if (isA && isB)
      break;
  }
  return a.socket_name < b.socket_name;
}

using StringMap = std::map<std::string, std::string>;

void MapProcessesToPackages(const std::string& response,
                            StringMap* pid_to_package,
                            StringMap* pid_to_user) {
  // Parse 'ps' output which on Android looks like this:
  //
  // USER PID PPID VSIZE RSS WCHAN PC ? NAME
  //
  for (std::string_view line : base::SplitStringPiece(
           response, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> fields =
        base::SplitString(line, " \r", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (fields.size() < 9)
      continue;
    std::string pid = fields[1];
    (*pid_to_package)[pid] = fields[8];
    (*pid_to_user)[pid] = fields[0];
  }
}

StringMap MapSocketsToProcesses(const std::string& response) {
  // Parse 'cat /proc/net/unix' output which on Android looks like this:
  //
  // Num       RefCount Protocol Flags    Type St Inode Path
  // 00000000: 00000002 00000000 00010000 0001 01 331813 /dev/socket/zygote
  // 00000000: 00000002 00000000 00010000 0001 01 358606 @xxx_devtools_remote
  // 00000000: 00000002 00000000 00010000 0001 01 347300 @yyy_devtools_remote
  //
  // We need to find records with paths starting from '@' (abstract socket)
  // and containing the channel pattern ("_devtools_remote").
  StringMap socket_to_pid;
  for (std::string_view line : base::SplitStringPiece(
           response, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> fields =
        base::SplitString(line, " \r", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (fields.size() < 8)
      continue;
    if (fields[3] != "00010000" || fields[5] != "01")
      continue;
    std::string path_field = fields[7];
    if (path_field.empty() || path_field[0] != '@')
      continue;
    size_t socket_name_pos = path_field.find(kDevToolsSocketSuffix);
    if (socket_name_pos == std::string::npos)
      continue;

    std::string socket = path_field.substr(1);

    std::string pid;
    size_t socket_name_end = socket_name_pos + strlen(kDevToolsSocketSuffix);
    if (socket_name_end < path_field.size() &&
        path_field[socket_name_end] == '_') {
      pid = path_field.substr(socket_name_end + 1);
    }
    socket_to_pid[socket] = pid;
  }
  return socket_to_pid;
}

gfx::Size ParseScreenSize(std::string_view str) {
  std::vector<std::string_view> pairs = base::SplitStringPiece(
      str, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pairs.size() != 2)
    return gfx::Size();

  int width;
  int height;
  std::vector<std::string_view> numbers =
      base::SplitStringPiece(pairs[1].substr(1, pairs[1].size() - 2), ",",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (numbers.size() != 2 ||
      !base::StringToInt(numbers[0], &width) ||
      !base::StringToInt(numbers[1], &height))
    return gfx::Size();

  return gfx::Size(width, height);
}

gfx::Size ParseWindowPolicyResponse(const std::string& response) {
  for (std::string_view line : base::SplitStringPiece(
           response, "\r", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    size_t pos = line.find(kScreenSizePrefix);
    if (pos != std::string_view::npos) {
      return ParseScreenSize(
          line.substr(pos + strlen(kScreenSizePrefix)));
    }
  }
  return gfx::Size();
}

StringMap MapIdsToUsers(const std::string& response) {
  // Parse 'dumpsys user' output which looks like this:
  // Users:
  //   UserInfo{0:Test User:13} serialNo=0
  //     Created: <unknown>
  //     Last logged in: +17m18s871ms ago
  //   UserInfo{10:User with : (colon):10} serialNo=10
  //     Created: +3d4h35m1s139ms ago
  //     Last logged in: +17m26s287ms ago
  StringMap id_to_username;
  for (std::string_view line : base::SplitStringPiece(
           response, "\r", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    size_t pos = line.find(kUserInfoPrefix);
    if (pos != std::string::npos) {
      std::string_view fields = line.substr(pos + strlen(kUserInfoPrefix));
      size_t first_pos = fields.find_first_of(":");
      size_t last_pos = fields.find_last_of(":");
      if (first_pos != std::string::npos && last_pos != std::string::npos) {
        std::string id(fields.substr(0, first_pos));
        std::string name(
            fields.substr(first_pos + 1, last_pos - first_pos - 1));
        id_to_username[id] = name;
      }
    }
  }
  return id_to_username;
}

std::string GetUserName(const std::string& unix_user,
                        const StringMap id_to_username) {
  // Parse username as returned by ps which looks like 'u0_a31'
  // where '0' is user id and '31' is app id.
  if (!unix_user.empty() && unix_user[0] == 'u') {
    size_t pos = unix_user.find('_');
    if (pos != std::string::npos) {
      auto it = id_to_username.find(unix_user.substr(1, pos - 1));
      if (it != id_to_username.end())
        return it->second;
    }
  }
  return std::string();
}

AndroidDeviceManager::BrowserInfo::Type
GetBrowserType(const std::string& socket) {
  if (base::StartsWith(socket, kChromeDefaultSocket,
                       base::CompareCase::SENSITIVE)) {
    return AndroidDeviceManager::BrowserInfo::kTypeChrome;
  }

  if (base::StartsWith(socket, kWebViewSocketPrefix,
                       base::CompareCase::SENSITIVE)) {
    return AndroidDeviceManager::BrowserInfo::kTypeWebView;
  }

  return AndroidDeviceManager::BrowserInfo::kTypeOther;
}

void ReceivedResponse(AndroidDeviceManager::DeviceInfoCallback callback,
                      int result,
                      const std::string& response) {
  AndroidDeviceManager::DeviceInfo device_info;
  if (result < 0) {
    std::move(callback).Run(device_info);
    return;
  }
  std::vector<std::string> outputs = base::SplitStringUsingSubstr(
      response, kSeparator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (outputs.size() != 5) {
    std::move(callback).Run(device_info);
    return;
  }
  device_info.connected = true;
  device_info.model = outputs[0];
  device_info.screen_size = ParseWindowPolicyResponse(outputs[1]);
  StringMap pid_to_package;
  StringMap pid_to_user;
  MapProcessesToPackages(outputs[2], &pid_to_package, &pid_to_user);
  StringMap socket_to_pid = MapSocketsToProcesses(outputs[3]);
  StringMap id_to_username = MapIdsToUsers(outputs[4]);
  std::set<std::string> used_pids;
  for (const auto& pair : socket_to_pid)
    used_pids.insert(pair.second);

  for (const auto& pair : pid_to_package) {
    std::string pid = pair.first;
    std::string package = pair.second;
    if (used_pids.find(pid) == used_pids.end()) {
      const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
      if (descriptor)
        socket_to_pid[descriptor->socket] = pid;
    }
  }

  for (const auto& pair : socket_to_pid) {
    std::string socket = pair.first;
    std::string pid = pair.second;
    std::string package;
    auto pit = pid_to_package.find(pid);
    if (pit != pid_to_package.end())
      package = pit->second;

    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = socket;
    browser_info.type = GetBrowserType(socket);
    browser_info.display_name =
        AndroidDeviceManager::GetBrowserName(socket, package);

    auto uit = pid_to_user.find(pid);
    if (uit != pid_to_user.end())
      browser_info.user = GetUserName(uit->second, id_to_username);

    device_info.browser_info.push_back(browser_info);
  }
  std::sort(device_info.browser_info.begin(),
            device_info.browser_info.end(),
            &BrowserCompare);
  std::move(callback).Run(device_info);
}

}  // namespace

// static
std::string AndroidDeviceManager::GetBrowserName(const std::string& socket,
                                                 const std::string& package) {
  if (package.empty()) {
    // Derive a fallback display name from the socket name.
    std::string name = socket.substr(0, socket.find(kDevToolsSocketSuffix));
    name[0] = base::ToUpperASCII(name[0]);
    return name;
  }

  const BrowserDescriptor* descriptor = FindBrowserDescriptor(package);
  if (descriptor)
    return descriptor->display_name;

  if (GetBrowserType(socket) ==
      AndroidDeviceManager::BrowserInfo::kTypeWebView)
    return base::StringPrintf(kWebViewNameTemplate, package.c_str());

  return package;
}

// static
void AndroidDeviceManager::QueryDeviceInfo(RunCommandCallback command_callback,
                                           DeviceInfoCallback callback) {
  std::move(command_callback)
      .Run(kAllCommands,
           base::BindOnce(&ReceivedResponse, std::move(callback)));
}
