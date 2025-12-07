// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

namespace ash {

class ExtensionConsoleErrorObserver;

namespace test {
class SpeechMonitor;
}  // namespace test

// A class that can be used to exercise ChromeVox in browsertests.
class ChromeVoxTestUtils {
 public:
  ChromeVoxTestUtils();
  // TODO(crbug.com/388867840): Turn off ChromeVox below in the destructor or
  // introduce a separate method called DisableChromeVox.
  ~ChromeVoxTestUtils();
  ChromeVoxTestUtils(const ChromeVoxTestUtils&) = delete;
  ChromeVoxTestUtils& operator=(const ChromeVoxTestUtils&) = delete;

  // Enables and sets up ChromeVox. Set `check_for_speech` to true to wait for
  // the ChromeVox welcome message to be spoken. This helps ensure that
  // ChromeVox is in a stable state. Set `check_for_speech` to false to skip the
  // welcome message. This can be helpful if the associated test doesn't care
  // about the specific utterances being spoken.
  void EnableChromeVox(bool check_for_speech = true);
  // Exposes the module, specified by `name`, on the `globalThis` object. This
  // allows tests to call directly into various ChromeVox methods.
  void GlobalizeModule(const std::string& name);
  void DisableEarcons();
  void WaitForReady();
  void WaitForValidRange();

  void ExecuteCommandHandlerCommand(std::string command);

  // Runs the given script in the ChromeVox background page.
  void RunJS(const std::string& script);

  test::SpeechMonitor* sm() { return sm_.get(); }

 private:
  std::unique_ptr<test::SpeechMonitor> sm_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_TEST_UTILS_H_
