// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TTC_CORE_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_TTC_CORE_TTC_CORE_BROWSER_TEST_BASE_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/platform_browser_test.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace ttc {

class Conversation;
class MockConversation;
class TtcKeyedService;

// Base class for Ttc core browser tests with common settings and setup.
// Installs a Conversation factory that creates MockConversations, so tests can
// set expectations on the conversation belonging to the current session.
class TtcCoreBrowserTestBase : public PlatformBrowserTest {
 public:
  TtcCoreBrowserTestBase();
  ~TtcCoreBrowserTestBase() override;

  // PlatformBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  Profile* profile();
  content::WebContents* web_contents();
  TtcKeyedService& ttc_service();

  // The conversation belonging to the service's current session, or null if
  // there's no session or it has no conversation.
  MockConversation* conversation();

 private:
  static std::unique_ptr<Conversation> MakeMockConversation(Profile* profile);

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TTC_CORE_BROWSER_TEST_BASE_H_
