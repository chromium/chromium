// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASIC_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASIC_H_

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_base.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

// Handles print commands, but as a simplified basic version.
class PrintViewManagerCrosBasic
    : public PrintViewManagerCrosBase,
      public content::WebContentsUserData<PrintViewManagerCrosBasic> {
 public:
  PrintViewManagerCrosBasic(const PrintViewManagerCrosBasic&) = delete;
  PrintViewManagerCrosBasic& operator=(const PrintViewManagerCrosBasic&) =
      delete;

  ~PrintViewManagerCrosBasic() override = default;

 private:
  friend class content::WebContentsUserData<PrintViewManagerCrosBasic>;

  explicit PrintViewManagerCrosBasic(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_BASIC_H_
