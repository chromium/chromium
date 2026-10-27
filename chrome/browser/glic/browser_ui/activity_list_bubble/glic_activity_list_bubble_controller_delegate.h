// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_DELEGATE_H_

namespace glic {

// Interface through which GlicActivityListBubbleController communicates with
// its UI.
class GlicActivityListBubbleControllerDelegate {
 public:
  virtual ~GlicActivityListBubbleControllerDelegate() = default;

  // Show the activity list bubble anchored to the button.
  virtual void ShowActivityListBubble() = 0;

  // Hide the activity list bubble.
  virtual void CloseActivityListBubble() = 0;

  // Returns true if the activity list bubble is showing.
  virtual bool IsActivityListBubbleShowing() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_DELEGATE_H_
