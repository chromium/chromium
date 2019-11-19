// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_finished_event_dispatcher.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/history/mock_print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chrome/browser/chromeos/printing/history/test_print_job_history_service_observer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/printing_metrics.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<extensions::api::printing_metrics::PrintJobInfo>
GetPrintJobFinishedEventValue(
    const extensions::TestEventRouterObserver* observer) {
  const auto& event_map = observer->events();
  auto iter = event_map.find(
      extensions::api::printing_metrics::OnPrintJobFinished::kEventName);
  if (iter == event_map.end())
    return nullptr;

  const extensions::Event& event = *iter->second;
  if (!event.event_args || !event.event_args->is_list() ||
      event.event_args->GetList().size() != 1u) {
    ADD_FAILURE() << "Invalid event args";
    return nullptr;
  }

  return extensions::api::printing_metrics::PrintJobInfo::FromValue(
      event.event_args->GetList()[0]);
}

// Creates a new MockPrintJobHistoryService for the given |context|.
std::unique_ptr<KeyedService> BuildPrintJobHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::MockPrintJobHistoryService>();
}

// Creates a new EventRouter for the given |context|.
std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(context, nullptr);
}

}  // namespace

namespace extensions {

class PrintJobFinishedEventDispatcherUnittest : public testing::Test {
 public:
  PrintJobFinishedEventDispatcherUnittest() {}
  ~PrintJobFinishedEventDispatcherUnittest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    chromeos::PrintJobHistoryServiceFactory::GetInstance()->SetTestingFactory(
        testing_profile_, base::BindRepeating(&BuildPrintJobHistoryService));

    EventRouterFactory::GetInstance()->SetTestingFactory(
        testing_profile_, base::BindRepeating(&BuildEventRouter));

    dispatcher_ =
        std::make_unique<PrintJobFinishedEventDispatcher>(testing_profile_);
    observer_ = std::make_unique<TestEventRouterObserver>(
        EventRouter::Get(testing_profile_));
  }

  void TearDown() override {
    observer_.reset();
    dispatcher_.reset();

    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext service_manager_context_;
  TestingProfile* testing_profile_;
  std::unique_ptr<TestEventRouterObserver> observer_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<PrintJobFinishedEventDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobFinishedEventDispatcherUnittest);
};

// Test that |OnPrintJobFinished| is dispatched when the print job is saved by
// PrintJobHistoryService.
TEST_F(PrintJobFinishedEventDispatcherUnittest, EventIsDispatched) {
  constexpr char kTitle[] = "title";
  constexpr int kPagesNumber = 3;

  base::RunLoop run_loop;
  chromeos::MockPrintJobHistoryService* print_job_history_service =
      static_cast<chromeos::MockPrintJobHistoryService*>(
          chromeos::PrintJobHistoryServiceFactory::GetForBrowserContext(
              testing_profile_));
  chromeos::TestPrintJobHistoryServiceObserver observer(
      print_job_history_service, run_loop.QuitWhenIdleClosure());

  chromeos::printing::proto::PrintJobInfo print_job_info_proto;
  print_job_info_proto.set_title(kTitle);
  print_job_info_proto.set_status(
      chromeos::printing::proto::PrintJobInfo_PrintJobStatus_FAILED);
  print_job_info_proto.set_number_of_pages(kPagesNumber);
  print_job_history_service->SavePrintJobProto(print_job_info_proto);
  run_loop.Run();

  // As soon as Run() is completed all PrintJobHistoryService observers are
  // called and event is expected to appear in the |observer_|.
  std::unique_ptr<extensions::api::printing_metrics::PrintJobInfo>
      print_job_info = GetPrintJobFinishedEventValue(observer_.get());
  ASSERT_TRUE(print_job_info);
  EXPECT_EQ(kTitle, print_job_info->title);
  EXPECT_EQ(extensions::api::printing_metrics::PRINT_JOB_STATUS_FAILED,
            print_job_info->status);
  EXPECT_EQ(kPagesNumber, print_job_info->number_of_pages);
}

}  // namespace extensions
