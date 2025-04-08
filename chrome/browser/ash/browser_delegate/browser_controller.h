// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_

#include <string_view>

#include "base/containers/span.h"

class GURL;

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

class BrowserDelegate;

// BrowserController is a singleton created by
// ChromeBrowserMainExtraPartsAsh::PostProfileInit. See also README.md.
class BrowserController {
 public:
  static BrowserController* GetInstance();

  // Makes a POST request in a new tab in the last active tabbed browser. If no
  // such browser exists, a new one is created. Returns nullptr if the creation
  // is not possible for the given arguments.
  // This is needed by the Media app.
  virtual BrowserDelegate* NewTabWithPostData(
      user_manager::User& user,
      const GURL& url,
      base::span<const uint8_t> post_data,
      std::string_view extra_headers) = 0;

 protected:
  BrowserController();
  BrowserController(const BrowserController&) = delete;
  BrowserController& operator=(const BrowserController&) = delete;
  virtual ~BrowserController();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
