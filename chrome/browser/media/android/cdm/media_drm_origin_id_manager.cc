// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/media_switches.h"
#include "media/base/provision_fetcher.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

// The storage will be managed by PrefService. All data will be stored in a
// dictionary under the key "media.media_drm_origin_ids". The dictionary is
// structured as follows:
//
// {
//     "origin_ids": [ $origin_id, ... ]
//     "expirable_token": $expiration_time,
// }
//
// If specified, "expirable_token" is stored as a string representing the
// int64_t (base::NumberToString()) form of the number of microseconds since
// Windows epoch (1601-01-01 00:00:00 UTC). It is the latest time that this
// code should attempt to pre-provision more origins on some devices.

namespace {

const char kMediaDrmOriginIds[] = "media.media_drm_origin_ids";
const char kExpirableToken[] = "expirable_token";
const char kOriginIds[] = "origin_ids";

// The maximum number of origin IDs to pre-provision. Chosen to be small to
// minimize provisioning server load.
// TODO(jrummell): Adjust this value if needed after initial launch.
constexpr int kMaxPreProvisionedOriginIds = 2;

// The maximum number of origin IDs logged to UMA.
constexpr int kUMAMaxPreProvisionedOriginIds = 10;

// "expirable_token" is only good for 24 hours.
constexpr base::TimeDelta kExpirationDelta = base::TimeDelta::FromHours(24);

// Time to wait before attempting pre-provisioning at startup (if enabled).
constexpr base::TimeDelta kStartupDelay = base::TimeDelta::FromMinutes(1);

// Time to wait before logging number of pre-provisioned origin IDs at startup
constexpr base::TimeDelta kCheckDelay = base::TimeDelta::FromMinutes(5);
static_assert(kCheckDelay > kStartupDelay,
              "Must allow time for pre-provisioning to run first");

// When unable to get an origin ID, only attempt to pre-provision more if
// pre-provision is called within |kExpirationDelta| of the time of this
// failure.
void SetExpirableTokenIfNeeded(PrefService* const pref_service) {
  DVLOG(3) << __func__;

  // This is not needed on devices that support per-application provisioning.
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported())
    return;

  DictionaryPrefUpdate update(pref_service, kMediaDrmOriginIds);
  auto* origin_id_dict = update.Get();
  origin_id_dict->SetKey(
      kExpirableToken,
      base::CreateTimeValue(base::Time::Now() + kExpirationDelta));
}

void RemoveExpirableToken(base::Value* origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(origin_id_dict);
  DCHECK(origin_id_dict->is_dict());

  origin_id_dict->RemoveKey(kExpirableToken);
}

// On devices that don't support per-application provisioning attempts to
// pre-provision more origin IDs should only happen if an origin ID was
// requested recently and failed. This code checks that the time saved in
// |kExpirableToken| is less than the current time. If |kExpirableToken| doesn't
// exist then this function returns false. On devices that support per
// application provisioning pre-provisioning is always allowed. If
// |kExpirableToken| is expired or corrupt, it will be removed for privacy
// reasons.
bool CanPreProvision(base::Value* origin_id_dict) {
  DVLOG(3) << __func__;

  // On devices that support per-application provisioning, this is always true.
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported())
    return true;

  // Device doesn't support per-application provisioning, so check if
  // "expirable_token" is still valid.
  if (!origin_id_dict || !origin_id_dict->is_dict())
    return false;

  const base::Value* token_value =
      origin_id_dict->FindKeyOfType(kExpirableToken, base::Value::Type::STRING);
  if (!token_value)
    return false;

  base::Time expiration_time;
  if (!base::GetValueAsTime(*token_value, &expiration_time)) {
    RemoveExpirableToken(origin_id_dict);
    return false;
  }

  if (base::Time::Now() > expiration_time) {
    DVLOG(3) << __func__ << ": Token exists but has expired";
    RemoveExpirableToken(origin_id_dict);
    return false;
  }

  return true;
}

int CountAvailableOriginIds(const base::Value* origin_id_dict) {
  DVLOG(3) << __func__;

  if (!origin_id_dict || !origin_id_dict->is_dict())
    return 0;

  const base::Value* origin_ids =
      origin_id_dict->FindKeyOfType(kOriginIds, base::Value::Type::LIST);
  if (!origin_ids)
    return 0;

  DVLOG(3) << "count: " << origin_ids->GetList().size();
  return origin_ids->GetList().size();
}

base::UnguessableToken TakeFirstOriginId(PrefService* const pref_service) {
  DVLOG(3) << __func__;

  DictionaryPrefUpdate update(pref_service, kMediaDrmOriginIds);
  auto* origin_id_dict = update.Get();
  DCHECK(origin_id_dict->is_dict());

  base::Value* origin_ids =
      origin_id_dict->FindKeyOfType(kOriginIds, base::Value::Type::LIST);
  if (!origin_ids)
    return base::UnguessableToken::Null();

  auto& origin_ids_list = origin_ids->GetList();
  if (origin_ids_list.empty())
    return base::UnguessableToken::Null();

  base::UnguessableToken result;
  auto first_entry = origin_ids_list.begin();
  if (!base::GetValueAsUnguessableToken(*first_entry, &result))
    return base::UnguessableToken::Null();

  // Update the setting with the value removed.
  origin_ids_list.erase(first_entry);
  origin_id_dict->SetKey(kOriginIds, base::Value(origin_ids_list));
  return result;
}

void AddOriginId(base::Value* origin_id_dict,
                 const base::UnguessableToken& origin_id) {
  DVLOG(3) << __func__;
  DCHECK(origin_id_dict);
  DCHECK(origin_id_dict->is_dict());

  base::Value* origin_ids =
      origin_id_dict->FindKeyOfType(kOriginIds, base::Value::Type::LIST);
  if (!origin_ids) {
    base::Value::ListStorage list;
    list.push_back(base::CreateUnguessableTokenValue(origin_id));
    origin_id_dict->SetKey(kOriginIds, base::Value(list));
    return;
  }

  auto& origin_ids_list = origin_ids->GetList();
  origin_ids_list.push_back(base::CreateUnguessableTokenValue(origin_id));
  origin_id_dict->SetKey(kOriginIds, base::Value(origin_ids_list));
}

// Helper class that creates a new origin ID and provisions it for both L1
// (if available) and L3. This class self destructs when provisioning is done
// (successfully or not).
class MediaDrmProvisionHelper {
 public:
  using ProvisionedOriginIdCB = base::OnceCallback<void(
      bool success,
      const MediaDrmOriginIdManager::MediaDrmOriginId& origin_id)>;

  MediaDrmProvisionHelper() {
    DVLOG(1) << __func__;
    DCHECK(media::MediaDrmBridge::IsPerOriginProvisioningSupported());
  }

  void Provision(ProvisionedOriginIdCB callback) {
    DVLOG(1) << __func__;

    auto* network_context_manager =
        g_browser_process->system_network_context_manager();
    if (!network_context_manager) {
      // system_network_context_manager() returns nullptr in unit tests.
      DLOG(WARNING) << "Failed to provision origin ID as no "
                       "system_network_context_manager";
      std::move(callback).Run(false, base::nullopt);
      return;
    }

    complete_callback_ = std::move(callback);
    origin_id_ = base::UnguessableToken::Create();
    create_fetcher_cb_ = base::BindRepeating(
        &content::CreateProvisionFetcher,
        network_context_manager->GetSharedURLLoaderFactory());

    // Try provisioning for L3 first.
    media_drm_bridge_ = media::MediaDrmBridge::CreateWithoutSessionSupport(
        kWidevineKeySystem, origin_id_.ToString(),
        media::MediaDrmBridge::SECURITY_LEVEL_3, create_fetcher_cb_);
    if (!media_drm_bridge_) {
      // Unable to create mediaDrm for L3, so try L1.
      DVLOG(1) << "Unable to create MediaDrmBridge for L3.";
      ProvisionLevel1(false);
      return;
    }

    // Use of base::Unretained() is safe as ProvisionLevel1() eventually calls
    // ProvisionDone() which destructs this object.
    media_drm_bridge_->Provision(base::BindOnce(
        &MediaDrmProvisionHelper::ProvisionLevel1, base::Unretained(this)));
  }

 private:
  friend class base::RefCounted<MediaDrmProvisionHelper>;
  ~MediaDrmProvisionHelper() { DVLOG(1) << __func__; }

  void ProvisionLevel1(bool L3_success) {
    DVLOG(1) << __func__ << " origin_id: " << origin_id_.ToString()
             << ", L3_success: " << L3_success;

    // Try L1. This replaces the previous |media_drm_bridge_| as it is no longer
    // needed.
    media_drm_bridge_ = media::MediaDrmBridge::CreateWithoutSessionSupport(
        kWidevineKeySystem, origin_id_.ToString(),
        media::MediaDrmBridge::SECURITY_LEVEL_1, create_fetcher_cb_);
    if (!media_drm_bridge_) {
      // Unable to create MediaDrm for L1, so quit. Note that L3 provisioning
      // may or may not have worked.
      DVLOG(1) << "Unable to create MediaDrmBridge for L1.";
      ProvisionDone(L3_success, false);
      return;
    }

    // Use of base::Unretained() is safe as ProvisionDone() destructs this
    // object.
    media_drm_bridge_->Provision(
        base::BindOnce(&MediaDrmProvisionHelper::ProvisionDone,
                       base::Unretained(this), L3_success));
  }

  void ProvisionDone(bool L3_success, bool L1_success) {
    DVLOG(1) << __func__ << " origin_id: " << origin_id_.ToString()
             << ", L1_success: " << L1_success
             << ", L3_success: " << L3_success;

    const bool success = L1_success || L3_success;
    LOG_IF(WARNING, !success) << "Failed to provision origin ID";
    std::move(complete_callback_)
        .Run(success,
             success ? base::make_optional(origin_id_) : base::nullopt);
    delete this;
  }

  media::CreateFetcherCB create_fetcher_cb_;
  ProvisionedOriginIdCB complete_callback_;
  base::UnguessableToken origin_id_;
  scoped_refptr<media::MediaDrmBridge> media_drm_bridge_;
};

}  // namespace

// Watch for the device being connected to a network and call
// PreProvisionIfNecessary(). This object is owned by MediaDrmOriginIdManager
// and will be deleted when the manager goes away, so it is safe to keep a
// direct reference to the manager.
class MediaDrmOriginIdManager::NetworkObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit NetworkObserver(MediaDrmOriginIdManager* parent) : parent_(parent) {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }

  ~NetworkObserver() override {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }

  // Returns true if this NetworkObserver has seen a connection to the network
  // more than |kMaxAttemptsAllowed| times.
  bool MaxAttemptsExceeded() const {
    constexpr int kMaxAttemptsAllowed = 5;
    return number_of_attempts_ >= kMaxAttemptsAllowed;
  }

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    if (type == network::mojom::ConnectionType::CONNECTION_NONE)
      return;

    ++number_of_attempts_;
    parent_->PreProvisionIfNecessary();
  }

 private:
  // Use of raw pointer is okay as |parent_| owns this object.
  MediaDrmOriginIdManager* const parent_;
  int number_of_attempts_ = 0;
};

// static
void MediaDrmOriginIdManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMediaDrmOriginIds);
}

MediaDrmOriginIdManager::MediaDrmOriginIdManager(PrefService* pref_service)
    : pref_service_(pref_service) {
  DVLOG(1) << __func__;
  DCHECK(pref_service_);

  // This manager can be started when the user's profile is loaded, if
  // |kMediaDrmPreprovisioning| is enabled. If that flag is not set, then this
  // manager is started only when a pre-provisioned origin ID is needed.

  // If |kMediaDrmPreprovisioningAtStartup| is enabled, attempt to
  // pre-provisioning origin IDs when started. If this manager is only loaded
  // when needed, then the caller is most likely going to call GetOriginId()
  // right away, so it will pre-provision extra origin IDs if necessary (and the
  // posted task won't do anything). |kMediaDrmPreprovisioningAtStartup| is also
  // used by testing so that it can check pre-provisioning directly.
  if (base::FeatureList::IsEnabled(media::kMediaDrmPreprovisioningAtStartup)) {
    // Running this after a delay of |kStartupDelay| in order to not do too much
    // extra work when the profile is loaded.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MediaDrmOriginIdManager::PreProvisionIfNecessary,
                       weak_factory_.GetWeakPtr()),
        kStartupDelay);
  }

  // In order to determine how devices are pre-provisioning origin IDs, post a
  // task to check how many pre-provisioned origin IDs are available after a
  // delay of |kCheckDelay|.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &MediaDrmOriginIdManager::RecordCountOfPreprovisionedOriginIds,
          weak_factory_.GetWeakPtr()),
      kCheckDelay);
}

MediaDrmOriginIdManager::~MediaDrmOriginIdManager() {
  // Reject any pending requests.
  while (!pending_provisioned_origin_id_cbs_.empty()) {
    std::move(pending_provisioned_origin_id_cbs_.front())
        .Run(GetOriginIdStatus::kFailure, base::nullopt);
    pending_provisioned_origin_id_cbs_.pop();
  }
}

void MediaDrmOriginIdManager::PreProvisionIfNecessary() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If pre-provisioning already running, no need to start it again.
  if (is_provisioning_)
    return;

  // On devices that need to, check that the user has recently requested
  // an origin ID. If not, then skip pre-provisioning on those devices.
  DictionaryPrefUpdate update(pref_service_, kMediaDrmOriginIds);
  if (!CanPreProvision(update.Get())) {
    // Disable any network monitoring, if it exists.
    network_observer_.reset();
    return;
  }

  // No need to pre-provision if there are already enough existing
  // pre-provisioned origin IDs.
  if (CountAvailableOriginIds(update.Get()) >= kMaxPreProvisionedOriginIds) {
    // Disable any network monitoring, if it exists.
    network_observer_.reset();
    return;
  }

  // Attempt to pre-provision more origin IDs in the near future.
  is_provisioning_ = true;
  StartProvisioningAsync();
}

void MediaDrmOriginIdManager::GetOriginId(ProvisionedOriginIdCB callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // See if there is one already pre-provisioned that can be used.
  base::UnguessableToken origin_id = TakeFirstOriginId(pref_service_);

  // Start a task to pre-provision more origin IDs if we are currently not doing
  // so. If there is one available then we need to replace it. If there are
  // none, we need one.
  if (!is_provisioning_) {
    is_provisioning_ = true;
    StartProvisioningAsync();
  }

  // If no pre-provisioned origin ID currently available, so save the callback
  // for when provisioning creates one and we're done.
  if (!origin_id) {
    pending_provisioned_origin_id_cbs_.push(std::move(callback));
    return;
  }

  // There is an origin ID available so pass it to the caller.
  std::move(callback).Run(GetOriginIdStatus::kSuccessWithPreProvisionedOriginId,
                          origin_id);
}

void MediaDrmOriginIdManager::StartProvisioningAsync() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_provisioning_);

  // Run StartProvisioning() later, on this thread.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MediaDrmOriginIdManager::StartProvisioning,
                                weak_factory_.GetWeakPtr()));
}

void MediaDrmOriginIdManager::StartProvisioning() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_provisioning_);

  if (provisioning_result_cb_for_testing_) {
    // MediaDrm can't provision an origin ID during unittests, so create a new
    // origin ID and pretend it was provisioned or not depending on the result
    // from |provisioning_result_cb_for_testing_|.
    OriginIdProvisioned(provisioning_result_cb_for_testing_.Run(),
                        base::UnguessableToken::Create());
    return;
  }

  // MediaDrmProvisionHelper will delete itself when it's done.
  auto* helper = new MediaDrmProvisionHelper();
  helper->Provision(
      base::BindOnce(&MediaDrmOriginIdManager::OriginIdProvisioned,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmOriginIdManager::OriginIdProvisioned(
    bool success,
    const MediaDrmOriginId& origin_id) {
  DVLOG(1) << __func__
           << " origin_id: " << (origin_id ? origin_id->ToString() : "null")
           << ", success: " << success;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_provisioning_);

  if (!success) {
    // Unable to provision an origin ID, most likely due to being unable to
    // connect to a provisioning server. Set up a NetworkObserver to detect when
    // we're connected to a network so that we can try again. If there is
    // already a NetworkObserver and provisioning has failed multiple times,
    // stop watching for network changes.
    if (!network_observer_)
      network_observer_ = std::make_unique<NetworkObserver>(this);
    else if (network_observer_->MaxAttemptsExceeded())
      network_observer_.reset();

    if (!pending_provisioned_origin_id_cbs_.empty()) {
      // This failure results from a user request (as opposed to
      // pre-provisioning having been started).
      SetExpirableTokenIfNeeded(pref_service_);

      // As this failed, satisfy all pending requests by returning false.
      base::queue<ProvisionedOriginIdCB> pending_requests;
      pending_requests.swap(pending_provisioned_origin_id_cbs_);
      while (!pending_requests.empty()) {
        std::move(pending_requests.front())
            .Run(GetOriginIdStatus::kFailure, base::nullopt);
        pending_requests.pop();
      }
    }

    is_provisioning_ = false;
    return;
  }

  // Success, for at least one level. Pass |origin_id| to the first requestor if
  // somebody is waiting for it. Otherwise add it to the list of available
  // origin IDs in the preference.
  DCHECK(origin_id);
  if (!pending_provisioned_origin_id_cbs_.empty()) {
    std::move(pending_provisioned_origin_id_cbs_.front())
        .Run(GetOriginIdStatus::kSuccessWithNewlyProvisionedOriginId,
             origin_id);
    pending_provisioned_origin_id_cbs_.pop();
  } else {
    DictionaryPrefUpdate update(pref_service_, kMediaDrmOriginIds);
    AddOriginId(update.Get(), origin_id.value());

    // If we already have enough pre-provisioned origin IDs, we're done.
    // Stop watching for network change events.
    if (CountAvailableOriginIds(update.Get()) >= kMaxPreProvisionedOriginIds) {
      network_observer_.reset();
      RemoveExpirableToken(update.Get());
      is_provisioning_ = false;
      return;
    }
  }

  // Create another pre-provisioned origin ID asynchronously.
  StartProvisioningAsync();
}

void MediaDrmOriginIdManager::RecordCountOfPreprovisionedOriginIds() {
  int available_origin_ids = 0;
  const auto* pref = pref_service_->GetDictionary(kMediaDrmOriginIds);
  if (pref)
    available_origin_ids = CountAvailableOriginIds(pref);

  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.EME.MediaDrm.PreprovisionedOriginId.PerAppProvisioningDevice",
        available_origin_ids, kUMAMaxPreProvisionedOriginIds);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.EME.MediaDrm.PreprovisionedOriginId.NonPerAppProvisioningDevice",
        available_origin_ids, kUMAMaxPreProvisionedOriginIds);
  }
}
