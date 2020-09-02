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
#include "base/test/bind_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/mock_nearby_connections.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_decoder.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"
#include "chrome/services/sharing/public/mojom/sharing.mojom.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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

class FakeSharingMojoService : public sharing::mojom::Sharing {
 public:
  FakeSharingMojoService() = default;
  ~FakeSharingMojoService() override = default;

  // sharing::mojom::Sharing:
  void CreateNearbyConnections(
      NearbyConnectionsDependenciesPtr dependencies,
      CreateNearbyConnectionsCallback callback) override {
    dependencies_ = std::move(dependencies);
    mojo::PendingRemote<NearbyConnectionsMojom> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockNearbyConnections>(),
                                remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));

    run_loop_connections.Quit();
  }

  void CreateNearbySharingDecoder(
      CreateNearbySharingDecoderCallback callback) override {
    mojo::PendingRemote<NearbySharingDecoderMojom> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<MockNearbySharingDecoder>(),
                                remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote));

    run_loop_decoder.Quit();
  }

  mojo::PendingRemote<sharing::mojom::Sharing> BindSharingService() {
    return receiver.BindNewPipeAndPassRemote();
  }

  void WaitForConnections() { run_loop_connections.Run(); }

  void WaitForDecoder() { run_loop_decoder.Run(); }

  void Reset() { receiver.reset(); }

  NearbyConnectionsDependencies* dependencies() { return dependencies_.get(); }

 private:
  base::RunLoop run_loop_connections;
  base::RunLoop run_loop_decoder;
  mojo::Receiver<sharing::mojom::Sharing> receiver{this};
  NearbyConnectionsDependenciesPtr dependencies_;
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
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    NearbyProcessManager::GetInstance().ClearActiveProfile();
  }

  void TearDown() override {
    NearbyProcessManager::GetInstance().ClearActiveProfile();
    DeleteAllProfiles();
  }

  Profile* CreateProfile(const std::string& name) {
    Profile* profile = testing_profile_manager_.CreateTestingProfile(name);
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

TEST_F(NearbyProcessManagerTest, StartStopProcessWithNearbyConnections) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  auto adapter = base::MakeRefCounted<device::MockBluetoothAdapter>();
  EXPECT_CALL(*adapter, IsPresent()).WillOnce(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  // Start up a new process and wait for it to launch.
  EXPECT_NE(nullptr, manager.GetOrStartNearbyConnections(profile));
  run_loop_started.Run();

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));

  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, GetOrStartNearbyConnections) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  auto adapter = base::MakeRefCounted<device::MockBluetoothAdapter>();
  EXPECT_CALL(*adapter, IsPresent()).WillOnce(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Request a new Nearby Connections interface.
  EXPECT_NE(nullptr, manager.GetOrStartNearbyConnections(profile));
  // Expect the manager to bind a new Nearby Connections pipe.
  fake_sharing_service.WaitForConnections();

  EXPECT_TRUE(fake_sharing_service.dependencies()->bluetooth_adapter);
  EXPECT_TRUE(fake_sharing_service.dependencies()->webrtc_dependencies);
}

TEST_F(NearbyProcessManagerTest,
       GetOrStartNearbyConnections_BluetoothNotPresent) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  auto adapter = base::MakeRefCounted<device::MockBluetoothAdapter>();
  EXPECT_CALL(*adapter, IsPresent()).WillOnce(testing::Return(false));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Request a new Nearby Connections interface.
  EXPECT_NE(nullptr, manager.GetOrStartNearbyConnections(profile));
  // Expect the manager to bind a new Nearby Connections pipe.
  fake_sharing_service.WaitForConnections();

  EXPECT_FALSE(fake_sharing_service.dependencies()->bluetooth_adapter);
  EXPECT_TRUE(fake_sharing_service.dependencies()->webrtc_dependencies);
}

TEST_F(NearbyProcessManagerTest, ResetNearbyProcess) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  // Simulate a dropped mojo connection to the Nearby process.
  fake_sharing_service.Reset();

  // Expect the OnNearbyProcessStopped() callback to run.
  run_loop.Run();

  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, StartStopProcessWithNearbySharingDecoder) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  // Start up a new process and wait for it to launch.
  EXPECT_NE(nullptr, manager.GetOrStartNearbySharingDecoder(profile));
  run_loop_started.Run();

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));

  manager.RemoveObserver(&observer);
}

TEST_F(NearbyProcessManagerTest, GetOrStartNearbySharingDecoder) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  // Request a new Nearby Sharing Decoder interface.
  EXPECT_NE(nullptr, manager.GetOrStartNearbySharingDecoder(profile));
  // Expect the manager to bind a new Nearby Sharing Decoder pipe.
  fake_sharing_service.WaitForDecoder();
}

TEST_F(NearbyProcessManagerTest, GetOrStartNearbySharingDecoderAndConnections) {
  auto& manager = NearbyProcessManager::GetInstance();
  Profile* profile = CreateProfile("name");
  manager.SetActiveProfile(profile);

  // Inject fake Nearby process mojo connection.
  FakeSharingMojoService fake_sharing_service;
  manager.BindSharingProcess(fake_sharing_service.BindSharingService());

  auto adapter = base::MakeRefCounted<device::MockBluetoothAdapter>();
  EXPECT_CALL(*adapter, IsPresent()).WillOnce(testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  MockNearbyProcessManagerObserver observer;
  base::RunLoop run_loop_started;
  base::RunLoop run_loop_stopped;
  EXPECT_CALL(observer, OnNearbyProcessStarted())
      .WillOnce(testing::Invoke(&run_loop_started, &base::RunLoop::Quit));
  EXPECT_CALL(observer, OnNearbyProcessStopped())
      .WillOnce(testing::Invoke(&run_loop_stopped, &base::RunLoop::Quit));
  manager.AddObserver(&observer);

  // Request a new Nearby Sharing Decoder interface.
  EXPECT_NE(nullptr, manager.GetOrStartNearbySharingDecoder(profile));
  fake_sharing_service.WaitForDecoder();
  run_loop_started.Run();

  // Then request a new Nearby Connections interface.
  EXPECT_NE(nullptr, manager.GetOrStartNearbyConnections(profile));
  fake_sharing_service.WaitForConnections();

  // Stop the process and wait for it to finish.
  manager.StopProcess(profile);
  run_loop_stopped.Run();

  // Active profile should still be active.
  EXPECT_TRUE(manager.IsActiveProfile(profile));

  manager.RemoveObserver(&observer);
}
