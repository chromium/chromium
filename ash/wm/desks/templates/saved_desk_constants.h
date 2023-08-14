// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONSTANTS_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONSTANTS_H_

namespace ash {

// The common spacing used in save desk views.
// Used in space between icon views, and space between the textfield and the
// managed status icon.
constexpr int kSaveDeskSpacingDp = 8;

// The common corner radius used in save desk view.
// Used as corner radius for SavedDeskItemView, NoItemsLabel,
// SavedDeskSaveDeskButton.
constexpr int kSaveDeskCornerRadius = 16;

// The common padding used in save desk view.
// Used in desk grid padding, SavedDeskItemView horizontal padding.
constexpr int kSaveDeskPaddingDp = 24;

// Windows launched from admin templates or floating workspace templates will
// start with this activation index, and then proceed downwards. In other words,
// all windows launched from admin templates or floating workspace templates
// will have an activation index that is equal to or lower than this value.
constexpr int kTemplateStartingActivationIndex = -1000000;

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_CONSTANTS_H_
