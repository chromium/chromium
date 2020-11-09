// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::StrictMock;

class FakeClientSideDetectionService : public ClientSideDetectionService {
 public:
  FakeClientSideDetectionService() : ClientSideDetectionService(nullptr) {}

  void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      bool is_extended_reporting,
      bool is_enhanced_protection,
      const ClientReportPhishingRequestCallback& callback) override {
    saved_request_ = *verdict;
    saved_callback_ = callback;
    request_callback_.Run();
  }

  const ClientPhishingRequest& saved_request() { return saved_request_; }
  const ClientReportPhishingRequestCallback& saved_callback() {
    return saved_callback_;
  }

  void SetModel(const ClientSideModel& model) { model_ = model; }

  std::string GetModelStr() override { return model_.SerializeAsString(); }

  void SetRequestCallback(const base::RepeatingClosure& closure) {
    request_callback_ = closure;
  }

 private:
  ClientPhishingRequest saved_request_;
  ClientReportPhishingRequestCallback saved_callback_;
  ClientSideModel model_;
  base::RepeatingClosure request_callback_;
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager() : SafeBrowsingUIManager(nullptr) {}

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

 protected:
  ~MockSafeBrowsingUIManager() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

}  // namespace

class ClientSideDetectionHostBrowserTest : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostBrowserTest() = default;
  ~ClientSideDetectionHostBrowserTest() override = default;
};

#if BUILDFLAG(FULL_SAFE_BROWSING)
IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostBrowserTest,
                       VerifyVisualFeatureCollection) {
  FakeClientSideDetectionService fake_csd_service;

  ClientSideModel model;
  model.set_version(123);
  model.set_max_words_per_term(1);
  VisualTarget* target = model.mutable_vision_model()->add_targets();

  target->set_digest("target1_digest");
  // Create a hash corresponding to a blank screen.
  std::string hash = "\x30";
  for (int i = 0; i < 288; i++)
    hash += "\xff";
  target->set_hash(hash);
  target->set_dimension_size(48);
  MatchRule* match_rule = target->mutable_match_config()->add_match_rule();
  // The actual hash distance is 76, so set the distance to 100 for safety.
  match_rule->set_hash_distance(100);

  fake_csd_service.SetModel(model);

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ClientSideDetectionHost::Create(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(&fake_csd_service);
  csd_host->SendModelToRenderFrame();
  csd_host->set_ui_manager(mock_ui_manager.get());

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback().is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);
  ASSERT_EQ(fake_csd_service.saved_request().vision_match_size(), 1);
  EXPECT_EQ(
      fake_csd_service.saved_request().vision_match(0).matched_target_digest(),
      "target1_digest");

  // Expect an interstitail to be shown
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  fake_csd_service.saved_callback().Run(page_url, true);
}
#endif

}  // namespace safe_browsing
