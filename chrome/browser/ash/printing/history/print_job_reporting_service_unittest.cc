// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_reporting_service.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash {

namespace {

namespace print = printing::proto;
namespace em = ::enterprise_management;

using ::testing::_;
using ::testing::DoAll;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Not;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;

em::PrintJobEvent CreateJobEvent(
    const std::string id,
    const std::string title,
    ::reporting::error::Code status,
    em::PrintJobEvent::PrintSettings::ColorMode color,
    em::PrintJobEvent::PrintSettings::DuplexMode duplex) {
  em::PrintJobEvent event;
  // Print job configuration
  auto* job_config = event.mutable_job_configuration();
  job_config->set_id(id);
  job_config->set_title(title);
  job_config->set_status(status);
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCString("14 Feb 2021 10:00", &time));
  job_config->set_creation_timestamp_ms(time.InMillisecondsSinceUnixEpoch());
  EXPECT_TRUE(base::Time::FromUTCString("14 Feb 2021 10:30", &time));
  job_config->set_completion_timestamp_ms(time.InMillisecondsSinceUnixEpoch());
  job_config->set_number_of_pages(10);
  // Print settings
  auto* settings = job_config->mutable_settings();
  settings->set_color(color);
  settings->set_duplex(duplex);
  settings->mutable_media_size()->set_height(297000);
  settings->mutable_media_size()->set_width(420000);
  settings->mutable_media_size()->set_vendor_id("iso_a3_297x420mm");
  settings->set_copies(1);
  // Printer
  event.mutable_printer()->set_uri("ipp://192.168.1.5:631");
  event.mutable_printer()->set_name("name1");
  event.mutable_printer()->set_id(id);
  // User
  event.set_user_type(em::PrintJobEvent_UserType_KIOSK);
  return event;
}

print::PrintJobInfo CreateJobInfo(const std::string id,
                                  const std::string title,
                                  print::PrintJobInfo::PrintJobStatus status,
                                  print::PrintSettings::ColorMode color,
                                  print::PrintSettings::DuplexMode duplex) {
  print::PrintJobInfo info;
  // Print job configuration
  info.set_id(id);
  info.set_title(title);
  info.set_status(status);
  base::Time time;
  EXPECT_TRUE(base::Time::FromUTCString("14 Feb 2021 10:00", &time));
  info.set_creation_time(time.InMillisecondsSinceUnixEpoch());
  info.set_creation_time(time.InMillisecondsSinceUnixEpoch());
  EXPECT_TRUE(base::Time::FromUTCString("14 Feb 2021 10:30", &time));
  info.set_completion_time(time.InMillisecondsSinceUnixEpoch());
  info.set_number_of_pages(10);
  // Print settings
  auto* settings = info.mutable_settings();
  settings->set_color(color);
  settings->set_duplex(duplex);
  settings->mutable_media_size()->set_height(297000);
  settings->mutable_media_size()->set_width(420000);
  settings->mutable_media_size()->set_vendor_id("iso_a3_297x420mm");
  settings->set_copies(1);
  // Printer
  info.mutable_printer()->set_name("name1");
  info.mutable_printer()->set_uri("ipp://192.168.1.5:631");
  info.mutable_printer()->set_source(print::Printer_PrinterSource_POLICY);
  info.mutable_printer()->set_id(id);
  return info;
}

em::PrintJobEvent JobEvent1() {
  return CreateJobEvent("id1", "title1", ::reporting::error::OK,
                        em::PrintJobEvent::PrintSettings::BLACK_AND_WHITE,
                        em::PrintJobEvent::PrintSettings::ONE_SIDED);
}

print::PrintJobInfo JobInfo1() {
  return CreateJobInfo("id1", "title1", print::PrintJobInfo::PRINTED,
                       print::PrintSettings::BLACK_AND_WHITE,
                       print::PrintSettings::ONE_SIDED);
}

em::PrintJobEvent JobEvent2() {
  return CreateJobEvent("id2", "title2", ::reporting::error::CANCELLED,
                        em::PrintJobEvent::PrintSettings::COLOR,
                        em::PrintJobEvent::PrintSettings::TWO_SIDED_LONG_EDGE);
}

print::PrintJobInfo JobInfo2() {
  return CreateJobInfo("id2", "title2", print::PrintJobInfo::CANCELED,
                       print::PrintSettings::COLOR,
                       print::PrintSettings::TWO_SIDED_LONG_EDGE);
}

class PrintJobEventMatcher : public MatcherInterface<const em::PrintJobEvent&> {
 public:
  explicit PrintJobEventMatcher(const em::PrintJobEvent& event)
      : id_(event.job_configuration().id()),
        title_(event.job_configuration().title()),
        status_(static_cast<::reporting::error::Code>(
            event.job_configuration().status())),
        creation_timestamp_ms_(
            event.job_configuration().creation_timestamp_ms()),
        completion_timestamp_ms_(
            event.job_configuration().completion_timestamp_ms()),
        pages_(event.job_configuration().number_of_pages()),
        color_(event.job_configuration().settings().color()),
        duplex_(event.job_configuration().settings().duplex()),
        media_size_(event.job_configuration().settings().media_size()),
        copies_(event.job_configuration().settings().copies()),
        printer_uri_(event.printer().uri()),
        printer_name_(event.printer().name()),
        user_type_(event.user_type()) {}

  bool MatchAndExplain(const em::PrintJobEvent& event,
                       MatchResultListener* listener) const {
    // Print job configuration
    bool id_equal = event.job_configuration().id() == id_;
    if (!id_equal) {
      *listener << " |id| is " << event.job_configuration().id();
    }
    bool title_equal = event.job_configuration().title() == title_;
    if (!title_equal) {
      *listener << " |title| is " << event.job_configuration().title();
    }
    bool status_equal = event.job_configuration().status() == status_;
    if (!status_equal) {
      *listener << " |status| is " << event.job_configuration().status();
    }
    bool creation_timestamp_ms_equal =
        event.job_configuration().creation_timestamp_ms() ==
        creation_timestamp_ms_;
    if (!creation_timestamp_ms_equal) {
      *listener << " |creation timestamp_ms| is "
                << event.job_configuration().creation_timestamp_ms();
    }
    bool completion_timestamp_ms_equal =
        event.job_configuration().completion_timestamp_ms() ==
        completion_timestamp_ms_;
    if (!completion_timestamp_ms_equal) {
      *listener << " |completion timestamp_ms| is "
                << event.job_configuration().completion_timestamp_ms();
    }
    bool pages_equal = event.job_configuration().number_of_pages() == pages_;
    if (!pages_equal) {
      *listener << " |pages| is "
                << event.job_configuration().number_of_pages();
    }
    // Print settings
    bool color_equal = event.job_configuration().settings().color() == color_;
    if (!color_equal) {
      *listener << " |color| is "
                << event.job_configuration().settings().color();
    }
    bool duplex_equal =
        event.job_configuration().settings().duplex() == duplex_;
    if (!duplex_equal) {
      *listener << " |duplex| is "
                << event.job_configuration().settings().duplex();
    }
    auto& media_size = event.job_configuration().settings().media_size();
    bool media_equal = media_size.width() == media_size_.width() &&
                       media_size.height() == media_size_.height() &&
                       media_size.vendor_id() == media_size_.vendor_id();
    if (!media_equal) {
      *listener << " |media| is " << media_size.width() << "x"
                << media_size.height() << " and " << media_size.vendor_id();
    }
    bool copies_equal =
        event.job_configuration().settings().copies() == copies_;
    if (!copies_equal) {
      *listener << " |copies| is "
                << event.job_configuration().settings().copies();
    }
    // Printer
    bool printer_uri_equal = event.printer().uri() == printer_uri_;
    if (!printer_uri_equal) {
      *listener << " |printer uri| is " << event.printer().uri();
    }
    bool printer_name_equal = event.printer().name() == printer_name_;
    if (!printer_name_equal) {
      *listener << " |printer name| is " << event.printer().name();
    }
    bool printer_id_equal = event.printer().id() == id_;
    if (!printer_id_equal) {
      *listener << " |printer id| is " << event.printer().id();
    }

    // User
    bool user_type_equal = event.user_type() == user_type_;
    if (!user_type_equal) {
      *listener << " |user type| is " << event.user_type();
    }

    return id_equal && title_equal && status_equal &&
           creation_timestamp_ms_equal && completion_timestamp_ms_equal &&
           pages_equal && color_equal && duplex_equal && media_equal &&
           copies_equal && printer_uri_equal && printer_name_equal &&
           printer_id_equal && user_type_equal;
  }

  void DescribeTo(::std::ostream* os) const {}

 private:
  std::string id_;
  std::string title_;
  ::reporting::error::Code status_;
  int64_t creation_timestamp_ms_;
  int64_t completion_timestamp_ms_;
  int pages_;
  em::PrintJobEvent_PrintSettings_ColorMode color_;
  em::PrintJobEvent_PrintSettings_DuplexMode duplex_;
  em::PrintJobEvent_PrintSettings_MediaSize media_size_;
  int32_t copies_;
  std::string printer_uri_;
  std::string printer_name_;
  em::PrintJobEvent_UserType user_type_;
};

Matcher<const em::PrintJobEvent&> IsPrintJobEvent(
    const em::PrintJobEvent& event) {
  return Matcher<const em::PrintJobEvent&>(new PrintJobEventMatcher(event));
}

}  // namespace

class PrintJobReportingServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto user_manager = std::make_unique<FakeChromeUserManager>();
    AccountId account_id(AccountId::FromUserEmail("user0@managed.com"));
    user_manager->AddKioskAppUser(account_id);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  void ChangeReportingSetting(bool should_report) {
    testing_settings_.device_settings()->SetBoolean(kReportDevicePrintJobs,
                                                    should_report);
  }

  // Creates a new report queue that can be used by the PrintJobReportingService
  // with the enqueue operation stubbed
  std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
  CreateReportQueue() {
    auto report_queue = std::unique_ptr<::reporting::MockReportQueue,
                                        base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::ThreadPool::CreateSequencedTaskRunner({})));
    EXPECT_CALL(*report_queue, AddRecord)
        .WillRepeatedly(
            [this](std::string_view record, ::reporting::Priority priority,
                   ::reporting::ReportQueue::EnqueueCallback callback) {
              em::PrintJobEvent event;
              event.ParseFromString(std::string(record));
              events_.push_back(event);
              priorities_.push_back(priority);
            });
    return std::move(report_queue);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingCrosSettings testing_settings_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<PrintJobReportingService> print_job_reporting_service_;
  std::vector<em::PrintJobEvent> events_;
  std::vector<::reporting::Priority> priorities_;
};

TEST_F(PrintJobReportingServiceTest, ShouldReportPolicyDisabled) {
  print_job_reporting_service_ =
      PrintJobReportingService::CreateForTest(CreateReportQueue());
  ChangeReportingSetting(false);
  print_job_reporting_service_->OnPrintJobFinished(JobInfo1());
  print_job_reporting_service_->OnPrintJobFinished(JobInfo2());

  EXPECT_TRUE(events_.empty());
  EXPECT_TRUE(priorities_.empty());
}

TEST_F(PrintJobReportingServiceTest, Enqueue) {
  print_job_reporting_service_ =
      PrintJobReportingService::CreateForTest(CreateReportQueue());
  ChangeReportingSetting(true);
  print_job_reporting_service_->OnPrintJobFinished(JobInfo1());
  print_job_reporting_service_->OnPrintJobFinished(JobInfo2());

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[0], IsPrintJobEvent(JobEvent1()));
  EXPECT_EQ(priorities_[0], ::reporting::Priority::SLOW_BATCH);
  EXPECT_THAT(events_[1], IsPrintJobEvent(JobEvent2()));
  EXPECT_EQ(priorities_[1], ::reporting::Priority::SLOW_BATCH);
}

TEST_F(PrintJobReportingServiceTest, ShouldReportPolicyInitiallyEnabled) {
  ChangeReportingSetting(true);
  // Create the reporting service after setting the policy to true.
  print_job_reporting_service_ =
      PrintJobReportingService::CreateForTest(CreateReportQueue());

  print_job_reporting_service_->OnPrintJobFinished(JobInfo1());

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0], IsPrintJobEvent(JobEvent1()));
  EXPECT_EQ(priorities_[0], ::reporting::Priority::SLOW_BATCH);
}

TEST_F(PrintJobReportingServiceTest, ShouldReportPolicyInitiallyDisabled) {
  ChangeReportingSetting(false);
  // Create the reporting service after setting the policy to false.
  print_job_reporting_service_ =
      PrintJobReportingService::CreateForTest(CreateReportQueue());
  print_job_reporting_service_->OnPrintJobFinished(JobInfo1());

  EXPECT_TRUE(events_.empty());
  EXPECT_TRUE(priorities_.empty());
}

}  // namespace ash
