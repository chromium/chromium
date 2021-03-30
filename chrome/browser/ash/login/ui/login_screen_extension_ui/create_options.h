// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace chromeos {

namespace login_screen_extension_ui {

struct CreateOptions {
  CreateOptions(const std::string& extension_name,
                const GURL& content_url,
                bool can_be_closed_by_user,
                base::OnceClosure close_callback);
  ~CreateOptions();

  const std::string extension_name;
  const GURL content_url;
  bool can_be_closed_by_user;
  base::OnceClosure close_callback;

  DISALLOW_COPY_AND_ASSIGN(CreateOptions);
};

}  // namespace login_screen_extension_ui

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
