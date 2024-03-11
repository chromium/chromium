// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/metrics.h"

#import <Foundation/Foundation.h>
#include <sys/attr.h>
#include <sys/vnode.h>
#include <unistd.h>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"

namespace {

// Don't renumber these values. They are recorded in UMA metrics.
// See enum MacFileSystemType in enums.xml.
enum class FileSystemType {
  kHFS = 0,
  kAPFS = 1,
  kUnknown = 2,
  kMaxValue = kUnknown,
};

FileSystemType VolumeTagToFileSystemType(enum vtagtype tag) {
  static constexpr auto map =
      base::MakeFixedFlatMap<enum vtagtype, FileSystemType>({
          {VT_HFS, FileSystemType::kHFS},
          {VT_APFS, FileSystemType::kAPFS},
      });
  const auto it = map.find(tag);
  return it != map.end() ? it->second : FileSystemType::kUnknown;
}

void RecordAppFileSystemTypeUsingVolumeTag(enum vtagtype tag) {
  base::UmaHistogramEnumeration("Mac.AppFileSystemType",
                                VolumeTagToFileSystemType(tag));
}

struct alignas(4) AttributeBuffer {
  uint32_t length;
  enum vtagtype tag;
} __attribute__((packed));

}  // namespace

namespace mac_metrics {

void RecordAppFileSystemType() {
  const char* path =
      NSProcessInfo.processInfo.arguments.firstObject.fileSystemRepresentation;

  struct attrlist attr_list = {
      .bitmapcount = ATTR_BIT_MAP_COUNT,  // default
      .commonattr = ATTR_CMN_OBJTAG       // get the file system type
  };
  struct AttributeBuffer buff;

  // Using getattrlist instead of statfs. ATTR_CMN_OBJTAG from getattrlist is
  // the only value needed, which should be faster to get than the whole statfs
  // struct. Additionally the statfs field f_type does not seem to map to any
  // public enum of file system types. According to man 2 getattrlist it is not
  // a useful value. The f_fstypename field could be used but adds the
  // additional burden of handling strings.
  if (getattrlist(path, &attr_list, &buff, sizeof(buff), 0) != 0) {
    // Record FileSystemType::kUnknown if there is a failure with getattrlist.
    RecordAppFileSystemTypeUsingVolumeTag(VT_NON);
    return;
  }
  DCHECK_GE(sizeof(buff), buff.length);
  RecordAppFileSystemTypeUsingVolumeTag(buff.tag);
}

}  // namespace mac_metrics
