// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_

#include "base/files/scoped_temp_dir.h"
#include "content/public/test/fake_download_item.h"

namespace enterprise_connectors {

class DownloadItemForTest : public content::FakeDownloadItem {
 public:
  explicit DownloadItemForTest(base::FilePath::StringPieceType file_name);
  const base::FilePath& GetFullPath() const override;

 protected:
  base::ScopedTempDir tmp_dir_;
  base::FilePath file_path_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_
