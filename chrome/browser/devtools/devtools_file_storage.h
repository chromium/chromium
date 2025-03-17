// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_STORAGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/devtools_file_helper.h"

namespace content {
class WebContents;
}

class DevToolsFileStorage : public DevToolsFileHelper::Storage {
 public:
  explicit DevToolsFileStorage(content::WebContents* web_contents);
  DevToolsFileStorage(const DevToolsFileStorage&) = delete;
  DevToolsFileStorage& operator=(const DevToolsFileStorage&) = delete;
  ~DevToolsFileStorage() override;

  DevToolsFileHelper::FileSystem RegisterFileSystem(
      const base::FilePath& path,
      const std::string& type) override;
  void UnregisterFileSystem(const base::FilePath& path) override;

  std::vector<base::FilePath> GetDraggedFileSystemPaths(
      const GURL& file_system_url) override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_FILE_STORAGE_H_
