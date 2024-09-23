// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/device_info_fetcher_linux.h"

#if defined(USE_GIO)
#include <gio/gio.h>
#endif  // defined(USE_GIO)
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <string>

#include "base/environment.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "net/base/network_interfaces.h"

namespace enterprise_signals {

namespace {

std::string ReadFile(std::string path_str) {
  base::FilePath path(path_str);
  std::string output;
  if (base::PathExists(path) && base::ReadFileToString(path, &output))
    base::TrimWhitespaceASCII(output, base::TrimPositions::TRIM_ALL, &output);

  return output;
}

std::string GetDeviceModel() {
  return ReadFile("/sys/class/dmi/id/product_name");
}

std::string GetOsVersion() {
  base::FilePath os_release_file("/etc/os-release");
  std::string release_info;
  base::StringPairs values;
  if (base::PathExists(os_release_file) &&
      base::ReadFileToStringWithMaxSize(os_release_file, &release_info, 8192) &&
      base::SplitStringIntoKeyValuePairs(release_info, '=', '\n', &values)) {
    auto version_id = base::ranges::find(
        values, "VERSION_ID", &std::pair<std::string, std::string>::first);
    if (version_id != values.end()) {
      return std::string(
          base::TrimString(version_id->second, "\"", base::TRIM_ALL));
    }
  }
  return base::SysInfo::OperatingSystemVersion();
}

std::string GetSecurityPatchLevel() {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
}

std::string GetDeviceHostName() {
  return net::GetHostName();
}

std::string GetSerialNumber() {
  return ReadFile("/sys/class/dmi/id/product_serial");
}

// Implements the logic from the native client setup script. It reads the
// setting value straight from gsettings but picks the schema relevant to the
// currently active desktop environment.
// The current implementation support Gnone and Cinnamon only.
SettingValue GetScreenlockSecured() {
#if defined(USE_GIO)
  static constexpr char kLockScreenKey[] = "lock-enabled";

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop_env != base::nix::DESKTOP_ENVIRONMENT_CINNAMON &&
      desktop_env != base::nix::DESKTOP_ENVIRONMENT_GNOME) {
    return SettingValue::UNKNOWN;
  }

  const std::string settings_schema = base::StringPrintf(
      "org.%s.desktop.screensaver",
      desktop_env == base::nix::DESKTOP_ENVIRONMENT_CINNAMON ? "cinnamon"
                                                             : "gnome");

  GSettingsSchema* screensaver_schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), settings_schema.c_str(), FALSE);
  GSettings* screensaver_settings = nullptr;
  if (!screensaver_schema ||
      !g_settings_schema_has_key(screensaver_schema, kLockScreenKey)) {
    return SettingValue::UNKNOWN;
  }
  screensaver_settings = g_settings_new(settings_schema.c_str());
  if (!screensaver_settings)
    return SettingValue::UNKNOWN;
  gboolean lock_screen_enabled =
      g_settings_get_boolean(screensaver_settings, kLockScreenKey);
  g_object_unref(screensaver_settings);

  return lock_screen_enabled ? SettingValue::ENABLED : SettingValue::DISABLED;
#else
  return SettingValue::UNKNOWN;
#endif  // defined(USE_GIO)
}

// Implements the logic from the native host installation script. First find the
// root device identifier, then locate its parent and get its type.
SettingValue GetDiskEncrypted() {
  struct stat info;
  // First figure out the device identifier. Fail fast if this fails.
  if (stat("/", &info) != 0)
    return SettingValue::UNKNOWN;
  int dev_major = major(info.st_dev);
  // The parent identifier will have the same major and minor 0. If and only if
  // it is a dm device can it also be an encrypted device (as evident from the
  // source code of the lsblk command).
  base::FilePath dev_uuid(
      base::StringPrintf("/sys/dev/block/%d:0/dm/uuid", dev_major));
  std::string uuid;
  if (base::PathExists(dev_uuid)) {
    if (base::ReadFileToStringWithMaxSize(dev_uuid, &uuid, 1024)) {
      // The device uuid starts with the driver type responsible for it. If it
      // is the "crypt" driver then it is an encrypted device.
      bool is_encrypted = base::StartsWith(
          uuid, "crypt-", base::CompareCase::INSENSITIVE_ASCII);
      return is_encrypted ? SettingValue::ENABLED : SettingValue::DISABLED;
    }
    return SettingValue::UNKNOWN;
  }
  return SettingValue::DISABLED;
}

std::vector<std::string> GetMacAddresses() {
  std::vector<std::string> result;
  base::DirReaderPosix reader("/sys/class/net");
  if (!reader.IsValid())
    return result;
  while (reader.Next()) {
    std::string name = reader.name();
    if (name == "." || name == "..")
      continue;
    std::string address;
    base::FilePath address_file(
        base::StringPrintf("/sys/class/net/%s/address", name.c_str()));
    // Filter out the loopback interface here.
    if (!base::PathExists(address_file) ||
        !base::ReadFileToStringWithMaxSize(address_file, &address, 1024) ||
        base::StartsWith(address, "00:00:00:00:00:00",
                         base::CompareCase::SENSITIVE)) {
      continue;
    }

    base::TrimWhitespaceASCII(address, base::TrimPositions::TRIM_TRAILING,
                              &address);
    result.push_back(address);
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DeviceInfoFetcher> DeviceInfoFetcher::CreateInstanceInternal() {
  return std::make_unique<DeviceInfoFetcherLinux>();
}

DeviceInfoFetcherLinux::DeviceInfoFetcherLinux() = default;

DeviceInfoFetcherLinux::~DeviceInfoFetcherLinux() = default;

DeviceInfo DeviceInfoFetcherLinux::Fetch() {
  DeviceInfo device_info;
  device_info.os_name = "linux";
  device_info.os_version = GetOsVersion();
  device_info.security_patch_level = GetSecurityPatchLevel();
  device_info.device_host_name = GetDeviceHostName();
  device_info.device_model = GetDeviceModel();
  device_info.serial_number = GetSerialNumber();
  device_info.screen_lock_secured = GetScreenlockSecured();
  device_info.disk_encrypted = GetDiskEncrypted();
  device_info.mac_addresses = GetMacAddresses();
  return device_info;
}

}  // namespace enterprise_signals
