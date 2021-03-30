// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_

#include "chromeos/components/file_manager/file_manager_ui_delegate.h"

namespace content {
class WebUI;
}  // namespace content

// Chrome browser FileManagerUIDelegate implementation.
class ChromeFileManagerUIDelegate : public FileManagerUIDelegate {
 public:
  explicit ChromeFileManagerUIDelegate(content::WebUI* web_ui);

  ChromeFileManagerUIDelegate(const ChromeFileManagerUIDelegate&) = delete;
  ChromeFileManagerUIDelegate& operator=(const ChromeFileManagerUIDelegate&) =
      delete;

  // FileManagerUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource*) const override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
