// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_

#include <string>

class Profile;

namespace aura {
class Window;
}

namespace borealis {

class BorealisWindowManager {
 public:
  // Returns true if this window belongs to a borealis VM (based on its app_id
  // and startup_id).
  static bool IsBorealisWindow(aura::Window* window);

  explicit BorealisWindowManager(Profile* profile);

  std::string GetShelfAppId(aura::Window* window);

 private:
  Profile* const profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_WINDOW_MANAGER_H_
