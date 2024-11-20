// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/glic/glic_window_controller.h"

class GlicWindowController;
class Profile;

// GlicWindowManager is a singleton that guarantees that only one panel will be
// present per Chrome instance regardless of the number of windows, tabs,
// profiles, etc. In addition to guaranteeing one panel at a time, it is able to
// choose a profile for launching the window as well as show the Profile Picker,
// if needed.
class GlicWindowManager {
 public:
  // Returns the singleton instance.
  static GlicWindowManager* GetInstance();

  GlicWindowManager(const GlicWindowManager&) = delete;
  GlicWindowManager& operator=(const GlicWindowManager&) = delete;

  // Create a Glic window for profile, if there is any.
  void ShowGlicWindowForProfile(Profile* profile);

  // Close the Glic window, if one exists.
  void CloseGlicWindow();

 private:
  friend struct base::DefaultSingletonTraits<GlicWindowManager>;

  GlicWindowManager();
  ~GlicWindowManager();

  base::WeakPtr<GlicWindowController> glic_window_controller_;
};

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_MANAGER_H_
