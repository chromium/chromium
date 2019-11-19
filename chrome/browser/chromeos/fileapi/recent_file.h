// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_FILE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_FILE_H_

#include "base/time/time.h"
#include "storage/browser/file_system/file_system_url.h"

namespace chromeos {

class RecentFile {
 public:
  RecentFile();
  RecentFile(const storage::FileSystemURL& url,
             const base::Time& last_modified);
  RecentFile(const RecentFile& other);
  ~RecentFile();
  RecentFile& operator=(const RecentFile& other);

  const storage::FileSystemURL& url() const { return url_; }
  const base::Time& last_modified() const { return last_modified_; }

 private:
  storage::FileSystemURL url_;
  base::Time last_modified_;
};

// A comparator that sorts files in *descending* order of last modified time.
struct RecentFileComparator {
  bool operator()(const RecentFile& a, const RecentFile& b);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_FILE_H_
