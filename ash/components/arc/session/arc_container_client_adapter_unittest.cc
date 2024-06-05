// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_container_client_adapter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace arc {

namespace {

class ArcContainerClientAdapterTest : public testing::Test,
                                      public ArcClientAdapter::Observer {
 public:
  ArcContainerClientAdapterTest() = default;
  ~ArcContainerClientAdapterTest() override = default;
  ArcContainerClientAdapterTest(const ArcContainerClientAdapterTest&) = delete;
  ArcContainerClientAdapterTest& operator=(
      const ArcContainerClientAdapterTest&) = delete;

  void SetUp() override {
    ash::SessionManagerClient::InitializeFake();
    client_adapter_ = CreateArcContainerClientAdapter();
    client_adapter_->AddObserver(this);
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
  }

  void TearDown() override {
    client_adapter_ = nullptr;
    ash::SessionManagerClient::Shutdown();
  }

  // ArcClientAdapter::Observer:
  void ArcInstanceStopped(bool is_system_shutdown) override {
    is_system_shutdown_ = is_system_shutdown;
  }

 protected:
  ArcClientAdapter* client_adapter() { return client_adapter_.get(); }

  const std::optional<bool>& is_system_shutdown() const {
    return is_system_shutdown_;
  }

 private:
  std::unique_ptr<ArcClientAdapter> client_adapter_;
  content::BrowserTaskEnvironment browser_task_environment_;
  std::optional<bool> is_system_shutdown_;
};

void OnMiniInstanceStarted(bool result) {
  DCHECK(result);
}

TEST_F(ArcContainerClientAdapterTest, ArcInstanceStopped) {
  ash::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped(
      login_manager::ArcContainerStopReason::USER_REQUEST);
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_FALSE(is_system_shutdown().value());
}

TEST_F(ArcContainerClientAdapterTest, ArcInstanceStoppedSystemShutdown) {
  ash::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped(
      login_manager::ArcContainerStopReason::SESSION_MANAGER_SHUTDOWN);
  ASSERT_TRUE(is_system_shutdown().has_value());
  EXPECT_TRUE(is_system_shutdown().value());
}

// b/164816080 This test ensures that a new container instance that is
// created while handling the shutting down of the previous instance,
// doesn't incorrectly receive the shutdown event as well.
TEST_F(ArcContainerClientAdapterTest,
       DoesNotGetArcInstanceStoppedOnNestedInstance) {
  class Observer : public ArcClientAdapter::Observer {
   public:
    explicit Observer(Observer* child_observer)
        : child_observer_(child_observer) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    ~Observer() override {
      if (child_observer_ && nested_client_adapter_)
        nested_client_adapter_->RemoveObserver(child_observer_);
    }

    bool stopped_called() const { return stopped_called_; }

    // ArcClientAdapter::Observer:
    void ArcInstanceStopped(bool is_system_shutdown) override {
      stopped_called_ = true;

      if (child_observer_) {
        nested_client_adapter_ = CreateArcContainerClientAdapter();
        nested_client_adapter_->AddObserver(child_observer_);
      }
    }

   private:
    const raw_ptr<Observer> child_observer_;
    std::unique_ptr<ArcClientAdapter> nested_client_adapter_;
    bool stopped_called_ = false;
  };

  Observer child_observer(nullptr);
  Observer parent_observer(&child_observer);
  client_adapter()->AddObserver(&parent_observer);
  absl::Cleanup teardown = [this, &parent_observer] {
    client_adapter()->RemoveObserver(&parent_observer);
  };

  ash::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped(
      login_manager::ArcContainerStopReason::USER_REQUEST);

  EXPECT_TRUE(parent_observer.stopped_called());
  EXPECT_FALSE(child_observer.stopped_called());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_DisableMediaStoreMaintenance) {
  StartParams start_params;
  start_params.disable_media_store_maintenance = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_disable_media_store_maintenance());
  EXPECT_TRUE(request.disable_media_store_maintenance());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_DisableDownloadProviderDefault) {
  StartParams start_params;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_disable_download_provider());
  EXPECT_FALSE(request.disable_download_provider());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_DisableDownloadProviderOn) {
  StartParams start_params;
  start_params.disable_download_provider = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_disable_download_provider());
  EXPECT_TRUE(request.disable_download_provider());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_DoNotUseDevCachesByDefault) {
  StartParams start_params;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_use_dev_caches());
  EXPECT_FALSE(request.use_dev_caches());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_UseDevCachesSet) {
  StartParams start_params;
  start_params.use_dev_caches = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_use_dev_caches());
  EXPECT_TRUE(request.use_dev_caches());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_ArcSignedInDefault) {
  StartParams start_params;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_arc_signed_in());
  EXPECT_FALSE(request.arc_signed_in());
}

TEST_F(ArcContainerClientAdapterTest, Startrc_ArcSignedIn) {
  StartParams start_params;
  start_params.arc_signed_in = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_arc_signed_in());
  EXPECT_TRUE(request.arc_signed_in());
}

TEST_F(ArcContainerClientAdapterTest, ArcTTSCachingDefault) {
  StartParams start_params;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_enable_tts_caching());
  EXPECT_FALSE(request.enable_tts_caching());
}

TEST_F(ArcContainerClientAdapterTest, ArcTTSCachingEnabled) {
  StartParams start_params;
  start_params.enable_tts_caching = true;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_enable_tts_caching());
  EXPECT_TRUE(request.enable_tts_caching());
}

TEST_F(ArcContainerClientAdapterTest, ConvertUpgradeParams_SkipTtsCacheSetup) {
  UpgradeParams upgrade_params;
  upgrade_params.skip_tts_cache = true;
  client_adapter()->UpgradeArc(std::move(upgrade_params),
                               base::BindOnce(&OnMiniInstanceStarted));
  const auto& upgrade_request =
      ash::FakeSessionManagerClient::Get()->last_upgrade_arc_request();
  EXPECT_TRUE(upgrade_request.skip_tts_cache());
}

TEST_F(ArcContainerClientAdapterTest,
       ConvertUpgradeParams_EnableTtsCacheSetup) {
  UpgradeParams upgrade_params;
  upgrade_params.skip_tts_cache = false;
  client_adapter()->UpgradeArc(std::move(upgrade_params),
                               base::BindOnce(&OnMiniInstanceStarted));
  const auto& upgrade_request =
      ash::FakeSessionManagerClient::Get()->last_upgrade_arc_request();
  EXPECT_FALSE(upgrade_request.skip_tts_cache());
}

TEST_F(ArcContainerClientAdapterTest, StartArc_EnableArcAttestationDefault) {
  StartParams start_params;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_FALSE(request.enable_arc_attestation());
}

struct DalvikMemoryProfileTestParam {
  // Requested profile.
  StartParams::DalvikMemoryProfile profile;
  // Expected value passed to DBus.
  StartArcMiniInstanceRequest_DalvikMemoryProfile expectation;
};

constexpr DalvikMemoryProfileTestParam kDalvikMemoryProfileTestCases[] = {
    {StartParams::DalvikMemoryProfile::DEFAULT,
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_DEFAULT},
    {StartParams::DalvikMemoryProfile::M4G,
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_4G},
    {StartParams::DalvikMemoryProfile::M8G,
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_8G},
    {StartParams::DalvikMemoryProfile::M16G,
     StartArcMiniInstanceRequest_DalvikMemoryProfile_MEMORY_PROFILE_16G}};

class ArcContainerClientAdapterDalvikMemoryProfileTest
    : public ArcContainerClientAdapterTest,
      public testing::WithParamInterface<DalvikMemoryProfileTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcContainerClientAdapterDalvikMemoryProfileTest,
                         ::testing::ValuesIn(kDalvikMemoryProfileTestCases));

TEST_P(ArcContainerClientAdapterDalvikMemoryProfileTest, Profile) {
  const auto& test_param = GetParam();
  StartParams start_params;
  start_params.dalvik_memory_profile = test_param.profile;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_dalvik_memory_profile());
  EXPECT_EQ(test_param.expectation, request.dalvik_memory_profile());
}

struct HostUreadaheadModeTestParam {
  // Requested profile.
  StartParams::HostUreadaheadMode mode;
  // Expected value passed to DBus.
  StartArcMiniInstanceRequest_HostUreadaheadMode expectation;
};

constexpr HostUreadaheadModeTestParam kHostUreadaheadModeTestCases[] = {
    {StartParams::HostUreadaheadMode::MODE_READAHEAD,
     StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_DEFAULT},
    {StartParams::HostUreadaheadMode::MODE_GENERATE,
     StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_GENERATE},
    {StartParams::HostUreadaheadMode::MODE_DISABLED,
     StartArcMiniInstanceRequest_HostUreadaheadMode_MODE_DISABLED}};

class ArcContainerClientAdapterHostUreadaheadModeTest
    : public ArcContainerClientAdapterTest,
      public testing::WithParamInterface<HostUreadaheadModeTestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcContainerClientAdapterHostUreadaheadModeTest,
                         ::testing::ValuesIn(kHostUreadaheadModeTestCases));

TEST_P(ArcContainerClientAdapterHostUreadaheadModeTest, Mode) {
  const auto& test_param = GetParam();
  StartParams start_params;
  start_params.host_ureadahead_mode = test_param.mode;
  client_adapter()->StartMiniArc(std::move(start_params),
                                 base::BindOnce(&OnMiniInstanceStarted));
  const auto& request = ash::FakeSessionManagerClient::Get()
                            ->last_start_arc_mini_container_request();
  EXPECT_TRUE(request.has_host_ureadahead_mode());
  EXPECT_EQ(test_param.expectation, request.host_ureadahead_mode());
}
}  // namespace

}  // namespace arc
