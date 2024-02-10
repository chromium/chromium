// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/image/image_skia.h"

namespace app_restore {
class RestoreData;
}

namespace ash {

// Various data needed to populate the pine dialog.
struct ASH_EXPORT PineContentsData {
 public:
  PineContentsData();
  PineContentsData(const PineContentsData&) = delete;
  PineContentsData& operator=(const PineContentsData&) = delete;
  ~PineContentsData();

  // Image read from the pine image file. Will be null if pine image file was
  // missing or decoding failed.
  gfx::ImageSkia image;

  // Contains the app data needed to show app titles, app icons, favicons, etc.
  // Read from the full restore file.
  // TODO(sammiequon): Use a subset of `app_restore::RestoreData` here instead
  // as it contains a lot of unnecessary information.
  std::unique_ptr<app_restore::RestoreData> restore_data;

  // TODO(sammiequon): Add ok/cancel callbacks.
  // TODO(sammiequon): Add dialog type (crash, update, normal).
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTENTS_DATA_H_
