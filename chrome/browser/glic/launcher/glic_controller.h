// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONTROLLER_H_

// This class is owned by GlicBackgroundModeManager and is responsible for
// showing/hiding the glic UI when the status icon is clicked or the appropriate
// menu item is selected.
class GlicController {
 public:
  GlicController();
  ~GlicController();
  GlicController(const GlicController&) = delete;
  GlicController& operator=(const GlicController&) = delete;

  // Shows the glic UI.
  void Show();

  // Hides the glic UI.
  void Hide();
};

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONTROLLER_H_
