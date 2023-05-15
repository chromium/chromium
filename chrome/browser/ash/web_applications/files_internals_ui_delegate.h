// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_

#include "ash/webui/files_internals/files_internals_ui_delegate.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebUI;
}  // namespace content

// Chrome browser FilesInternalsUIDelegate implementation.
class ChromeFilesInternalsUIDelegate : public ash::FilesInternalsUIDelegate {
 public:
  explicit ChromeFilesInternalsUIDelegate(content::WebUI* web_ui);
  ChromeFilesInternalsUIDelegate(const ChromeFilesInternalsUIDelegate&) =
      delete;
  ChromeFilesInternalsUIDelegate& operator=(
      const ChromeFilesInternalsUIDelegate&) = delete;
  ~ChromeFilesInternalsUIDelegate() override;

  base::Value GetDebugJSON() const override;

  bool GetSmbfsEnableVerboseLogging() const override;
  void SetSmbfsEnableVerboseLogging(bool enabled) override;

  std::string GetOfficeFileHandlers() const override;
  void ClearOfficeFileHandlers() override;

  bool GetMoveConfirmationShownForDrive() const override;
  bool GetMoveConfirmationShownForOneDrive() const override;

  bool GetMoveConfirmationShownForLocalToDrive() const override;
  bool GetMoveConfirmationShownForLocalToOneDrive() const override;
  bool GetMoveConfirmationShownForCloudToDrive() const override;
  bool GetMoveConfirmationShownForCloudToOneDrive() const override;

  bool GetAlwaysMoveOfficeFilesToDrive() const override;
  void SetAlwaysMoveOfficeFilesToDrive(bool always_move) override;

  bool GetAlwaysMoveOfficeFilesToOneDrive() const override;
  void SetAlwaysMoveOfficeFilesToOneDrive(bool always_move) override;

 private:
  raw_ptr<content::WebUI> web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILES_INTERNALS_UI_DELEGATE_H_
