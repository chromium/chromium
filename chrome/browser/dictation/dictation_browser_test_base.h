// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_DICTATION_DICTATION_BROWSER_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/test/base/platform_browser_test.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace dictation {

class DictationKeyedService;
class ListenerStreamProvider;
class SessionController;
struct TargetId;

// Base class for browser tests with common settings and setup.
class DictationBrowserTestBase : public PlatformBrowserTest {
 public:
  DictationBrowserTestBase();
  ~DictationBrowserTestBase() override;

  // PlatformBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  Profile* profile();
  content::WebContents* web_contents();
  DictationKeyedService& dictation_service();
  SessionController* session_controller();
  ListenerStreamProvider* attached_stream();

  // Starts a session for the given target.
  void StartSession(const TargetId& target_id);
  // Starts a session for the focused editable.
  void StartSession();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_BROWSER_TEST_BASE_H_
