// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_
#define ASH_COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_

#include "base/files/file_path.h"

namespace arc {

// A move-only class to hold status of the host file system. This class is for
// ArcVmClientAdapter's internal use and visible for only testing purposes. Do
// not use directly.
class FileSystemStatus {
 public:
  FileSystemStatus(FileSystemStatus&& other);
  ~FileSystemStatus();
  FileSystemStatus& operator=(FileSystemStatus&& other);

  FileSystemStatus(const FileSystemStatus&) = delete;
  FileSystemStatus& operator=(const FileSystemStatus&) = delete;

  static FileSystemStatus GetFileSystemStatusBlocking() {
    return FileSystemStatus();
  }

  bool is_host_rootfs_writable() const { return is_host_rootfs_writable_; }
  bool is_system_image_ext_format() const {
    return is_system_image_ext_format_;
  }
  bool has_adbd_json() const { return has_adbd_json_; }
  const base::FilePath& vendor_image_path() const { return vendor_image_path_; }
  const base::FilePath& block_apex_path() const { return block_apex_path_; }

  // Setters for testing.
  void set_host_rootfs_writable_for_testing(bool is_host_rootfs_writable) {
    is_host_rootfs_writable_ = is_host_rootfs_writable;
  }
  void set_system_image_ext_format_for_testing(
      bool is_system_image_ext_format) {
    is_system_image_ext_format_ = is_system_image_ext_format;
  }
  void set_vendor_image_path_for_testing(
      const base::FilePath& vendor_image_path) {
    vendor_image_path_ = vendor_image_path;
  }
  void set_block_apex_path_for_testing(const base::FilePath& block_apex_path) {
    block_apex_path_ = block_apex_path;
  }

  static bool IsSystemImageExtFormatForTesting(const base::FilePath& path) {
    return IsSystemImageExtFormat(path);
  }

 private:
  FileSystemStatus();

  static bool IsHostRootfsWritable();

  // https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout
  // Super block starts from block 0, offset 0x400.
  // 0x38: Magic signature (Len=16, value=0xEF53) in little-endian order.
  static bool IsSystemImageExtFormat(const base::FilePath& path);

  bool is_host_rootfs_writable_;
  base::FilePath vendor_image_path_;
  base::FilePath block_apex_path_;
  bool is_system_image_ext_format_;
  bool has_adbd_json_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_FILE_SYSTEM_STATUS_H_
