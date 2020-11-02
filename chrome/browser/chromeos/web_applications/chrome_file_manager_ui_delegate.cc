// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_file_manager_ui_delegate.h"
#include "chrome/browser/chromeos/file_manager/file_manager_string_util.h"

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate() = default;

std::unique_ptr<base::DictionaryValue>
ChromeFileManagerUIDelegate::GetFileManagerAppStrings() const {
  return GetFileManagerStrings();
}
