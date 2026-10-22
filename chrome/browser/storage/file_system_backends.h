// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_FILE_SYSTEM_BACKENDS_H_
#define CHROME_BROWSER_STORAGE_FILE_SYSTEM_BACKENDS_H_

#include <memory>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace storage {
class FileSystemBackend;
}  // namespace storage

void GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends);

#endif  // CHROME_BROWSER_STORAGE_FILE_SYSTEM_BACKENDS_H_
