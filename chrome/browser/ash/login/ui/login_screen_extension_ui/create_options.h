// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace ash {
namespace login_screen_extension_ui {

struct CreateOptions {
  CreateOptions(const std::string& extension_name,
                const GURL& content_url,
                bool can_be_closed_by_user,
                base::OnceClosure close_callback);

  CreateOptions(const CreateOptions&) = delete;
  CreateOptions& operator=(const CreateOptions&) = delete;

  ~CreateOptions();

  const std::string extension_name;
  const GURL content_url;
  bool can_be_closed_by_user;
  base::OnceClosure close_callback;
};

}  // namespace login_screen_extension_ui
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace login_screen_extension_ui {
using ::ash::login_screen_extension_ui::CreateOptions;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_CREATE_OPTIONS_H_
