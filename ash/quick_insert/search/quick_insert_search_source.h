// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_SOURCE_H_
#define ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_SOURCE_H_

namespace ash {

enum class QuickInsertSearchSource {
  kOmnibox = 0,
  kDate,
  kAction,
  kLocalFile,
  kDrive,
  kMath,
  kClipboard,
  kEditorWrite,
  kEditorRewrite,
  kLobsterWithNoSelectedText,
  kLobsterWithSelectedText,
  kMaxValue = kLobsterWithSelectedText,
};
}

#endif  // ASH_QUICK_INSERT_SEARCH_QUICK_INSERT_SEARCH_SOURCE_H_
