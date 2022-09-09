// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/fake_user_image_file_selector.h"

namespace ash {

FakeUserImageFileSelector::FakeUserImageFileSelector(content::WebUI* web_ui)
    : UserImageFileSelector(web_ui) {}

FakeUserImageFileSelector::~FakeUserImageFileSelector() {}

void FakeUserImageFileSelector::SelectFile(
    base::OnceCallback<void(const base::FilePath&)> selected_cb,
    base::OnceCallback<void(void)> canceled_cb) {
  std::move(selected_cb).Run(file_path_);
}

void FakeUserImageFileSelector::SetFilePath(const base::FilePath& file_path) {
  file_path_ = file_path;
}

}  // namespace ash
