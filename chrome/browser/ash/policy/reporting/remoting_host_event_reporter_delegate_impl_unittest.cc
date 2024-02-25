// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/remoting_host_event_reporter_delegate_impl.h"

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "remoting/host/chromeos/host_event_reporter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::reporting::CRDRecord;
using ::reporting::SessionAffiliatedUser;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;
using ::testing::StrEq;

namespace remoting {
namespace {

// Production implementation of CRD event reporter for remoting host.
class HostEventReporterDelegateImplTest : public ::testing::Test {
 protected:
  std::unique_ptr<::reporting::UserEventReporterHelperTesting>
  GetReporterHelper(bool reporting_enabled,
                    bool should_report_user,
                    bool is_kiosk_user,
                    ::reporting::Status status = ::reporting::Status()) {
    auto mock_queue = std::unique_ptr<::reporting::MockReportQueue,
                                      base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::SequencedTaskRunner::GetCurrentDefault()));

    ON_CALL(*mock_queue, AddRecord(_, ::reporting::Priority::FAST_BATCH, _))
        .WillByDefault(
            [this, status](std::string_view record_string,
                           ::reporting::Priority event_priority,
                           ::reporting::ReportQueue::EnqueueCallback cb) {
              if (status.ok()) {
                CRDRecord record;
                EXPECT_TRUE(record.ParseFromArray(record_string.data(),
                                                  record_string.size()));
                records_.AddValue(std::move(record));
              }
              std::move(cb).Run(status);
            });

    auto reporter_helper =
        std::make_unique<::reporting::UserEventReporterHelperTesting>(
            reporting_enabled, should_report_user, is_kiosk_user,
            std::move(mock_queue));
    return reporter_helper;
  }

  CRDRecord WaitForEvent() { return records_.Take(); }

  bool IsEmpty() const { return records_.IsEmpty(); }

 private:
  content::BrowserTaskEnvironment task_environment;

  std::unique_ptr<HostEventReporterImpl::Delegate> delegate_;

  base::test::RepeatingTestFuture<CRDRecord> records_;
};

TEST_F(HostEventReporterDelegateImplTest, RegularCrdEvent) {
  static constexpr char kHostUser[] = "user@host.com";
  HostEventReporterDelegateImpl delegate(
      GetReporterHelper(/*reporting_enabled=*/true, /*should_report_user=*/true,
                        /*is_kiosk_user=*/false));

  {
    CRDRecord record;
    record.mutable_host_user()->set_user_email(kHostUser);
    delegate.EnqueueEvent(record);
  }

  EXPECT_THAT(WaitForEvent(),
              AllOf(Property(&CRDRecord::is_kiosk_session, Eq(false)),
                    Property(&CRDRecord::host_user,
                             Property(&SessionAffiliatedUser::user_email,
                                      StrEq(kHostUser)))));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(HostEventReporterDelegateImplTest, AnonymousCrdEvent) {
  static constexpr char kNonAffiliatedUser[] = "user@alien.com";
  HostEventReporterDelegateImpl delegate(GetReporterHelper(
      /*reporting_enabled=*/true, /*should_report_user=*/false,
      /*is_kiosk_user=*/false));

  {
    CRDRecord record;
    record.mutable_host_user()->set_user_email(kNonAffiliatedUser);
    delegate.EnqueueEvent(record);
  }

  EXPECT_THAT(WaitForEvent(),
              AllOf(Property(&CRDRecord::is_kiosk_session, Eq(false)),
                    Property(&CRDRecord::host_user,
                             Property(&SessionAffiliatedUser::has_user_email,
                                      Eq(false)))));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(HostEventReporterDelegateImplTest, KioskCrdEvent) {
  static constexpr char kKioskUser[] = "user@host.com";
  HostEventReporterDelegateImpl delegate(GetReporterHelper(
      /*reporting_enabled=*/true, /*should_report_user=*/false,
      /*is_kiosk_user=*/true));

  {
    CRDRecord record;
    record.mutable_host_user()->set_user_email(kKioskUser);
    delegate.EnqueueEvent(record);
  }

  EXPECT_THAT(WaitForEvent(),
              AllOf(Property(&CRDRecord::is_kiosk_session, Eq(true)),
                    Property(&CRDRecord::host_user,
                             Property(&SessionAffiliatedUser::has_user_email,
                                      Eq(false)))));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(HostEventReporterDelegateImplTest, DisabledCrdEvent) {
  static constexpr char kHostUser[] = "user@host.com";
  HostEventReporterDelegateImpl delegate(GetReporterHelper(
      /*reporting_enabled=*/false, /*should_report_user=*/true,
      /*is_kiosk_user=*/false));
  {
    CRDRecord record;
    record.mutable_host_user()->set_user_email(kHostUser);
    delegate.EnqueueEvent(record);
  }

  EXPECT_TRUE(IsEmpty());
}

TEST_F(HostEventReporterDelegateImplTest, ErrorPostingCrdEvent) {
  static constexpr char kHostUser[] = "user@host.com";
  HostEventReporterDelegateImpl delegate(GetReporterHelper(
      /*reporting_enabled=*/true, /*should_report_user=*/true,
      /*is_kiosk_user=*/false,
      ::reporting::Status(::reporting::error::INTERNAL, "")));
  {
    CRDRecord record;
    record.mutable_host_user()->set_user_email(kHostUser);
    delegate.EnqueueEvent(record);
  }

  EXPECT_TRUE(IsEmpty());
}

}  // namespace
}  // namespace remoting
