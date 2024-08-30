// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_feedback.h"

#include "base/test/gtest_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feedback/feedback_constants.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockUploader : public feedback::FeedbackUploader {
 public:
  using OnReportSentCallback =
      base::RepeatingCallback<void(userfeedback::ExtensionSubmit)>;

  explicit MockUploader(OnReportSentCallback on_report_sent)
      : FeedbackUploader(
            /*is_off_the_record=*/false,
            /*state_path=*/{},
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        on_report_sent_(std::move(on_report_sent)) {}

  MockUploader(const MockUploader&) = delete;
  MockUploader& operator=(const MockUploader&) = delete;

  // feedback::FeedbackUploader:
  void QueueReport(std::unique_ptr<std::string> data,
                   bool has_email,
                   int product_id) override {
    if (data != nullptr) {
      userfeedback::ExtensionSubmit feedback_data;
      feedback_data.ParseFromString(*data);
      on_report_sent_.Run(feedback_data);
    }
  }

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  OnReportSentCallback on_report_sent_;
  base::WeakPtrFactory<MockUploader> weak_ptr_factory_{this};
};

std::unique_ptr<KeyedService> CreateMockUploader(
    MockUploader::OnReportSentCallback on_report_sent,
    content::BrowserContext* context) {
  return std::make_unique<MockUploader>(std::move(on_report_sent));
}

std::unique_ptr<TestingProfile> CreateTestingProfile(
    const std::string& email,
    MockUploader::OnReportSentCallback on_report_sent_callback) {
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder()
          .AddTestingFactory(
              feedback::FeedbackUploaderFactoryChrome::GetInstance(),
              base::BindRepeating(CreateMockUploader,
                                  std::move(on_report_sent_callback)))
          .Build();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);
  return profile;
}

TEST(LobsterFeedback, SendFeedbackDoesNotSendEmail) {
  content::BrowserTaskEnvironment task_environment;
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@email.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(
      SendLobsterFeedback(profile.get(),
                          /*query=*/"",
                          /*model_version=*/"",
                          /*user_description=*/
                          "visit https://www.whatismyip.com/, log in using "
                          "test@email.com and try entering 111.222.333.444",
                          /*image_bytes=*/"a1b2c3"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();
  EXPECT_EQ(feedback_data.common_data().user_email(), "");
}

TEST(LobsterFeedback, SendFeedbackRedactsDescription) {
  content::BrowserTaskEnvironment task_environment;
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@email.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(
      SendLobsterFeedback(profile.get(),
                          /*query=*/"",
                          /*model_version=*/"",
                          /*user_description=*/
                          "visit https://www.whatismyip.com/ log in using "
                          "test@email.com and try entering 111.222.333.444",
                          /*image_bytes=*/"a1b2c3"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();
  EXPECT_EQ(feedback_data.common_data().description(),
            "model_input: \nmodel_version: \nuser_description: visit (URL: 1) "
            "log in using (email: 1) and try entering 111.222.333.444");
}

TEST(LobsterFeedback, SendFeedbackOnlyContainsNecessaryInformation) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      "CHROMEOS_RELEASE_VERSION=42", base::Time::Now());
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@google.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(SendLobsterFeedback(profile.get(), /*query=*/"a dummy query",
                                  /*model_version=*/"dummy_version",
                                  /*user_description=*/
                                  "some dummy description",
                                  /*image_bytes=*/"a1b2c3"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();

  userfeedback::ExtensionSubmit expected_feedback_data;
  expected_feedback_data.set_product_id(feedback::kLobsterFeedbackProductId);
  expected_feedback_data.set_type_id(0);
  expected_feedback_data.mutable_common_data()->set_gaia_id(0);
  expected_feedback_data.mutable_common_data()->set_user_email("");
  expected_feedback_data.mutable_common_data()->set_description(
      "model_input: a dummy query\nmodel_version: "
      "dummy_version\nuser_description: some dummy description");
  expected_feedback_data.mutable_common_data()->set_source_description_language(
      "");
  expected_feedback_data.mutable_web_data()
      ->mutable_navigator()
      ->set_user_agent("");
  userfeedback::ProductSpecificData chrome_version_data;
  chrome_version_data.set_key("CHROME VERSION");
  chrome_version_data.set_value(
      chrome::GetVersionString(chrome::WithExtendedStable(true)));
  userfeedback::ProductSpecificData chromeos_version_data;
  chromeos_version_data.set_key("CHROMEOS_RELEASE_VERSION");
  chromeos_version_data.set_value("42");
  *expected_feedback_data.mutable_web_data()->add_product_specific_data() =
      chrome_version_data;
  *expected_feedback_data.mutable_web_data()->add_product_specific_data() =
      chromeos_version_data;
  userfeedback::PostedScreenshot screenshot;
  screenshot.set_binary_content("a1b2c3");
  screenshot.set_mime_type("image/png");
  screenshot.mutable_dimensions()->set_width(0);
  screenshot.mutable_dimensions()->set_height(0);
  *expected_feedback_data.mutable_screenshot() = screenshot;

  EXPECT_THAT(feedback_data, base::test::EqualsProto(expected_feedback_data));
}

}  // namespace
