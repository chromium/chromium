// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/test_cups_print_job_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/printing.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class PrintingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  PrintingEventObserver(TestEventRouter* event_router,
                        const std::string& event_name)
      : event_router_(event_router), event_name_(event_name) {
    event_router_->AddEventObserver(this);
  }

  ~PrintingEventObserver() override {
    event_router_->RemoveEventObserver(this);
  }

  // TestEventRouter::EventObserver:
  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const Event& event) override {
    if (event.event_name == event_name_) {
      extension_id_ = extension_id;
      event_args_ = event.event_args->Clone();
    }
  }

  const std::string& extension_id() const { return extension_id_; }

  const base::Value& event_args() const { return event_args_; }

 private:
  // Event router this class should observe.
  TestEventRouter* const event_router_;

  // The name of the observed event.
  const std::string event_name_;

  // The extension id passed for the last observed event.
  std::string extension_id_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(PrintingEventObserver);
};

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kPrinterId[] = "printer";
constexpr int kJobId = 10;

// Creates a new TestCupsPrintJobManager for the given |context|.
std::unique_ptr<KeyedService> BuildCupsPrintJobManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::TestCupsPrintJobManager>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class PrintingAPIHandlerUnittest : public testing::Test {
 public:
  PrintingAPIHandlerUnittest()
      : scoped_current_channel_(version_info::Channel::DEV) {}
  ~PrintingAPIHandlerUnittest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    chromeos::CupsPrintJobManagerFactory::GetInstance()->SetTestingFactory(
        testing_profile_, base::BindRepeating(&BuildCupsPrintJobManager));
    event_router_ = CreateAndUseTestEventRouter(testing_profile_);

    const char kExtensionName[] = "Printing extension";
    const char kPermissionName[] = "printing";
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(kExtensionName)
            .SetID(kExtensionId)
            .AddPermission(kPermissionName)
            .Build();
    ExtensionRegistry::Get(testing_profile_)->AddEnabled(extension);

    printing_api_handler_ =
        std::make_unique<PrintingAPIHandler>(testing_profile_);
  }

  void TearDown() override {
    printing_api_handler_.reset();

    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext service_manager_context_;
  TestingProfile* testing_profile_;
  TestEventRouter* event_router_ = nullptr;

 private:
  // TODO(crbug.com/992889): Remove this once the API is launched for stable.
  ScopedCurrentChannel scoped_current_channel_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<PrintingAPIHandler> printing_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(PrintingAPIHandlerUnittest);
};

// Test that |OnJobStatusChanged| is dispatched when the print job status is
// changed.
TEST_F(PrintingAPIHandlerUnittest, EventIsDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  chromeos::TestCupsPrintJobManager* print_job_manager =
      static_cast<chromeos::TestCupsPrintJobManager*>(
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              testing_profile_));
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::EXTENSION,
          kExtensionId, chromeos::printing::proto::PrintSettings());
  print_job_manager->CreatePrintJob(print_job.get());

  EXPECT_EQ(kExtensionId, event_observer.extension_id());
  const base::Value& event_args = event_observer.event_args();
  ASSERT_TRUE(event_args.is_list());
  ASSERT_EQ(2u, event_args.GetList().size());
  base::Value job_id = event_args.GetList()[0].Clone();
  ASSERT_TRUE(job_id.is_string());
  EXPECT_EQ(chromeos::CupsPrintJob::CreateUniqueId(kPrinterId, kJobId),
            job_id.GetString());
  base::Value job_status = event_args.GetList()[1].Clone();
  ASSERT_TRUE(job_status.is_string());
  EXPECT_EQ(api::printing::JOB_STATUS_PENDING,
            api::printing::ParseJobStatus(job_status.GetString()));
}

// Test that |OnJobStatusChanged| is not dispatched if the print job was created
// on Print Preview page.
TEST_F(PrintingAPIHandlerUnittest, PrintPreviewEventIsNotDispatched) {
  PrintingEventObserver event_observer(
      event_router_, api::printing::OnJobStatusChanged::kEventName);

  chromeos::TestCupsPrintJobManager* print_job_manager =
      static_cast<chromeos::TestCupsPrintJobManager*>(
          chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(
              testing_profile_));
  std::unique_ptr<chromeos::CupsPrintJob> print_job =
      std::make_unique<chromeos::CupsPrintJob>(
          chromeos::Printer(kPrinterId), kJobId, "title",
          /*total_page_number=*/3, ::printing::PrintJob::Source::PRINT_PREVIEW,
          /*source_id=*/"", chromeos::printing::proto::PrintSettings());
  print_job_manager->CreatePrintJob(print_job.get());

  // Check that the print job created on Print Preview doesn't show up.
  EXPECT_EQ("", event_observer.extension_id());
  EXPECT_TRUE(event_observer.event_args().is_none());
}

}  // namespace extensions
