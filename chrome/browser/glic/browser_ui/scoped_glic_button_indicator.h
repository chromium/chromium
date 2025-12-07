// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_SCOPED_GLIC_BUTTON_INDICATOR_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_SCOPED_GLIC_BUTTON_INDICATOR_H_

#include "base/memory/raw_ptr.h"

namespace glic {

class GlicButton;

// Helper class to temporarily set the indicator status of a glic button.
class ScopedGlicButtonIndicator {
 public:
  explicit ScopedGlicButtonIndicator(GlicButton* glic_button);
  ScopedGlicButtonIndicator(const ScopedGlicButtonIndicator&) = delete;
  ScopedGlicButtonIndicator& operator=(const ScopedGlicButtonIndicator&) =
      delete;
  ~ScopedGlicButtonIndicator();
  GlicButton* GetGlicButton() { return glic_button_; }

 private:
  raw_ptr<GlicButton> glic_button_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_SCOPED_GLIC_BUTTON_INDICATOR_H_
