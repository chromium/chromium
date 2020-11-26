// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_process_manager.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/services/nearby/public/cpp/fake_nearby_process_manager.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/services/nearby/public/mojom/webrtc.mojom.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using NearbyConnectionsDependencies =
    location::nearby::connections::mojom::NearbyConnectionsDependencies;
using NearbyConnectionsDependenciesPtr =
    location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr;
using NearbySharingDecoderMojom = sharing::mojom::NearbySharingDecoder;

namespace {

class FakeNearbyProcessManagerFactory
    : public chromeos::nearby::NearbyProcessManagerImpl::Factory {
 public:
  FakeNearbyProcessManagerFactory() = default;
  ~FakeNearbyProcessManagerFactory() override = default;

  chromeos::nearby::FakeNearbyProcessManager* instance() { return instance_; }

 private:
  // chromeos::nearby::NearbyProcessManagerImpl::Factory:
  std::unique_ptr<chromeos::nearby::NearbyProcessManager> BuildInstance(
      chromeos::nearby::NearbyConnectionsDependenciesProvider*
          nearby_connections_dependencies_provider,
      std::unique_ptr<base::OneShotTimer> timer) override {
    auto instance =
        std::make_unique<chromeos::nearby::FakeNearbyProcessManager>();
    instance_ = instance.get();
    return instance;
  }

  chromeos::nearby::FakeNearbyProcessManager* instance_ = nullptr;
};

class MockNearbyProcessManagerObserver : public NearbyProcessManager::Observer {
 public:
  MOCK_METHOD1(OnNearbyProfileChanged, void(Profile* profile));
  MOCK_METHOD0(OnNearbyProcessStarted, void());
  MOCK_METHOD0(OnNearbyProcessStopped, void());
};

class NearbyProcessManagerTest : public testing::Test {
 public:
  NearbyProcessManagerTest() = default;
  ~NearbyProcessManagerTest() override = default;

  void SetUp() override {
    chromeos::nearby::NearbyProcessManagerImpl::Factory::SetFactoryForTesting(
        &fake_nearby_process_manager_factory_);
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    NearbyProcessManager::GetInstance().ClearActiveProfile();
  }

  void TearDown() override {
    NearbyProcessManager::GetInstance().ClearActiveProfile();
    DeleteAllProfiles();
    chromeos::nearby::NearbyProcessManagerImpl::Factory::SetFactoryForTesting(
        nullptr);
  }

  Profile* CreateProfile(const std::string& name,
                         bool is_primary_profile = false) {
    // NearbyProcessManager is only created for the primary user. Because it is
    // created when the Profile is created but it is not possible to set the
    // primary user before the Proflile is created, use
    // SetBypassPrimaryUserCheckForTesting() to bypass this.
    chromeos::nearby::NearbyProcessManagerFactory::
        SetBypassPrimaryUserCheckForTesting(is_primary_profile);
    Profile* profile = testing_profile_manager_.CreateTestingProfile(name);
    chromeos::nearby::NearbyProcessManagerFactory::
        SetBypassPrimaryUserCheckForTesting(false);

    profiles_.insert(profile);
    return profile;
  }

  base::FilePath CreateProfileOnDisk(const std::string& name) {
    base::FilePath file_path =
        testing_profile_manager_.profiles_dir().AppendASCII(name);
    ProfileAttributesStorage* storage =
        testing_profile_manager_.profile_attributes_storage();
    storage->AddProfile(file_path, base::ASCIIToUTF16(name),
                        /*gaia_id=*/std::string(),
                        /*user_name=*/base::string16(),
                        /*is_consented_primary_account=*/false,
                        /*icon_index=*/0, "TEST_ID", EmptyAccountId());
    return file_path;
  }

  void DeleteProfile(Profile* profile) {
    DoDeleteProfile(profile);
    profiles_.erase(profile);
  }

  void DeleteAllProfiles() {
    for (Profile* profile : profiles_)
      DoDeleteProfile(profile);
    profiles_.clear();
  }

  chromeos::nearby::FakeNearbyProcessManager* fake_process_manager() {
    return fake_nearby_process_manager_factory_.instance();
  }

 private:
  void DoDeleteProfile(Profile* profile) {
    NearbyProcessManager::GetInstance().OnProfileMarkedForPermanentDeletion(
        profile);
    testing_profile_manager_.DeleteTestingProfile(
        profile->GetProfileUserName());
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::MainThreadType::IO};
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  std::set<Profile*> profiles_;
  FakeNearbyProcessManagerFactory fake_nearby_process_manager_factory_;
};
}  // namespace

TEST_F(NearbyProcessManagerTest, AddRemoveObserver) {
  MockNearbyProcessManagerObserver observer;
  auto& manager = NearbyProcessManager::GetInstance();

  manager.AddObserver(&observer);
  EXPECT_TRUE(manager.observers_.HasObserver(&observer));

  manager.RemoveObserver(&observer);
  EXPECT_FALSE(manager.observers_.HasObserver(&observer));
}

TEST_F(NearbyProcessManagerTest, SetGetActiveProfile) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  EXPECT_EQ(nullptr, manager.GetActiveProfile());

  manager.SetActiveProfile(profile);
  ASSERT_NE(nullptr, manager.GetActiveProfile());
  EXPECT_EQ(profile->GetPath(), manager.GetActiveProfile()->GetPath());
}

TEST_F(NearbyProcessManagerTest, IsActiveProfile) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile_1 = CreateProfile("name 1");
  Profile* profile_2 = CreateProfile("name 2");
  EXPECT_FALSE(manager.IsActiveProfile(profile_1));
  EXPECT_FALSE(manager.IsActiveProfile(profile_2));

  manager.SetActiveProfile(profile_1);
  EXPECT_TRUE(manager.IsActiveProfile(profile_1));
  EXPECT_FALSE(manager.IsActiveProfile(profile_2));

  manager.SetActiveProfile(profile_2);
  EXPECT_FALSE(manager.IsActiveProfile(profile_1));
  EXPECT_TRUE(manager.IsActiveProfile(profile_2));

  manager.ClearActiveProfile();
  EXPECT_FALSE(manager.IsActiveProfile(profile_1));
  EXPECT_FALSE(manager.IsActiveProfile(profile_2));
}

TEST_F(NearbyProcessManagerTest, IsActiveProfile_Unloaded) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile_1 = CreateProfile("name 1");
  base::FilePath file_path_profile_2 = CreateProfileOnDisk("name 2");
  EXPECT_FALSE(manager.IsAnyProfileActive());

  g_browser_process->local_state()->SetFilePath(
      prefs::kNearbySharingActiveProfilePrefName, file_path_profile_2);

  EXPECT_TRUE(manager.IsAnyProfileActive());
  EXPECT_FALSE(manager.IsActiveProfile(profile_1));

  ProfileAttributesEntry* active_profile = manager.GetActiveProfile();
  ASSERT_TRUE(active_profile);
  EXPECT_EQ(file_path_profile_2, active_profile->GetPath());
}

TEST_F(NearbyProcessManagerTest, IsAnyProfileActive) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");

  EXPECT_FALSE(manager.IsAnyProfileActive());

  manager.SetActiveProfile(profile);
  EXPECT_TRUE(manager.IsAnyProfileActive());

  manager.ClearActiveProfile();
  EXPECT_FALSE(manager.IsAnyProfileActive());
}

TEST_F(NearbyProcessManagerTest, OnProfileDeleted_ActiveProfile) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile_1 = CreateProfile("name 1");
  Profile* profile_2 = CreateProfile("name 2");

  // Set active profile and delete it.
  manager.SetActiveProfile(profile_1);
  manager.OnProfileMarkedForPermanentDeletion(profile_1);

  // No profile should be active now.
  EXPECT_FALSE(manager.IsActiveProfile(profile_1));
  EXPECT_FALSE(manager.IsActiveProfile(profile_2));
}

TEST_F(NearbyProcessManagerTest, OnProfileDeleted_InactiveProfile) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile_1 = CreateProfile("name 1");
  Profile* profile_2 = CreateProfile("name 2");

  // Set active profile and delete inactive one.
  manager.SetActiveProfile(profile_1);
  manager.OnProfileMarkedForPermanentDeletion(profile_2);

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile_1));
  EXPECT_FALSE(manager.IsActiveProfile(profile_2));
}

TEST_F(NearbyProcessManagerTest, NearbyConnections) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name", /*is_primary_profile=*/true);
  manager.SetActiveProfile(profile);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  NearbyProcessManager::NearbyConnectionsMojom* nearby_connections =
      manager.GetOrStartNearbyConnections(profile);
  run_loop_started.Run();
  EXPECT_EQ(
      nearby_connections,
      fake_process_manager()->active_connections()->shared_remote().get());
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  EXPECT_EQ(0u, fake_process_manager()->GetNumActiveReferences());

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));
  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, NearbySharingDecoder) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name", /*is_primary_profile=*/true);
  manager.SetActiveProfile(profile);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  sharing::mojom::NearbySharingDecoder* decoder =
      manager.GetOrStartNearbySharingDecoder(profile);
  run_loop_started.Run();
  EXPECT_EQ(decoder,
            fake_process_manager()->active_decoder()->shared_remote().get());
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  EXPECT_EQ(0u, fake_process_manager()->GetNumActiveReferences());

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));
  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, NearbyConnectionsAndDecoder) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name", /*is_primary_profile=*/true);
  manager.SetActiveProfile(profile);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  NearbyProcessManager::NearbyConnectionsMojom* nearby_connections =
      manager.GetOrStartNearbyConnections(profile);
  run_loop_started.Run();
  EXPECT_EQ(
      nearby_connections,
      fake_process_manager()->active_connections()->shared_remote().get());
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  sharing::mojom::NearbySharingDecoder* decoder =
      manager.GetOrStartNearbySharingDecoder(profile);
  EXPECT_EQ(decoder,
            fake_process_manager()->active_decoder()->shared_remote().get());

  // Only one reference should have been created to serve both Nearby
  // Connections and the Nearby Share decoder.
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  EXPECT_EQ(0u, fake_process_manager()->GetNumActiveReferences());

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));
  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, SharedReferences) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name", /*is_primary_profile=*/true);
  manager.SetActiveProfile(profile);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  // Create a reference without using the class; this simulates another feature
  // (e.g., Phone Hub) using Nearby Connections.
  auto reference =
      fake_process_manager()->GetNearbyProcessReference(base::DoNothing());
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  NearbyProcessManager::NearbyConnectionsMojom* nearby_connections =
      manager.GetOrStartNearbyConnections(profile);
  run_loop_started.Run();
  EXPECT_EQ(
      nearby_connections,
      fake_process_manager()->active_connections()->shared_remote().get());
  EXPECT_EQ(2u, fake_process_manager()->GetNumActiveReferences());

  // Stop the process; there should still be an active reference.
  manager.StopProcess(profile);
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  // Delete the reference and verify the process is stopped.
  reference.reset();
  run_loop_stopped.Run();
  EXPECT_EQ(0u, fake_process_manager()->GetNumActiveReferences());

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));
  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, NearbyProcessStopsOnItsOwn) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name", /*is_primary_profile=*/true);
  manager.SetActiveProfile(profile);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  NearbyProcessManager::NearbyConnectionsMojom* nearby_connections =
      manager.GetOrStartNearbyConnections(profile);
  run_loop_started.Run();
  EXPECT_EQ(
      nearby_connections,
      fake_process_manager()->active_connections()->shared_remote().get());
  EXPECT_EQ(1u, fake_process_manager()->GetNumActiveReferences());

  // Simulate the process stopping on its own, like what would happen if it
  // crashed.
  fake_process_manager()->SimulateProcessStopped();

  // Verify that the observer was notified.
  run_loop_stopped.Run();

  // NearbyProcessManager is expected to have dropped its active reference.
  EXPECT_EQ(0u, fake_process_manager()->GetNumActiveReferences());

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));
  manager.RemoveObserver(&observer);
}
