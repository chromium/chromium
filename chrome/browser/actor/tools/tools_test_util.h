// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class WebContents;
}

namespace gfx {
class RectF;
}

namespace tabs {
class TabInterface;
}

namespace actor {

actor_login::Credential MakeTestCredential(const std::u16string& username,
                                           const GURL& url,
                                           bool immediately_available_to_login);

class MockActorLoginService : public actor_login::ActorLoginService {
 public:
  MockActorLoginService();
  ~MockActorLoginService() override;

  // `actor_login::ActorLoginService`:
  void GetCredentials(tabs::TabInterface* tab,
                      actor_login::CredentialsOrErrorReply callback) override;
  void AttemptLogin(
      tabs::TabInterface* tab,
      const actor_login::Credential& credential,
      bool should_store_permission,
      actor_login::LoginStatusResultOrErrorReply callback) override;

  void SetCredentials(const actor_login::CredentialsOrError& credentials);

  void SetCredential(const actor_login::Credential& credential);

  void SetLoginStatus(actor_login::LoginStatusResultOrError login_status);

  const std::optional<actor_login::Credential>& last_credential_used() const;

 private:
  actor_login::CredentialsOrError credentials_;
  actor_login::LoginStatusResultOrError login_status_;
  std::optional<actor_login::Credential> last_credential_used_;
};

inline constexpr int32_t kNonExistentContentNodeId =
    std::numeric_limits<int32_t>::max();

class ActorToolsTest : public InProcessBrowserTest {
 public:
  ActorToolsTest();
  ActorToolsTest(const ActorToolsTest&) = delete;
  ActorToolsTest& operator=(const ActorToolsTest&) = delete;
  ~ActorToolsTest() override;

  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  void GoBack();
  void TinyWait();

  content::WebContents* web_contents();
  tabs::TabInterface* active_tab();
  content::RenderFrameHost* main_frame();
  ExecutionEngine& execution_engine();
  ActorTask& actor_task() const;

 protected:
  virtual std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile);

  TaskId task_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_for_init_;
  base::ScopedTempDir temp_dir_;
};

class ActorToolsGeneralPageStabilityTest
    : public ActorToolsTest,
      public ::testing::WithParamInterface<
          ::features::ActorGeneralPageStabilityMode> {
 public:
  static std::string DescribeParam(
      const testing::TestParamInfo<ParamType>& info);
  ActorToolsGeneralPageStabilityTest();
  ~ActorToolsGeneralPageStabilityTest() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

gfx::RectF GetBoundingClientRect(content::RenderFrameHost& rfh,
                                 std::string_view query);

std::string DescribeGeneralPageStabilityMode(
    features::ActorGeneralPageStabilityMode mode);

inline constexpr features::ActorGeneralPageStabilityMode
    kActorGeneralPageStabilityModeValues[] = {
        features::ActorGeneralPageStabilityMode::kDisabled,
        features::ActorGeneralPageStabilityMode::kAllEnabled,
};

std::string DescribePaintStabilityMode(features::ActorPaintStabilityMode mode);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
