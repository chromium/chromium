// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_CONTROLLER_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_CONTROLLER_H_

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// This class is owned by GlicBackgroundModeManager and is responsible for
// showing/hiding the glic UI when the status icon is clicked or the appropriate
// menu item is selected.
class GlicController {
 public:
  GlicController();
  virtual ~GlicController();
  GlicController(const GlicController&) = delete;
  GlicController& operator=(const GlicController&) = delete;

  // Toggles the glic UI.
  virtual void Toggle(mojom::InvocationSource source);

  // Shows the glic UI.
  virtual void Show(mojom::InvocationSource source);

  // Forcibly closes glic UI.
  virtual void Close();

  // Returns true if the glic UI is visible.
  virtual bool IsShowing() const;

 private:
  // Helper that implements both Toggle and Show.
  void ToggleUI(bool prevent_close, mojom::InvocationSource source);
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_CONTROLLER_H_
