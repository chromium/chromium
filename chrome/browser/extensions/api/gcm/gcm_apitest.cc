// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "extensions/test/result_catcher.h"

using extensions::ResultCatcher;

namespace {

const char kEventsExtension[] = "gcm/events";

gcm::GCMClient::SendErrorDetails CreateErrorDetails(
    const std::string& message_id,
    const gcm::GCMClient::Result result,
    const std::string& total_messages) {
  gcm::GCMClient::SendErrorDetails error;
  error.message_id = message_id;
  error.result = result;
  error.additional_data["expectedMessageId"] = message_id;
  switch (result) {
    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
      error.additional_data["expectedErrorMessage"] =
          "Asynchronous operation is pending.";
      break;
    case gcm::GCMClient::SERVER_ERROR:
      error.additional_data["expectedErrorMessage"] = "Server error occurred.";
      break;
    case gcm::GCMClient::NETWORK_ERROR:
      error.additional_data["expectedErrorMessage"] = "Network error occurred.";
      break;
    case gcm::GCMClient::TTL_EXCEEDED:
      error.additional_data["expectedErrorMessage"] = "Time-to-live exceeded.";
      break;
    case gcm::GCMClient::UNKNOWN_ERROR:
    default:  // Default case is the same as UNKNOWN_ERROR
      error.additional_data["expectedErrorMessage"] = "Unknown error occurred.";
      break;
  }
  error.additional_data["totalMessages"] = total_messages;
  return error;
}

}  // namespace

namespace extensions {

class GcmApiTest : public ExtensionApiTest {
 public:
  GcmApiTest() : fake_gcm_profile_service_(NULL) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  void StartCollecting();

  const Extension* LoadTestExtension(const std::string& extension_path,
                                     const std::string& page_name);
  gcm::FakeGCMProfileService* service() const;

 private:
  gcm::FakeGCMProfileService* fake_gcm_profile_service_;
};

void GcmApiTest::SetUpCommandLine(base::CommandLine* command_line) {
  // We now always create the GCMProfileService instance in
  // ProfileSyncServiceFactory that is called when a profile is being
  // initialized. In order to prevent it from being created, we add the switch
  // to disable the sync logic.
  command_line->AppendSwitch(switches::kDisableSync);

  ExtensionApiTest::SetUpCommandLine(command_line);
}

void GcmApiTest::SetUpOnMainThread() {
  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      base::BindRepeating(&gcm::FakeGCMProfileService::Build));
  fake_gcm_profile_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetInstance()->GetForProfile(
          browser()->profile()));

  ExtensionApiTest::SetUpOnMainThread();
}

void GcmApiTest::StartCollecting() {
  service()->set_collect(true);
}

gcm::FakeGCMProfileService* GcmApiTest::service() const {
  return fake_gcm_profile_service_;
}

const Extension* GcmApiTest::LoadTestExtension(
    const std::string& extension_path,
    const std::string& page_name) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(extension_path));
  if (extension) {
    ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL(page_name));
  }
  return extension;
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, RegisterValidation) {
  ASSERT_TRUE(RunExtensionTest("gcm/functions/register_validation"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, Register) {
  StartCollecting();
  ASSERT_TRUE(RunExtensionTest("gcm/functions/register"));

  const std::vector<std::string>& sender_ids =
      service()->last_registered_sender_ids();
  EXPECT_TRUE(base::Contains(sender_ids, "Sender1"));
  EXPECT_TRUE(base::Contains(sender_ids, "Sender2"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, Unregister) {
  service()->AddExpectedUnregisterResponse(gcm::GCMClient::SUCCESS);
  service()->AddExpectedUnregisterResponse(gcm::GCMClient::SERVER_ERROR);

  ASSERT_TRUE(RunExtensionTest("gcm/functions/unregister"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, SendValidation) {
  ASSERT_TRUE(RunExtensionTest("gcm/functions/send"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, SendMessageData) {
  StartCollecting();
  ASSERT_TRUE(RunExtensionTest("gcm/functions/send_message_data"));

  EXPECT_EQ("destination-id", service()->last_receiver_id());
  const gcm::OutgoingMessage& message = service()->last_sent_message();
  gcm::MessageData::const_iterator iter;

  EXPECT_EQ(100, message.time_to_live);

  EXPECT_TRUE((iter = message.data.find("key1")) != message.data.end());
  EXPECT_EQ("value1", iter->second);

  EXPECT_TRUE((iter = message.data.find("key2")) != message.data.end());
  EXPECT_EQ("value2", iter->second);
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, SendMessageDefaultTTL) {
  StartCollecting();
  ASSERT_TRUE(RunExtensionTest("gcm/functions/send_message_default_ttl"));

  EXPECT_EQ("destination-id", service()->last_receiver_id());
  const gcm::OutgoingMessage& message = service()->last_sent_message();
  gcm::MessageData::const_iterator iter;

  EXPECT_EQ(gcm::OutgoingMessage::kMaximumTTL, message.time_to_live);
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnMessagesDeleted) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_messages_deleted.html");
  ASSERT_TRUE(extension);

  extensions::ExtensionGCMAppHandler app_handler(profile());
  app_handler.OnMessagesDeleted(extension->id());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnMessage) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_message.html");
  ASSERT_TRUE(extension);

  extensions::ExtensionGCMAppHandler app_handler(profile());

  gcm::IncomingMessage message;
  message.data["property1"] = "value1";
  message.data["property2"] = "value2";
  // First message is sent without from and collapse key.
  app_handler.OnMessage(extension->id(), message);

  // Second message is send with from.
  message.sender_id = "12345678";
  app_handler.OnMessage(extension->id(), message);

  // Third message is send with a collapse key.
  message.sender_id.clear();
  message.collapse_key = "collapseKeyValue";
  app_handler.OnMessage(extension->id(), message);

  // Fourth message carries the same data, from and collapse key.
  message.sender_id = "12345678";
  app_handler.OnMessage(extension->id(), message);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnSendError) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_send_error.html");
  ASSERT_TRUE(extension);

  std::string total_expected_messages = "5";
  extensions::ExtensionGCMAppHandler app_handler(profile());
  app_handler.OnSendError(
      extension->id(),
      CreateErrorDetails("error_message_1",
                         gcm::GCMClient::ASYNC_OPERATION_PENDING,
                         total_expected_messages));
  app_handler.OnSendError(
      extension->id(),
      CreateErrorDetails("error_message_2",
                         gcm::GCMClient::SERVER_ERROR,
                         total_expected_messages));
  app_handler.OnSendError(
      extension->id(),
      CreateErrorDetails("error_message_3",
                         gcm::GCMClient::NETWORK_ERROR,
                         total_expected_messages));
  app_handler.OnSendError(
      extension->id(),
      CreateErrorDetails("error_message_4",
                         gcm::GCMClient::UNKNOWN_ERROR,
                         total_expected_messages));
  app_handler.OnSendError(
      extension->id(),
      CreateErrorDetails("error_message_5",
                         gcm::GCMClient::TTL_EXCEEDED,
                         total_expected_messages));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, Incognito) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ResultCatcher incognito_catcher;
  incognito_catcher.RestrictToBrowserContext(
      profile()->GetOffTheRecordProfile());

  ASSERT_TRUE(RunExtensionTestIncognito("gcm/functions/incognito"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(incognito_catcher.GetNextResult()) << incognito_catcher.message();
}

}  // namespace extensions
