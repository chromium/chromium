// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/mock_user_image_loader_delegate.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "components/user_manager/user_image/user_image.h"

namespace ash::test {

namespace {

std::unique_ptr<user_manager::UserImage> MakeUserImage() {
  return std::make_unique<user_manager::UserImage>();
}

}  // namespace

MockUserImageLoaderDelegate::MockUserImageLoaderDelegate() {
  ON_CALL(*this, FromGURLAnimated)
      .WillByDefault([](const GURL& default_image_url,
                        user_image_loader::LoadedCallback loaded_cb) {
        base::SequencedTaskRunner::GetCurrentDefault()
            ->PostTaskAndReplyWithResult(FROM_HERE,
                                         base::BindOnce(&MakeUserImage),
                                         std::move(loaded_cb));
      });
}

MockUserImageLoaderDelegate::~MockUserImageLoaderDelegate() = default;

}  // namespace ash::test
