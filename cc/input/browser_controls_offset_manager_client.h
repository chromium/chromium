// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_

namespace cc {

class CC_EXPORT BrowserControlsOffsetManagerClient {
 public:
  virtual float TopControlsHeight() const = 0;
  virtual float BottomControlsHeight() const = 0;
  virtual void SetCurrentBrowserControlsShownRatio(float ratio) = 0;
  virtual float CurrentBrowserControlsShownRatio() const = 0;
  virtual void DidChangeBrowserControlsPosition() = 0;
  virtual bool HaveRootScrollNode() const = 0;
  virtual void SetNeedsCommit() = 0;

 protected:
  virtual ~BrowserControlsOffsetManagerClient() {}
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_
