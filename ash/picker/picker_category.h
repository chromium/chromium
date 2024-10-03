// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CATEGORY_H_
#define ASH_PICKER_PICKER_CATEGORY_H_

#include "ash/ash_export.h"

namespace ash {

// A category specifies a type of data that can be searched for.
enum class ASH_EXPORT PickerCategory {
  // Editor categories:
  kEditorWrite,
  kEditorRewrite,
  kLobster,
  // General categories:
  kLinks,
  kEmojisGifs,
  kEmojis,
  kClipboard,
  kDriveFiles,
  kLocalFiles,
  // Calculation categories:
  kDatesTimes,
  kUnitsMaths,
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_CATEGORY_H_
