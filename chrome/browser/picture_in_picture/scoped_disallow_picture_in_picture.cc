// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/scoped_disallow_picture_in_picture.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

ScopedDisallowPictureInPicture::ScopedDisallowPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()
      ->OnScopedDisallowPictureInPictureCreated(
          base::PassKey<ScopedDisallowPictureInPicture>());
}

ScopedDisallowPictureInPicture::~ScopedDisallowPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()
      ->OnScopedDisallowPictureInPictureDestroyed(
          base::PassKey<ScopedDisallowPictureInPicture>());
}
