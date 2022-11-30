// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_

#include "ash/webui/file_manager/file_manager_ui_delegate.h"

namespace content {
class WebUI;
}  // namespace content

// Chrome browser FileManagerUIDelegate implementation.
class ChromeFileManagerUIDelegate : public ash::FileManagerUIDelegate {
 public:
  explicit ChromeFileManagerUIDelegate(content::WebUI* web_ui);

  ChromeFileManagerUIDelegate(const ChromeFileManagerUIDelegate&) = delete;
  ChromeFileManagerUIDelegate& operator=(const ChromeFileManagerUIDelegate&) =
      delete;

  // Fetches a map that maps message IDs to actual strings shown to the user.
  // Extends the map with properties used by the files app, such as which
  // features are enabled. Returns the populated map to the caller.
  base::Value::Dict GetLoadTimeData() const override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
