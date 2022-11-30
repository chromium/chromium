// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_STORAGE_PARTITION_DESCRIPTOR_H_
#define CHROME_BROWSER_PROFILES_STORAGE_PARTITION_DESCRIPTOR_H_

#include "base/files/file_path.h"

// This structure combines a StoragePartition's on-disk path and a boolean for
// whether the partition should be persisted on disk. Its purpose is to serve as
// a unique key to look up RequestContext objects in the ProfileIOData derived
// classes.
struct StoragePartitionDescriptor {
  StoragePartitionDescriptor(const base::FilePath& partition_path,
                             const bool in_memory_only)
    : path(partition_path),
      in_memory(in_memory_only) {}

  const base::FilePath path;
  const bool in_memory;
};

// Functor for operator <.
struct StoragePartitionDescriptorLess {
  bool operator()(const StoragePartitionDescriptor& lhs,
                  const StoragePartitionDescriptor& rhs) const {
    if (lhs.path != rhs.path)
      return lhs.path < rhs.path;
    else if (lhs.in_memory != rhs.in_memory)
      return lhs.in_memory < rhs.in_memory;
    else
      return false;
  }
};

#endif  // CHROME_BROWSER_PROFILES_STORAGE_PARTITION_DESCRIPTOR_H_
