// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/media_switches.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::InvokeWithoutArgs;
using testing::Return;
using MediaDrmOriginId = MediaDrmOriginIdManager::MediaDrmOriginId;

// These values must match the values specified for the implementation
// in media_drm_origin_id_manager.cc.
const char kMediaDrmOriginIds[] = "media.media_drm_origin_ids";
const char kExpirableToken[] = "expirable_token";
const char kAvailableOriginIds[] = "origin_ids";
constexpr size_t kExpectedPreferenceListSize = 2;
constexpr base::TimeDelta kExpirationDelta = base::Hours(24);
constexpr size_t kConnectionAttempts = 5;
constexpr base::TimeDelta kStartupDelay = base::Minutes(1);

}  // namespace

class MediaDrmOriginIdManagerTest : public testing::Test {
 public:
  // By default MediaDrmOriginIdManager will attempt to pre-provision origin
  // IDs at startup. For most tests this should be disabled.
  void Initialize(bool enable_preprovision_at_startup = false) {
    scoped_feature_list_.InitWithFeatureState(
        media::kMediaDrmPreprovisioningAtStartup,
        enable_preprovision_at_startup);

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    origin_id_manager_ =
        MediaDrmOriginIdManagerFactory::GetForProfile(profile_.get());
    origin_id_manager_->SetProvisioningResultCBForTesting(
        base::BindRepeating(&MediaDrmOriginIdManagerTest::GetProvisioningResult,
                            base::Unretained(this)));
  }

  MOCK_METHOD0(GetProvisioningResult, MediaDrmOriginId());

  // Call MediaDrmOriginIdManager::GetOriginId() synchronously.
  MediaDrmOriginId GetOriginId() {
    base::RunLoop run_loop;
    MediaDrmOriginId result;

    origin_id_manager_->GetOriginId(base::BindOnce(
        [](base::OnceClosure callback, MediaDrmOriginId* result,
           MediaDrmOriginIdManager::GetOriginIdStatus status,
           const MediaDrmOriginId& origin_id) {
          // If |status| = kFailure, then |origin_id| should be null.
          // Otherwise (successful), |origin_id| should be not null.
          EXPECT_EQ(
              status != MediaDrmOriginIdManager::GetOriginIdStatus::kFailure,
              origin_id.has_value());
          *result = origin_id;
          std::move(callback).Run();
        },
        run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void PreProvision() {
    origin_id_manager_->PreProvisionIfNecessary();
  }

  std::string DisplayPref(const base::Value::Dict& value) {
    std::string output;
    JSONStringValueSerializer serializer(&output);
    EXPECT_TRUE(serializer.Serialize(value));
    return output;
  }

  const base::Value::Dict& GetDict(const std::string& path) const {
    return profile_->GetTestingPrefService()->GetDict(path);
  }

  void VerifyListSize() {
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    const auto* list = dict.FindList(kAvailableOriginIds);
    EXPECT_TRUE(list);
    EXPECT_EQ(list->size(), kExpectedPreferenceListSize);
  }

  // On devices that support per-application provisioning pre-provisioning
  // should fully populate the list of pre-provisioned origin IDs (as long as
  // provisioning succeeds). On devices that don't the list should be empty.
  void CheckPreferenceForPreProvisioning() {
    DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;

    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);

    const auto* list = dict.FindList(kAvailableOriginIds);
    if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
      // PreProvision() should have pre-provisioned
      // |kExpectedPreferenceListSize| origin IDs.
      DVLOG(1) << "Per-application provisioning is supported.";
      EXPECT_TRUE(list);
      EXPECT_EQ(list->size(), kExpectedPreferenceListSize);
    } else {
      // No pre-provisioned origin IDs should exist. In fact, the dictionary
      // should not have any entries.
      DVLOG(1) << "Per-application provisioning is NOT supported.";
      EXPECT_FALSE(list);
      EXPECT_EQ(dict.size(), 0u);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MediaDrmOriginIdManager> origin_id_manager_;
};

TEST_F(MediaDrmOriginIdManagerTest, DisablePreProvisioningAtStartup) {
  // Test verifies that the construction of MediaDrmOriginIdManager is
  // successful. Pre-provisioning origin IDs at startup should be disabled
  // so no calls to GetProvisioningResult() are expected.
  Initialize();

  EXPECT_FALSE(
      base::FeatureList::IsEnabled(media::kMediaDrmPreprovisioningAtStartup));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(media::kFailUrlProvisionFetcherForTesting));

  task_environment_.RunUntilIdle();

  // Preference should not exist. Not using GetDict() as it will
  // create the preference if it doesn't exist.
  EXPECT_FALSE(
      profile_->GetTestingPrefService()->HasPrefPath(kMediaDrmOriginIds));
}

TEST_F(MediaDrmOriginIdManagerTest, OneOriginId) {
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  EXPECT_TRUE(GetOriginId());
}

TEST_F(MediaDrmOriginIdManagerTest, TwoOriginIds) {
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  MediaDrmOriginId origin_id1 = GetOriginId();
  MediaDrmOriginId origin_id2 = GetOriginId();
  EXPECT_TRUE(origin_id1);
  EXPECT_TRUE(origin_id2);
  EXPECT_NE(origin_id1, origin_id2);
}

TEST_F(MediaDrmOriginIdManagerTest, PreProvision) {
  // On devices that support per-application provisioning PreProvision() will
  // pre-provisioned several origin IDs and populate the preference. On devices
  // that don't, the list will be empty.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  PreProvision();
  task_environment_.RunUntilIdle();

  CheckPreferenceForPreProvisioning();
}

TEST_F(MediaDrmOriginIdManagerTest, PreProvisionAtStartup) {
  // Initialize without disabling kMediaDrmPreprovisioningAtStartup. Check
  // that pre-provisioning actually runs at profile creation (on devices
  // that support it).
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize(true);

  DVLOG(1) << "Advancing Time";
  task_environment_.FastForwardBy(kStartupDelay);
  task_environment_.RunUntilIdle();

  CheckPreferenceForPreProvisioning();
}

TEST_F(MediaDrmOriginIdManagerTest, PreProvisionFailAtStartup) {
  // Initialize without disabling kMediaDrmPreprovisioningAtStartup. Have
  // provisioning fail at startup, if it is attempted.
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    EXPECT_CALL(*this, GetProvisioningResult()).WillOnce(Return(std::nullopt));
  } else {
    // If per-application provisioning is NOT supported, no attempt will be made
    // to pre-provision any origin IDs at startup.
    EXPECT_CALL(*this, GetProvisioningResult()).Times(0);
  }

  Initialize(true);

  DVLOG(1) << "Advancing Time";
  task_environment_.FastForwardBy(kStartupDelay);
  task_environment_.RunUntilIdle();

  // Pre-provisioning should have failed.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto& dict = GetDict(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);

  // After failure the preference should not contain |kExpireableToken| as that
  // should only be set if the user requested an origin ID on devices that
  // support per-application provisioning.
  EXPECT_FALSE(dict.Find(kExpirableToken));

  // There should be no pre-provisioned origin IDs.
  EXPECT_FALSE(dict.Find(kAvailableOriginIds));

  // Now let provisioning succeed.
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    // If per-application provisioning is NOT supported, no attempt will be made
    // to pre-provision any origin IDs. So only expect calls if per-application
    // provisioning is supported.
    EXPECT_CALL(*this, GetProvisioningResult())
        .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  }

  // Trigger a network connection to force pre-provisioning to run again.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  task_environment_.RunUntilIdle();

  // Pre-provisioning should have run again. Should return the same result as if
  // pre-provisioning had succeeded at startup.
  CheckPreferenceForPreProvisioning();
}

TEST_F(MediaDrmOriginIdManagerTest, GetOriginIdCreatesList) {
  // After fetching an origin ID the code should pre-provision more origins
  // and fill up the list. This is independent of whether the device supports
  // per-application provisioning or not.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  GetOriginId();
  task_environment_.RunUntilIdle();

  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  VerifyListSize();
}

TEST_F(MediaDrmOriginIdManagerTest, OriginIdNotInList) {
  // After fetching one origin ID MediaDrmOriginIdManager will create the list
  // of pre-provisioned origin IDs (asynchronously). It doesn't matter if the
  // device supports per-application provisioning or not.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  MediaDrmOriginId origin_id = GetOriginId();
  task_environment_.RunUntilIdle();

  // Check that the preference does not contain |origin_id|.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto& dict = GetDict(kMediaDrmOriginIds);
  auto* list = dict.FindList(kAvailableOriginIds);
  EXPECT_FALSE(
      base::Contains(*list, base::UnguessableTokenToValue(origin_id.value())));
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningFail) {
  // Provisioning fails, so GetOriginId() returns an empty origin ID.
  EXPECT_CALL(*this, GetProvisioningResult()).WillOnce(Return(std::nullopt));
  Initialize();

  EXPECT_FALSE(GetOriginId());

  task_environment_.RunUntilIdle();

  // After failure the preference should contain |kExpireableToken| only if
  // per-application provisioning is NOT supported.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto& dict = GetDict(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);

  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    DVLOG(1) << "Per-application provisioning is supported.";
    EXPECT_FALSE(dict.Find(kExpirableToken));
  } else {
    DVLOG(1) << "Per-application provisioning is NOT supported.";
    EXPECT_TRUE(dict.Find(kExpirableToken));
  }
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningSuccessAfterFail) {
  // Provisioning fails, so GetOriginId() returns an empty origin ID.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillOnce(Return(std::nullopt))
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  EXPECT_FALSE(GetOriginId());
  EXPECT_TRUE(GetOriginId());  // Provisioning will succeed on the second call.

  // Let pre-provisioning of other origin IDs finish.
  task_environment_.RunUntilIdle();

  // After success the preference should not contain |kExpireableToken|.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto& dict = GetDict(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  EXPECT_FALSE(dict.Find(kExpirableToken));

  // As well, the list of available pre-provisioned origin IDs should be full.
  VerifyListSize();
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningAfterExpiration) {
  // Provisioning fails, so GetOriginId() returns an empty origin ID.
  DVLOG(1) << "Current time: " << base::Time::Now();
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillOnce(Return(std::nullopt))
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  EXPECT_FALSE(GetOriginId());
  task_environment_.RunUntilIdle();

  {
    // Check that |kAvailableOriginIds| in the preference is empty.
    DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    EXPECT_FALSE(dict.Find(kAvailableOriginIds));

    // Check that |kExpirableToken| is only set if per-application provisioning
    // is not supported.
    EXPECT_TRUE(
        media::MediaDrmBridge::IsPerApplicationProvisioningSupported() ||
        dict.Find(kExpirableToken));
  }

  // Advance clock by |kExpirationDelta| (plus one minute) and attempt to
  // pre-provision more origin Ids.
  DVLOG(1) << "Advancing Time";
  task_environment_.FastForwardBy(kExpirationDelta);
  task_environment_.FastForwardBy(base::Minutes(1));
  DVLOG(1) << "Adjusted time: " << base::Time::Now();
  PreProvision();
  task_environment_.RunUntilIdle();

  // Look at the preference again.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds << " again";
  auto& dict = GetDict(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  auto* list = dict.FindList(kAvailableOriginIds);

  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    // If per-application provisioning is supported, it's OK to attempt
    // to pre-provision origin IDs any time.
    DVLOG(1) << "Per-application provisioning is supported.";
    ASSERT_TRUE(list);
    EXPECT_EQ(list->size(), kExpectedPreferenceListSize);
  } else {
    // Per-application provisioning is not supported, so attempting to
    // pre-provision origin IDs after |kExpirationDelta| should not do anything.
    // As well, |kExpirableToken| should be removed.
    DVLOG(1) << "Per-application provisioning is NOT supported.";
    EXPECT_FALSE(list);
  }
  EXPECT_FALSE(dict.Find(kExpirableToken));
}

TEST_F(MediaDrmOriginIdManagerTest, Incognito) {
  // No MediaDrmOriginIdManager should be created for an incognito profile.
  Initialize();
  auto* incognito_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(
      MediaDrmOriginIdManagerFactory::GetForProfile(incognito_profile));
}

TEST_F(MediaDrmOriginIdManagerTest, NetworkChange) {
  // Try to pre-provision a bunch of origin IDs. Provisioning will fail, so
  // there will not be a bunch of origin IDs created. However, it should be
  // watching for a network change.
  // TODO(crbug.com/41433110): Currently the code returns an origin ID even if
  // provisioning fails. Update this once it returns an empty origin ID when
  // pre-provisioning fails.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillOnce(Return(std::nullopt))
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  EXPECT_FALSE(GetOriginId());
  task_environment_.RunUntilIdle();

  // Check that |kAvailableOriginIds| in the preference is empty.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  {
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    EXPECT_FALSE(dict.Find(kAvailableOriginIds));
  }

  // Provisioning will now "succeed", so trigger a network change to
  // unconnected.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  task_environment_.RunUntilIdle();

  // Check that |kAvailableOriginIds| is still empty.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds << " again";
  {
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    EXPECT_FALSE(dict.Find(kAvailableOriginIds));
  }

  // Now trigger a network change to connected.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  task_environment_.RunUntilIdle();

  // Pre-provisioning should have run and filled up the list.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds << " again";
  VerifyListSize();
}

TEST_F(MediaDrmOriginIdManagerTest, NetworkChangeFails) {
  // Try to pre-provision a bunch of origin IDs. Provisioning will fail the
  // first time, so there will not be a bunch of origin IDs created. However, it
  // should be watching for a network change, and will try again on the next
  // |kConnectionAttempts| connections to a network. GetProvisioningResult()
  // should only be called once for the GetOriginId() call +
  // |kConnectionAttempts| when a network connection is detected.
  // TODO(crbug.com/41433110): Currently the code returns an origin ID even if
  // provisioning fails. Update this once it returns an empty origin ID when
  // pre-provisioning fails.
  EXPECT_CALL(*this, GetProvisioningResult())
      .Times(kConnectionAttempts + 1)
      .WillOnce(Return(std::nullopt));
  Initialize();

  EXPECT_FALSE(GetOriginId());
  task_environment_.RunUntilIdle();

  // Check that |kAvailableOriginIds| in the preference is empty.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  {
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    EXPECT_FALSE(dict.Find(kAvailableOriginIds));
  }

  // Trigger multiple network connections (provisioning still fails). Call more
  // than |kConnectionAttempts| to ensure that the network change is ignored
  // after several failed attempts.
  for (size_t i = 0; i < kConnectionAttempts + 3; ++i) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
    task_environment_.RunUntilIdle();
  }

  // Check that |kAvailableOriginIds| is still empty.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds << " again";
  {
    auto& dict = GetDict(kMediaDrmOriginIds);
    DVLOG(1) << DisplayPref(dict);
    EXPECT_FALSE(dict.Find(kAvailableOriginIds));
  }
}

TEST_F(MediaDrmOriginIdManagerTest, InvalidEntry) {
  // After fetching an origin ID the code should pre-provision more origins
  // and fill up the list. This is independent of whether the device supports
  // per-application provisioning or not.
  EXPECT_CALL(*this, GetProvisioningResult())
      .WillRepeatedly(InvokeWithoutArgs(&base::UnguessableToken::Create));
  Initialize();

  EXPECT_TRUE(GetOriginId());
  task_environment_.RunUntilIdle();
  VerifyListSize();

  // Fetching the first origin ID has now filled up the list. Replace the
  // first entry in the list with something (a boolean value) that cannot
  // be converted to a base::UnguessableToken.
  {
    ScopedDictPrefUpdate update(profile_->GetTestingPrefService(),
                                kMediaDrmOriginIds);
    base::Value::List* origin_ids = update->FindList(kAvailableOriginIds);
    EXPECT_FALSE(origin_ids->empty());
    auto first_entry = origin_ids->begin();
    *first_entry = base::Value(true);
  }

  // Next GetOriginId() call should attempt to use the invalid entry. Since
  // it's invalid, a new origin ID will be created and used. And then an
  // additional one is created to replace the one that should have been taken
  // from the list.
  EXPECT_TRUE(GetOriginId());
  task_environment_.RunUntilIdle();
  VerifyListSize();
}
