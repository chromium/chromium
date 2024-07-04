// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_feedback.h"

#include "ash/constants/ash_features.h"
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
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
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

TEST(EditorFeedback, SendFeedbackDoesNotSendEmail) {
  content::BrowserTaskEnvironment task_environment;
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@email.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(SendEditorFeedback(profile.get(), "test description"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();
  EXPECT_EQ(feedback_data.common_data().user_email(), "");
}

TEST(EditorFeedback, SendFeedbackRedactsDescription) {
  content::BrowserTaskEnvironment task_environment;
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@email.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(SendEditorFeedback(
      profile.get(), "http://www.google.com test@email.com 111.222.3.4"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();
  EXPECT_EQ(feedback_data.common_data().description(),
            "(URL: 1) (email: 1) (IPv4: 1)");
}

// A change-detector test to ensure that the feedback only contains allowed
// information.
TEST(EditorFeedback, SendFeedbackOnlyContainsNecessaryInformation) {
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      "CHROMEOS_RELEASE_VERSION=42", base::Time::Now());
  base::test::TestFuture<userfeedback::ExtensionSubmit> on_report_sent_future;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile(
      "test@google.com", on_report_sent_future.GetRepeatingCallback());

  EXPECT_TRUE(SendEditorFeedback(profile.get(), "test description"));

  auto feedback_data =
      on_report_sent_future.Get<userfeedback::ExtensionSubmit>();

  userfeedback::ExtensionSubmit expected_feedback_data;
  expected_feedback_data.set_product_id(5314436);
  expected_feedback_data.set_type_id(0);
  expected_feedback_data.mutable_common_data()->set_gaia_id(0);
  expected_feedback_data.mutable_common_data()->set_user_email("");
  expected_feedback_data.mutable_common_data()->set_description(
      "test description");
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

  EXPECT_THAT(feedback_data, base::test::EqualsProto(expected_feedback_data));
}

}  // namespace
}  // namespace ash::input_method
