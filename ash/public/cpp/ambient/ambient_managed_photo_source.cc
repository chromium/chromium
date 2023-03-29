// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ambient_managed_photo_source.h"

namespace ash {

namespace {

// Global pointer to the managed photo source, there can only every be one
// instance of this class.
AmbientManagedPhotoSource* managed_photo_source_ = nullptr;

}  // namespace

AmbientManagedPhotoSource::AmbientManagedPhotoSource() {
  CHECK(!managed_photo_source_);
  managed_photo_source_ = this;
}

AmbientManagedPhotoSource::~AmbientManagedPhotoSource() {
  CHECK_EQ(managed_photo_source_, this);
  managed_photo_source_ = nullptr;
}

AmbientManagedPhotoSource* AmbientManagedPhotoSource::Get() {
  return managed_photo_source_;
}

}  // namespace ash
