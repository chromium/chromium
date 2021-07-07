// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_descriptor_posix.h"

#include "base/files/file.h"

namespace base {

FileDescriptor::FileDescriptor() = default;

FileDescriptor::FileDescriptor(int ifd, bool iauto_close)
    : fd(ifd), auto_close(iauto_close) {}

FileDescriptor::FileDescriptor(File file)
    : fd(file.TakePlatformFile()), auto_close(true) {}

FileDescriptor::FileDescriptor(ScopedFD fd)
    : fd(fd.release()), auto_close(true) {}

bool FileDescriptor::operator==(const FileDescriptor& other) const {
  return fd == other.fd && auto_close == other.auto_close;
}

bool FileDescriptor::operator!=(const FileDescriptor& other) const {
  return !operator==(other);
}

bool FileDescriptor::operator<(const FileDescriptor& other) const {
  return other.fd < fd;
}

}  // namespace base