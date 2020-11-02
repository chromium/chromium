// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_

#include "chromeos/components/file_manager/file_manager_ui_delegate.h"

#include <memory>

#include "base/values.h"

/**
 * Implementation of the FileManagerUIDelegate interface. Provides the file
 * manager code in //chromeos with functions that only exist in //chrome.
 */
class ChromeFileManagerUIDelegate : public FileManagerUIDelegate {
 public:
  ChromeFileManagerUIDelegate();

  ChromeFileManagerUIDelegate(const ChromeFileManagerUIDelegate&) = delete;
  ChromeFileManagerUIDelegate& operator=(const ChromeFileManagerUIDelegate&) =
      delete;

  // Returns a map from message labels to actual messages used by Files app.
  std::unique_ptr<base::DictionaryValue> GetFileManagerAppStrings()
      const override;
};

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_FILE_MANAGER_UI_DELEGATE_H_
