// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_FAKE_USER_IMAGE_FILE_SELECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_FAKE_USER_IMAGE_FILE_SELECTOR_H_

#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"

#include "base/functional/callback.h"
#include "content/public/browser/web_ui.h"

namespace ash {

class FakeUserImageFileSelector : public UserImageFileSelector {
 public:
  explicit FakeUserImageFileSelector(content::WebUI* web_ui);

  FakeUserImageFileSelector(const FakeUserImageFileSelector&) = delete;
  FakeUserImageFileSelector& operator=(const FakeUserImageFileSelector&) =
      delete;

  ~FakeUserImageFileSelector() override;

  void SelectFile(base::OnceCallback<void(const base::FilePath&)> selected_cb,
                  base::OnceCallback<void(void)> canceled_cb) override;

  void SetFilePath(const base::FilePath& file_path);

 private:
  base::FilePath file_path_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_FAKE_USER_IMAGE_FILE_SELECTOR_H_
