// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "content/public/browser/browser_task_traits.h"
#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAppID[] = "TestApp";
const char kUserID[] = "user";

void RequestProxyResolvingSocketFactoryOnUIThread(
    Profile* profile,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service)
    return;
  return content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetNetworkContext()
      ->CreateProxyResolvingSocketFactory(std::move(receiver));
}

void RequestProxyResolvingSocketFactory(
    Profile* profile,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                profile, service, std::move(receiver)));
}

std::unique_ptr<KeyedService> BuildGCMProfileService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  return std::make_unique<gcm::GCMProfileService>(
      profile->GetPrefs(), profile->GetPath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, profile),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess(),
      network::TestNetworkConnectionTracker::GetInstance(),
      chrome::GetChannel(),
      gcm::GetProductCategoryForSubtypes(profile->GetPrefs()),
      IdentityManagerFactory::GetForProfile(profile),
      std::unique_ptr<gcm::GCMClientFactory>(new gcm::FakeGCMClientFactory(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}),
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}))),
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}),
      base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
      blocking_task_runner);
}

}  // namespace

class GCMProfileServiceTest : public testing::Test {
 protected:
  GCMProfileServiceTest();
  ~GCMProfileServiceTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  FakeGCMClient* GetGCMClient() const;

  void CreateGCMProfileService();

  void RegisterAndWaitForCompletion(const std::vector<std::string>& sender_ids);
  void UnregisterAndWaitForCompletion();
  void SendAndWaitForCompletion(const OutgoingMessage& message);

  void RegisterCompleted(const base::Closure& callback,
                         const std::string& registration_id,
                         GCMClient::Result result);
  void UnregisterCompleted(const base::Closure& callback,
                           GCMClient::Result result);
  void SendCompleted(const base::Closure& callback,
                     const std::string& message_id,
                     GCMClient::Result result);

  GCMDriver* driver() const { return gcm_profile_service_->driver(); }
  std::string registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }
  std::string send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  GCMProfileService* gcm_profile_service_;
  std::unique_ptr<FakeGCMAppHandler> gcm_app_handler_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  GCMClient::Result unregistration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTest);
};

GCMProfileServiceTest::GCMProfileServiceTest()
    : gcm_profile_service_(NULL),
      gcm_app_handler_(new FakeGCMAppHandler),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR) {
}

GCMProfileServiceTest::~GCMProfileServiceTest() {
}

FakeGCMClient* GCMProfileServiceTest::GetGCMClient() const {
  return static_cast<FakeGCMClient*>(
      gcm_profile_service_->driver()->GetGCMClientForTesting());
}

void GCMProfileServiceTest::SetUp() {
#if defined(OS_CHROMEOS)
  // Create a DBus thread manager setter for its side effect.
  // Ignore the return value.
  chromeos::DBusThreadManager::GetSetterForTesting();
#endif
  TestingProfile::Builder builder;
  profile_ = builder.Build();
}

void GCMProfileServiceTest::TearDown() {
  gcm_profile_service_->driver()->RemoveAppHandler(kTestAppID);
}

void GCMProfileServiceTest::CreateGCMProfileService() {
  gcm_profile_service_ = static_cast<GCMProfileService*>(
      GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), base::BindRepeating(&BuildGCMProfileService)));
  gcm_profile_service_->driver()->AddAppHandler(
      kTestAppID, gcm_app_handler_.get());
}

void GCMProfileServiceTest::RegisterAndWaitForCompletion(
    const std::vector<std::string>& sender_ids) {
  base::RunLoop run_loop;
  gcm_profile_service_->driver()->Register(
      kTestAppID,
      sender_ids,
      base::Bind(&GCMProfileServiceTest::RegisterCompleted,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::UnregisterAndWaitForCompletion() {
  base::RunLoop run_loop;
  gcm_profile_service_->driver()->Unregister(
      kTestAppID,
      base::Bind(&GCMProfileServiceTest::UnregisterCompleted,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::SendAndWaitForCompletion(
    const OutgoingMessage& message) {
  base::RunLoop run_loop;
  gcm_profile_service_->driver()->Send(
      kTestAppID,
      kUserID,
      message,
      base::Bind(&GCMProfileServiceTest::SendCompleted,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::RegisterCompleted(
     const base::Closure& callback,
     const std::string& registration_id,
     GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  callback.Run();
}

void GCMProfileServiceTest::UnregisterCompleted(
    const base::Closure& callback,
    GCMClient::Result result) {
  unregistration_result_ = result;
  callback.Run();
}

void GCMProfileServiceTest::SendCompleted(
    const base::Closure& callback,
    const std::string& message_id,
    GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  callback.Run();
}

TEST_F(GCMProfileServiceTest, RegisterAndUnregister) {
  CreateGCMProfileService();

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender");
  RegisterAndWaitForCompletion(sender_ids);

  std::string expected_registration_id =
      FakeGCMClient::GenerateGCMRegistrationID(sender_ids);
  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  UnregisterAndWaitForCompletion();
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

TEST_F(GCMProfileServiceTest, Send) {
  CreateGCMProfileService();

  OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  SendAndWaitForCompletion(message);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());
}

}  // namespace gcm
