// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_

#include "ash/webui/files_internals/files_internals_ui_delegate.h"

// Chrome browser FilesInternalsUIDelegate implementation.
class ChromeFilesInternalsUIDelegate : public ash::FilesInternalsUIDelegate {
 public:
  ChromeFilesInternalsUIDelegate();

  ChromeFilesInternalsUIDelegate(const ChromeFilesInternalsUIDelegate&) =
      delete;
  ChromeFilesInternalsUIDelegate& operator=(
      const ChromeFilesInternalsUIDelegate&) = delete;

  base::Value GetDebugJSON() const override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_
