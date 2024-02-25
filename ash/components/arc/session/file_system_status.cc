// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/file_system_status.h"

#include <linux/magic.h>
#include <sys/statvfs.h>

#include <array>
#include <string>

#include "base/bit_cast.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace arc {
namespace {

constexpr const char kAdbdJson[] = "/etc/arc/adbd.json";
constexpr const char kBuiltinPath[] = "/opt/google/vms/android";
constexpr const char kRootFs[] = "system.raw.img";
constexpr const char kVendorImage[] = "vendor.raw.img";
// Path to block apex payload. This is a composite disk containing
// Android apexes which will be mounted as block devices.
// This is a relative path starting from kBuiltinPath.
constexpr const char kBlockApexPath[] = "apex/payload.img";

}  // namespace

FileSystemStatus::FileSystemStatus(FileSystemStatus&& other) = default;
FileSystemStatus::~FileSystemStatus() = default;
FileSystemStatus& FileSystemStatus::operator=(FileSystemStatus&& other) =
    default;

FileSystemStatus::FileSystemStatus()
    : is_host_rootfs_writable_(IsHostRootfsWritable()),
      vendor_image_path_(base::FilePath(kBuiltinPath).Append(kVendorImage)),
      is_system_image_ext_format_(
          IsSystemImageExtFormat(base::FilePath(kBuiltinPath).Append(kRootFs))),
      has_adbd_json_(base::PathExists(base::FilePath(kAdbdJson))) {
  auto apex_path = base::FilePath(kBuiltinPath).Append(kBlockApexPath);
  block_apex_path_ =
      base::PathExists(apex_path) ? apex_path : base::FilePath("");
}

// static
bool FileSystemStatus::IsHostRootfsWritable() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  struct statvfs buf;
  if (statvfs("/", &buf) < 0) {
    PLOG(ERROR) << "statvfs() failed";
    return false;
  }
  const bool rw = !(buf.f_flag & ST_RDONLY);
  VLOG(1) << "Host's rootfs is " << (rw ? "rw" : "ro");
  return rw;
}

// static
bool FileSystemStatus::IsSystemImageExtFormat(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Cannot open system image file: " << path.value();
    return false;
  }

  std::array<uint8_t, 2> buf;
  if (!file.ReadAndCheck(0x400 + 0x38, base::make_span(buf))) {
    PLOG(ERROR) << "File read error on system image file: " << path.value();
    return false;
  }

  uint16_t magic_le = base::bit_cast<uint16_t>(buf);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  return magic_le == EXT4_SUPER_MAGIC;
#else
#error Unsupported platform
#endif
}

}  // namespace arc
