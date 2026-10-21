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

  void SelectFile(const base::FilePath& downloads_path,
                  base::OnceCallback<void(const base::FilePath&)> selected_cb,
                  base::OnceClosure canceled_cb) override;

  void SetFilePath(const base::FilePath& file_path);

  // Returns the `downloads_path` passed to the last SelectFile() call.
  const base::FilePath& downloads_path() const { return downloads_path_; }

 private:
  base::FilePath file_path_;
  base::FilePath downloads_path_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_FAKE_USER_IMAGE_FILE_SELECTOR_H_
