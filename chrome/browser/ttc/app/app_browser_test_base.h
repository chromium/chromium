// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/platform_browser_test.h"

class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace media {
class MockAudioManager;
}

namespace ttc {

class Conversation;
class ConversationImpl;
class MockTtcBackend;
class SessionController;
class TtcKeyedService;

// Base class for Ttc app browser tests with common settings and setup.
// Installs a TtcKeyedService whose sessions use a real ConversationImpl talking
// to a MockTtcBackend, and routes audio through a MockAudioManager so tests
// don't touch real audio devices.
class AppBrowserTestBase : public PlatformBrowserTest {
 public:
  AppBrowserTestBase();
  ~AppBrowserTestBase() override;

  // PlatformBrowserTest:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  Profile* profile();
  content::WebContents* web_contents();
  TtcKeyedService& ttc_service();

  // The conversation belonging to the service's current session, or null if
  // there's no session.
  ConversationImpl* conversation();

  // The backend of the current session's conversation, or null if there's no
  // session.
  MockTtcBackend* backend();

 private:
  std::unique_ptr<Conversation> MakeConversation(
      SessionController& session_controller);

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<media::MockAudioManager> audio_manager_;

  // Owned by the session's SessionController so it becomes dangling when the
  // session ends. Use conversation() which accounts for this.
  raw_ptr<ConversationImpl, DisableDanglingPtrDetection> conversation_ =
      nullptr;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_APP_BROWSER_TEST_BASE_H_
