// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"

#include "extensions/buildflags/buildflags.h"

namespace browsing_data_file_system_util {

std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return {storage::kFileSystemTypeSyncable};
#else
  return {};
#endif
}

}  // namespace browsing_data_file_system_util
