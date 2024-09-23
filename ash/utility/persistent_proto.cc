// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/persistent_proto.h"

namespace ash::internal {

WriteStatus Write(const base::FilePath& filepath, std::string_view proto_str) {
  if (const base::FilePath directory = filepath.DirName();
      !base::DirectoryExists(directory)) {
    base::CreateDirectory(directory);
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::ImportantFileWriter::WriteFileAtomically(
          filepath, proto_str, "AppListPersistentProto")) {
    return WriteStatus::kWriteError;
  }

  return WriteStatus::kOk;
}

}  // namespace ash::internal
