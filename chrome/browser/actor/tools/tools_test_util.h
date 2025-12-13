// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
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
  void GetCredentials(
      tabs::TabInterface* tab,
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      actor_login::CredentialsOrErrorReply callback) override;
  void AttemptLogin(
      tabs::TabInterface* tab,
      const actor_login::Credential& credential,
      bool should_store_permission,
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      actor_login::LoginStatusResultOrErrorReply callback) override;

  void SetCredentials(const actor_login::CredentialsOrError& credentials);

  void SetCredential(const actor_login::Credential& credential);

  void SetLoginStatus(actor_login::LoginStatusResultOrError login_status);

  const std::optional<actor_login::Credential>& last_credential_used() const;
  bool last_permission_was_permanent() const;

 private:
  actor_login::CredentialsOrError credentials_;
  actor_login::LoginStatusResultOrError login_status_;
  std::optional<actor_login::Credential> last_credential_used_;
  bool last_permission_was_permanent_ = false;
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

  void GetPageApc();

 protected:
  virtual std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile);

  // Returns true if actuation should always be enabled for the test (regardless
  // of policy / opt-in status).
  virtual bool ShouldForceActOnWeb();

  TaskId CreateNewTask();

  void SetPageContent(
      base::OnceClosure quit_closure,
      optimization_guide::AIPageContentResultOrError page_content);

  TaskId task_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_for_init_;
  base::ScopedTempDir temp_dir_;
};

gfx::RectF GetBoundingClientRect(content::RenderFrameHost& rfh,
                                 std::string_view query);

std::string DescribePaintStabilityMode(features::ActorPaintStabilityMode mode);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOLS_TEST_UTIL_H_
