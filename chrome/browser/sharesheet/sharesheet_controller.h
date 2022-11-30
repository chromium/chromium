// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"

namespace sharesheet {

// The SharesheetController allows ShareActions to request changes to the state
// of the sharesheet.
class SharesheetController {
 public:
  virtual ~SharesheetController() = default;

  // When called will set the bubble size to |width| and |height|.
  // |width| and |height| must be set to a positive int.
  virtual void SetBubbleSize(int width, int height) = 0;

  // Called by ShareAction to notify SharesheetBubbleView to close.
  // |result| indicates whether the share was successful, cancelled or closed
  // due to an error.
  virtual void CloseBubble(SharesheetResult result) = 0;

  // Returns whether the bubble is visible.
  virtual bool IsBubbleVisible() const = 0;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_CONTROLLER_H_
