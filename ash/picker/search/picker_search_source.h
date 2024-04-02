// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_SEARCH_PICKER_SEARCH_SOURCE_H_
#define ASH_PICKER_SEARCH_PICKER_SEARCH_SOURCE_H_

namespace ash {

enum class PickerSearchSource {
  kOmnibox,
  kTenor,  // Used for GIF search.
  kEmoji,
  kDate,
  kCategory,
  kLocalFile,
  kDrive,
  kMath,
  kClipboard,
};
}

#endif  // ASH_PICKER_SEARCH_PICKER_SEARCH_SOURCE_H_
