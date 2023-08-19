// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#endif

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
  return profile->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateProxyResolvingSocketFactory(std::move(receiver));
}

void RequestProxyResolvingSocketFactory(
    Profile* profile,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                profile, service, std::move(receiver)));
}

std::unique_ptr<KeyedService> BuildGCMProfileService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  return std::make_unique<gcm::GCMProfileService>(
      profile->GetPrefs(), profile->GetPath(),
      base::BindRepeating(&RequestProxyResolvingSocketFactory, profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      network::TestNetworkConnectionTracker::GetInstance(),
      chrome::GetChannel(),
      gcm::GetProductCategoryForSubtypes(profile->GetPrefs()),
      IdentityManagerFactory::GetForProfile(profile),
      std::unique_ptr<gcm::GCMClientFactory>(
          new gcm::FakeGCMClientFactory(content::GetUIThreadTaskRunner({}),
                                        content::GetIOThreadTaskRunner({}))),
      content::GetUIThreadTaskRunner({}), content::GetIOThreadTaskRunner({}),
      blocking_task_runner);
}

}  // namespace

class GCMProfileServiceTest : public testing::Test {
 public:
  GCMProfileServiceTest(const GCMProfileServiceTest&) = delete;
  GCMProfileServiceTest& operator=(const GCMProfileServiceTest&) = delete;

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

  void RegisterCompleted(base::OnceClosure callback,
                         const std::string& registration_id,
                         GCMClient::Result result);
  void UnregisterCompleted(base::OnceClosure callback,
                           GCMClient::Result result);
  void SendCompleted(base::OnceClosure callback,
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
  raw_ptr<GCMProfileService, DanglingUntriaged> gcm_profile_service_;
  std::unique_ptr<FakeGCMAppHandler> gcm_app_handler_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  GCMClient::Result unregistration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;
};

GCMProfileServiceTest::GCMProfileServiceTest()
    : gcm_profile_service_(nullptr),
      gcm_app_handler_(new FakeGCMAppHandler),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR) {}

GCMProfileServiceTest::~GCMProfileServiceTest() {
}

FakeGCMClient* GCMProfileServiceTest::GetGCMClient() const {
  return static_cast<FakeGCMClient*>(
      gcm_profile_service_->driver()->GetGCMClientForTesting());
}

void GCMProfileServiceTest::SetUp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
#endif
  TestingProfile::Builder builder;
  profile_ = builder.Build();
}

void GCMProfileServiceTest::TearDown() {
  gcm_profile_service_->driver()->RemoveAppHandler(kTestAppID);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  profile_.reset();
  ash::ConciergeClient::Shutdown();
#endif
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
      kTestAppID, sender_ids,
      base::BindOnce(&GCMProfileServiceTest::RegisterCompleted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::UnregisterAndWaitForCompletion() {
  base::RunLoop run_loop;
  gcm_profile_service_->driver()->Unregister(
      kTestAppID,
      base::BindOnce(&GCMProfileServiceTest::UnregisterCompleted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::SendAndWaitForCompletion(
    const OutgoingMessage& message) {
  base::RunLoop run_loop;
  gcm_profile_service_->driver()->Send(
      kTestAppID, kUserID, message,
      base::BindOnce(&GCMProfileServiceTest::SendCompleted,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::RegisterCompleted(
    base::OnceClosure callback,
    const std::string& registration_id,
    GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  std::move(callback).Run();
}

void GCMProfileServiceTest::UnregisterCompleted(base::OnceClosure callback,
                                                GCMClient::Result result) {
  unregistration_result_ = result;
  std::move(callback).Run();
}

void GCMProfileServiceTest::SendCompleted(base::OnceClosure callback,
                                          const std::string& message_id,
                                          GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  std::move(callback).Run();
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
