// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_

#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

// Used for attachingSelectFileDialogExtension's routing ID to its WebContents.
class SelectFileDialogExtensionUserData : public base::SupportsUserData::Data {
 public:
  static void SetRoutingIdForWebContents(content::WebContents* web_contents,
                                         const std::string& routing_id);
  static std::string GetRoutingIdForWebContents(
      content::WebContents* web_contents);

 private:
  explicit SelectFileDialogExtensionUserData(const std::string& routing_id);

  const std::string& routing_id() const { return routing_id_; }

  std::string routing_id_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogExtensionUserData);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
