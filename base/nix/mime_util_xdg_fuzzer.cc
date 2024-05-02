// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/nix/mime_util_xdg.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data_ptr, size_t size) {
  // SAFETY: LibFuzzer provides a valid pointer/size pair.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, size));

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    // Not a fuzzer error, so we return 0.
    LOG(ERROR) << "Failed to create temp dir";
    return 0;
  }

  // The parser reads file $XDG_DATA_DIRS/mime/mime.cache.
  setenv("XDG_DATA_DIRS", temp_dir.GetPath().value().c_str(), 1);
  base::FilePath mime_dir = temp_dir.GetPath().Append("mime");
  base::FilePath mime_cache = mime_dir.Append("mime.cache");
  if (!base::CreateDirectory(mime_dir) || !base::WriteFile(mime_cache, data)) {
    LOG(ERROR) << "Failed to create " << mime_cache;
    // Not a fuzzer error, so we return 0.
    return 0;
  }

  base::FilePath dummy_path("foo.txt");
  std::string type = base::nix::GetFileMimeType(dummy_path);
  return 0;
}
