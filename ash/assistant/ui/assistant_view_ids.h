// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_VIEW_IDS_H_
#define ASH_ASSISTANT_UI_ASSISTANT_VIEW_IDS_H_

namespace ash {

// IDs used for the main views that compose the Assistant UI.
// Use these for easy access to the views during the unittests.
// Note that these IDs are only guaranteed to be unique inside
// |AssistantPageView|.
enum AssistantViewID {
  // We start at 1 because 0 is not a valid view ID.
  kMainView = 1,

  // Dialog plate and its components.
  kDialogPlate,
  kKeyboardInputToggle,
  kMicView,
  kModuleIcon,
  kTextQueryField,
  kVoiceInputToggle,

  // Main stage and its components.
  kMainStage,
  kFooterView,
  kGreetingLabel,
  kOptInView,
  kProgressIndicator,
  kQueryView,
  kSuggestionContainer,
  kUiElementContainer,

  kWebView,
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_VIEW_IDS_H_
