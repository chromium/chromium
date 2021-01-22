// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_

#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager_observer.h"

// Instantiate this class to get errors and warnings for an extension.
//
// Sample usage:
//
// ExtensionConsoleErrorObserver console_observer(browser_context,
// some_extension_id); LoadSomeExtension();
// ...
// EXPECT_FALSE(console_observer.HasErrorsOrWarnings());
class ExtensionConsoleErrorObserver
    : public extensions::ProcessManagerObserver {
 public:
  ExtensionConsoleErrorObserver(content::BrowserContext* context,
                                const char* extension_id);
  ~ExtensionConsoleErrorObserver() override;

  // Returns whether errors or warnings were received.
  bool HasErrorsOrWarnings();

  // A helper method to return the string content (in UTF8) of the error or
  // warning at the given |index|. This will cause a test failure if there is no
  // such message.
  std::string GetErrorOrWarningAt(size_t index) const;

  // Get the number of errors and warnings received.
  size_t GetErrorsAndWarningsCount() const;

  // extensions::ProcessManagerObserver:
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override;

 private:
  content::BrowserContext* context_;
  const char* extension_id_;
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;
};

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_TEST_UTILS_H_
