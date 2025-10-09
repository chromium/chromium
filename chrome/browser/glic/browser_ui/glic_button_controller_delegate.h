// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_DELEGATE_H_

namespace glic {

// Delegate class for the glic button controller to update
// the properties of the button like visibility, icon.
class GlicButtonControllerDelegate {
 public:
  virtual ~GlicButtonControllerDelegate() = default;

  // Set the show state of the button
  virtual void SetGlicShowState(bool show) = 0;

  // Update the glic button when attachment changes.
  virtual void SetGlicDetached(bool detached) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BUTTON_CONTROLLER_DELEGATE_H_
