// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace chromeos {

struct LoginScreenExtensionUiCreateOptions {
  LoginScreenExtensionUiCreateOptions(const std::string& extension_name,
                                      const GURL& content_url,
                                      bool can_be_closed_by_user,
                                      base::OnceClosure close_callback);
  ~LoginScreenExtensionUiCreateOptions();

  const std::string extension_name;
  const GURL content_url;
  bool can_be_closed_by_user;
  base::OnceClosure close_callback;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
