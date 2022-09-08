// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_

namespace gfx {
class PointF;
}

namespace cc {

class CC_EXPORT BrowserControlsOffsetManagerClient {
 public:
  virtual float TopControlsHeight() const = 0;
  virtual float TopControlsMinHeight() const = 0;
  virtual float BottomControlsHeight() const = 0;
  virtual float BottomControlsMinHeight() const = 0;
  virtual void SetCurrentBrowserControlsShownRatio(float top_ratio,
                                                   float bottom_ratio) = 0;
  virtual float CurrentTopControlsShownRatio() const = 0;
  virtual float CurrentBottomControlsShownRatio() const = 0;
  virtual gfx::PointF ViewportScrollOffset() const = 0;
  virtual void DidChangeBrowserControlsPosition() = 0;
  virtual bool OnlyExpandTopControlsAtPageTop() const = 0;
  virtual bool HaveRootScrollNode() const = 0;
  virtual void SetNeedsCommit() = 0;

 protected:
  virtual ~BrowserControlsOffsetManagerClient() {}
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_CLIENT_H_
