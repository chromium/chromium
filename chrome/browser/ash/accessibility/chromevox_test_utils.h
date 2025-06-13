// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

namespace ash {

namespace test {
class SpeechMonitor;
}  // namespace test

// A class that can be used to exercise ChromeVox in browsertests.
class ChromeVoxTestUtils {
 public:
  explicit ChromeVoxTestUtils(Profile* profile);
  ~ChromeVoxTestUtils();
  ChromeVoxTestUtils(const ChromeVoxTestUtils&) = delete;
  ChromeVoxTestUtils& operator=(const ChromeVoxTestUtils&) = delete;

  // Enables and sets up ChromeVox.
  void EnableChromeVox(bool check_for_intro = true);
  // Exposes the module, specified by `name`, on the `globalThis` object. This
  // allows tests to call directly into various ChromeVox methods.
  void GlobalizeModule(const std::string& name);
  void DisableEarcons();

  // Runs the given script in the ChromeVox background page.
  void RunJS(const std::string& script);

  test::SpeechMonitor* sm() { return sm_.get(); }

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<test::SpeechMonitor> sm_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_
