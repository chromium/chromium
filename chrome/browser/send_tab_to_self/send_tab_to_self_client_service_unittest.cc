// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

// A test ReceivingUiHandler that keeps track of the number of entries for which
// DisplayNewEntry was called.
class TestReceivingUiHandler : public ReceivingUiHandler {
 public:
  TestReceivingUiHandler() = default;
  ~TestReceivingUiHandler() override {}

  void DisplayNewEntries(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override {
    number_displayed_entries_ = number_displayed_entries_ + new_entries.size();
  }

  void DismissEntries(const std::vector<std::string>& guids) override {}

  size_t number_displayed_entries() const { return number_displayed_entries_; }

 private:
  size_t number_displayed_entries_ = 0;
};

// A test SendTabToSelfClientService that exposes the TestReceivingUiHandler.
class TestSendTabToSelfClientService : public SendTabToSelfClientService {
 public:
  explicit TestSendTabToSelfClientService(SendTabToSelfModel* model)
      : SendTabToSelfClientService(nullptr, model) {}

  ~TestSendTabToSelfClientService() override = default;

  void SetupHandlerRegistry(Profile* profile) override {}

  TestReceivingUiHandler* SetupTestHandler() {
    test_handlers_.clear();
    std::unique_ptr<TestReceivingUiHandler> handler =
        std::make_unique<TestReceivingUiHandler>();
    TestReceivingUiHandler* handler_ptr = handler.get();
    test_handlers_.push_back(std::move(handler));
    return handler_ptr;
  }

  // This copies the SendTabToSelfClientService implementation without a cast to
  // DesktopNotificationHandler on desktop platforms. See notes in that file.
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override {
    for (const std::unique_ptr<ReceivingUiHandler>& handler : GetHandlers()) {
      handler->DisplayNewEntries(new_entries);
    }
  }

  const std::vector<std::unique_ptr<ReceivingUiHandler>>& GetHandlers()
      const override {
    return test_handlers_;
  }

 protected:
  std::vector<std::unique_ptr<ReceivingUiHandler>> test_handlers_;
};

// Tests that the UI handlers gets notified of each entries that were added
// remotely.
TEST(SendTabToSelfClientServiceTest, MultipleEntriesAdded) {
  // Set up the test objects.
  TestSendTabToSelfModel test_model_;
  TestSendTabToSelfClientService client_service(&test_model_);
  TestReceivingUiHandler* test_handler = client_service.SetupTestHandler();

  // Create 2 entries and simulated that they were both added remotely.
  SendTabToSelfEntry entry1("a", GURL("http://www.example-a.com"), "a site",
                            base::Time(), base::Time(), "device a", "device b");
  SendTabToSelfEntry entry2("b", GURL("http://www.example-b.com"), "b site",
                            base::Time(), base::Time(), "device b", "device a");
  client_service.EntriesAddedRemotely({&entry1, &entry2});

  EXPECT_EQ(2u, test_handler->number_displayed_entries());
}

}  // namespace

}  // namespace send_tab_to_self
