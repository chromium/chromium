// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal_processor.h"

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;

constexpr const char* kExtensionIds[] = {"aaaaaaaabbbbbbbbccccccccdddddddd",
                                         "eeeeeeeeffffffffgggggggghhhhhhhh",
                                         "aaaaeeeebbbbffffccccggggddddhhhh"};
constexpr const char* kUrls[] = {"http://www.example1.com/",
                                 "https://www.example2.com/"};

class DeclarativeNetRequestActionSignalProcessorTest : public ::testing::Test {
 protected:
  DeclarativeNetRequestActionSignalProcessorTest() = default;

  DeclarativeNetRequestActionSignalProcessor processor_;
};

TEST_F(DeclarativeNetRequestActionSignalProcessorTest,
       EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(DeclarativeNetRequestActionSignalProcessorTest, IgnoresInvalidSignal) {
  auto invalid_signal = DeclarativeNetRequestActionSignal::
      CreateDeclarativeNetRequestRedirectActionSignal(kExtensionIds[0],
                                                      /*request_url=*/GURL(),
                                                      /*redirect_url=*/GURL());

  processor_.ProcessSignal(*invalid_signal);

  // Verify that processor ignores the signal, i.e., it does not
  // have any data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(DeclarativeNetRequestActionSignalProcessorTest,
       StoresDataAfterProcessingSignal) {
  auto signal = DeclarativeNetRequestActionSignal::
      CreateDeclarativeNetRequestRedirectActionSignal(kExtensionIds[0],
                                                      /*request_url=*/GURL(),
                                                      GURL(kUrls[0]));

  processor_.ProcessSignal(*signal);

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct
  // extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionIds[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionIds[1]));
}

TEST_F(DeclarativeNetRequestActionSignalProcessorTest,
       ReportsSignalInfoCorrectlyWithMultipleUniqueActionDetails) {
  // Process 3 signals for extension 0, each containing the same action details.
  for (int i = 0; i < 3; i++) {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(kExtensionIds[0],
                                                        /*request_url=*/GURL(),
                                                        GURL(kUrls[0]));
    processor_.ProcessSignal(*signal);
  }

  // Process 3 signals for extension 1. Two signals contain the same
  // action details as above, and the third contains a second (different) action
  // details.
  for (int i = 0; i < 2; i++) {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(kExtensionIds[1],
                                                        /*request_url=*/GURL(),
                                                        GURL(kUrls[0]));
    processor_.ProcessSignal(*signal);
  }
  {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(
            kExtensionIds[1], GURL(kUrls[0]), GURL(kUrls[1]));
    processor_.ProcessSignal(*signal);
  }

  // Process 1 signal for extension 2. This signal contains different call
  // details from the above.
  {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(
            kExtensionIds[2], GURL(kUrls[0]),
            /*redirect_url=*/GURL());
    processor_.ProcessSignal(*signal);
  }

  // Retrieve signal info for extension 0.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[0]);
  ASSERT_NE(extension_0_signal_info, nullptr);

  // Verify that processor still has some data to report (for the other two
  // extensions).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for extension 1.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[1]);
  ASSERT_NE(extension_1_signal_info, nullptr);

  // Verify that processor still has some data to report (for the third
  // extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for extension 2.
  std::unique_ptr<SignalInfo> extension_2_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[2]);
  ASSERT_NE(extension_2_signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for extension 0.
  {
    const DeclarativeNetRequestActionInfo& dnr_action_info =
        extension_0_signal_info->declarative_net_request_action_info();

    // Verify data stored: only 1 unique action detail (3 API invocations).
    ASSERT_EQ(dnr_action_info.action_details_size(), 1);
    const ActionDetails& action_details = dnr_action_info.action_details(0);
    EXPECT_EQ(action_details.type(), DeclarativeNetRequestActionInfo::REDIRECT);
    EXPECT_EQ(action_details.request_url(), "");
    EXPECT_EQ(action_details.redirect_url(), kUrls[0]);
    EXPECT_EQ(action_details.count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for extension 1.
  {
    const DeclarativeNetRequestActionInfo& dnr_action_info =
        extension_1_signal_info->declarative_net_request_action_info();

    // Verify data stored: 2 unique action details (2 API invocations for the
    // 1st, 1 for the 2nd).
    ASSERT_EQ(dnr_action_info.action_details_size(), 2);
    {
      const ActionDetails& action_details = dnr_action_info.action_details(0);
      EXPECT_EQ(action_details.type(),
                DeclarativeNetRequestActionInfo::REDIRECT);
      EXPECT_EQ(action_details.request_url(), "");
      EXPECT_EQ(action_details.redirect_url(), kUrls[0]);
      EXPECT_EQ(action_details.count(), static_cast<uint32_t>(2));
    }
    {
      const ActionDetails& action_details = dnr_action_info.action_details(1);
      EXPECT_EQ(action_details.type(),
                DeclarativeNetRequestActionInfo::REDIRECT);
      EXPECT_EQ(action_details.request_url(), kUrls[0]);
      EXPECT_EQ(action_details.redirect_url(), kUrls[1]);
      EXPECT_EQ(action_details.count(), static_cast<uint32_t>(1));
    }
  }

  // Verify signal info contents for extension 2.
  {
    const DeclarativeNetRequestActionInfo& dnr_action_info =
        extension_2_signal_info->declarative_net_request_action_info();
    // Verify data stored: only 1 unique action detail (1 API invocation).
    ASSERT_EQ(dnr_action_info.action_details_size(), 1);

    const ActionDetails& action_details = dnr_action_info.action_details(0);
    EXPECT_EQ(action_details.type(), DeclarativeNetRequestActionInfo::REDIRECT);
    EXPECT_EQ(action_details.request_url(), kUrls[0]);
    EXPECT_EQ(action_details.redirect_url(), "");
    EXPECT_EQ(action_details.count(), static_cast<uint32_t>(1));
  }
}

TEST_F(DeclarativeNetRequestActionSignalProcessorTest,
       EnforcesMaxUniqueActionDetails) {
  processor_.SetMaxUniqueActionDetailsForTest(1);

  // Process 3 signals for extension 0:
  // - signals 1,2 have the same action details.
  // - signals 3 has different action details.
  for (int i = 0; i < 2; i++) {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(kExtensionIds[0],
                                                        /*request_url=*/GURL(),
                                                        GURL(kUrls[0]));
    processor_.ProcessSignal(*signal);
  }
  {
    auto signal = DeclarativeNetRequestActionSignal::
        CreateDeclarativeNetRequestRedirectActionSignal(
            kExtensionIds[0], GURL(kUrls[0]), GURL(kUrls[1]));
    processor_.ProcessSignal(*signal);
  }

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[0]);
  const DeclarativeNetRequestActionInfo& dnr_action_info =
      extension_signal_info->declarative_net_request_action_info();

  // Verify there is only 1 action details present with invocation count of 2.
  // The 2nd action details is ignored because of the limit of 1.
  ASSERT_EQ(dnr_action_info.action_details_size(), 1);
  const ActionDetails& action_details = dnr_action_info.action_details(0);
  EXPECT_EQ(action_details.count(), static_cast<uint32_t>(2));
}

}  // namespace

}  // namespace safe_browsing
