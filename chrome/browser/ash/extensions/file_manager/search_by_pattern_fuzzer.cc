// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"
#include "chrome/browser/ash/fileapi/recent_source.h"

/**
 * Helper function that creates a file without content (we only care about
 * names). Returns true if successful, false otherwise.
 */
bool CreateTestFile(const base::FilePath& file_path) {
  auto path = base::FilePath(file_path);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  return file.created();
}

/**
 * Checks the resiliance of SearchByPattern to fuzzed queries.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    // Not a fuzzer error, so we return 0.
    return 0;
  }

  // Create files in the temp_dir, as otherwise SearchByPattern will never use
  // the query (it only uses it when matching against files it found).
  base::FilePath test_dir = temp_dir.GetPath();
  std::vector<std::string> file_names = {"foo.txt", "bar.exe"};
  for (const auto& name : file_names) {
    base::FilePath file_path = test_dir.Append(name);
    if (!CreateTestFile(file_path)) {
      LOG(ERROR) << "Failed to create file \"" << file_path.MaybeAsASCII()
                 << "\"";
      // Not a fuzzer error, so we return 0.
      return 0;
    }
  }

  base::Time min_modified_time = base::Time::UnixEpoch();
  std::string query = std::string(reinterpret_cast<const char*>(data), size);

  // Searching by fuzzed query. Using most broad parameters, excluding no paths,
  // accepting files of any time with modified date after 1 Jan 1970.
  std::vector<base::FilePath> excluded_path;
  extensions::SearchByPattern(test_dir, excluded_path, query, min_modified_time,
                              ash::RecentSource::FileType::kAll,
                              file_names.size());
  return 0;
}
