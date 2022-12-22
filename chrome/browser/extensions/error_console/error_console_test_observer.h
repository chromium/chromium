// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_TEST_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_TEST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/error_console/error_console.h"

class Profile;

namespace extensions {

// A helper class to listen for extension errors being emitted in tests.
class ErrorConsoleTestObserver : public ErrorConsole::Observer {
 public:
  ErrorConsoleTestObserver(size_t errors_expected, Profile* profile);
  ~ErrorConsoleTestObserver();

  // Enables error collection for the associated profile.
  void EnableErrorCollection();

  // Waits until at least `errors_expected` errors have been seen.
  void WaitForErrors();

 private:
  // ErrorConsole::Observer implementation.
  void OnErrorAdded(const ExtensionError* error) override;

  size_t errors_expected_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  size_t errors_observed_ = 0;
  base::ScopedObservation<ErrorConsole, ErrorConsole::Observer> observation_{
      this};
  base::RunLoop run_loop_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_ERROR_CONSOLE_TEST_OBSERVER_H_
