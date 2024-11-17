// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"

#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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
//     "last_provisioning_attempt_time": $last_provisioning_attempt_time,
// }
//
// If specified, "expirable_token" is stored as a string representing the
// int64_t (base::NumberToString()) form of the number of microseconds since
// Windows epoch (1601-01-01 00:00:00 UTC). It is the latest time that this
// code should attempt to pre-provision more origins on some devices.
//
// "last_provisioning_attempt_time" is only used on Android R due to bugs in
// the OS. The OS can get into a weird state where provisioning attempts crash,
// although rebooting the device is expected to clear this condition. However,
// as the code attempts to pre-provision some origin IDs if needed shortly
// after launch, this can result in Chrome randomly crashing every time it is
// started. "last_provisioning_attempt_time" is a time like "expirable_token"
// and represents the last time a provisioning attempt was made (roughly, as
// the attempt is posted as a delayed task). If set, another attempt at
// provisioning won't be made until |kProvisioningDelta| has passed. If
// provisioning returns, then provisioning doesn't crash, so this value is
// cleared and provisioning can be checked every time Chrome starts.
// Note that this does not affect requests for an origin. If a page needs one,
// then provisioning will be attempted. This may still crash.
// TODO(b/253295050): Remove this workaround if Android R patched to fix this.

namespace {

const char kMediaDrmOriginIds[] = "media.media_drm_origin_ids";
const char kExpirableToken[] = "expirable_token";
const char kOriginIds[] = "origin_ids";
const char kLastProvisioningAttemptTimeToken[] =
    "last_provisioning_attempt_time";

// The maximum number of origin IDs to pre-provision. Chosen to be small to
// minimize provisioning server load.
// TODO(jrummell): Adjust this value if needed after initial launch.
constexpr int kMaxPreProvisionedOriginIds = 2;

// The maximum number of origin IDs logged to UMA.
constexpr int kUMAMaxPreProvisionedOriginIds = 10;

// "expirable_token" is only good for 24 hours.
constexpr base::TimeDelta kExpirationDelta = base::Hours(24);

// Only try provisioning once a week (Android R only due to crashes).
constexpr base::TimeDelta kProvisioningDelta = base::Days(7);

// Time to wait before attempting pre-provisioning at startup (if enabled).
constexpr base::TimeDelta kStartupDelay = base::Minutes(1);

// Time to wait before logging number of pre-provisioned origin IDs at startup
constexpr base::TimeDelta kCheckDelay = base::Minutes(5);
static_assert(kCheckDelay > kStartupDelay,
              "Must allow time for pre-provisioning to run first");

// These are reported to UMA server. Do not renumber or reuse values.
enum class ProvisioningResult {
  kSuccess = 0,
  kFailedWhileOnline = 1,
  kFailedWhileOffline = 2,
  kMaxValue = kFailedWhileOffline,
};

void ReportProvisioningResultUMA(ProvisioningResult result) {
  base::UmaHistogramEnumeration("Media.EME.MediaDrm.Provisioning", result);
}
// When unable to get an origin ID, only attempt to pre-provision more if
// pre-provision is called within |kExpirationDelta| of the time of this
// failure. This is not needed on devices that support per-application
// provisioning.
void SetExpirableToken(PrefService* const pref_service) {
  DVLOG(3) << __func__;

  ScopedDictPrefUpdate update(pref_service, kMediaDrmOriginIds);
  update->Set(kExpirableToken,
              base::TimeToValue(base::Time::Now() + kExpirationDelta));
}

void RemoveExpirableToken(base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;
  origin_id_dict.Remove(kExpirableToken);
}

// On Android R a bug in the OS can cause MediaDrm::getProvisionRequest()
// to crash. As this runs shortly after startup, Chrome will be unusable
// if that happens. So use |kLastProvisioningAttemptTimeToken| to keep track of
// the last attempt to pre-provision, and don't try again within
// |kProvisioningDelta|. Value is cleared if provisioning returns, so on devices
// without the crash things will work as normal.
// TODO(b/253295050): Remove this workaround if Android R patched to fix this.

bool IsAndroidR() {
  return base::android::BuildInfo::GetInstance()->sdk_int() ==
         base::android::SDK_VERSION_R;
}

bool ShouldAttemptProvisioning(base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(IsAndroidR());

  const base::Value* token_value =
      origin_id_dict.Find(kLastProvisioningAttemptTimeToken);
  if (token_value) {
    auto last_provisioning_attempt_time = base::ValueToTime(*token_value);
    if (last_provisioning_attempt_time) {
      if (base::Time::Now() <
          last_provisioning_attempt_time.value() + kProvisioningDelta) {
        // Last provisioning attempt is within |kProvisioningDelta|, so return
        // false so that provisioning is not attempted.
        return false;
      }
    }
  }

  // Either no value or it's too old, so return true to try a provisioning
  // attempt.
  return true;
}

void SetLastProvisioningTime(base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(IsAndroidR());

  origin_id_dict.Set(kLastProvisioningAttemptTimeToken,
                     base::TimeToValue(base::Time::Now()));
}

void RemoveLastProvisioningTime(base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(IsAndroidR());

  origin_id_dict.Remove(kLastProvisioningAttemptTimeToken);
}

// On devices that don't support per-application provisioning attempts to
// pre-provision more origin IDs should only happen if an origin ID was
// requested recently and failed. This code checks that the time saved in
// |kExpirableToken| is less than the current time. If |kExpirableToken| doesn't
// exist then this function returns false. On devices that support per
// application provisioning pre-provisioning is always allowed. If
// |kExpirableToken| is expired or corrupt, it will be removed for privacy
// reasons.
bool CanPreProvision(bool is_per_application_provisioning_supported,
                     base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;

  // On devices that support per-application provisioning, this is always true.
  if (is_per_application_provisioning_supported)
    return true;

  // Device doesn't support per-application provisioning, so check if
  // "expirable_token" is still valid.
  const base::Value* token_value = origin_id_dict.Find(kExpirableToken);
  if (!token_value)
    return false;

  std::optional<base::Time> expiration_time = base::ValueToTime(*token_value);
  if (!expiration_time) {
    RemoveExpirableToken(origin_id_dict);
    return false;
  }

  if (base::Time::Now() > *expiration_time) {
    DVLOG(3) << __func__ << ": Token exists but has expired";
    RemoveExpirableToken(origin_id_dict);
    return false;
  }

  return true;
}

int CountAvailableOriginIds(const base::Value::Dict& origin_id_dict) {
  DVLOG(3) << __func__;

  const base::Value::List* origin_ids = origin_id_dict.FindList(kOriginIds);
  if (!origin_ids)
    return 0;

  DVLOG(3) << "count: " << origin_ids->size();
  return origin_ids->size();
}

base::UnguessableToken TakeFirstOriginId(PrefService* const pref_service) {
  DVLOG(3) << __func__;

  ScopedDictPrefUpdate update(pref_service, kMediaDrmOriginIds);

  base::Value::List* origin_ids = update->FindList(kOriginIds);
  if (!origin_ids)
    return base::UnguessableToken::Null();

  if (origin_ids->empty())
    return base::UnguessableToken::Null();

  auto first_entry = origin_ids->begin();
  auto result = base::ValueToUnguessableToken(*first_entry);
  origin_ids->erase(first_entry);

  return result.value_or(base::UnguessableToken::Null());
}

void AddOriginId(base::Value::Dict& origin_id_dict,
                 const base::UnguessableToken& origin_id) {
  DVLOG(3) << __func__;
  base::Value::List* origin_ids = origin_id_dict.EnsureList(kOriginIds);
  origin_ids->Append(base::UnguessableTokenToValue(origin_id));
}

// Helper class that creates a new origin ID and provisions it for both L1
// (if available) and L3. This class self destructs when provisioning is done
// (successfully or not).
class MediaDrmProvisionHelper {
 public:
  using ProvisionedOriginIdCB = base::OnceCallback<void(
      const MediaDrmOriginIdManager::MediaDrmOriginId& origin_id)>;

  explicit MediaDrmProvisionHelper(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_shared_url_loader_factory) {
    DVLOG(1) << __func__;
    DCHECK(pending_shared_url_loader_factory);
    create_fetcher_cb_ =
        base::BindRepeating(&content::CreateProvisionFetcher,
                            network::SharedURLLoaderFactory::Create(
                                std::move(pending_shared_url_loader_factory)));
  }

  void Provision(ProvisionedOriginIdCB callback) {
    DVLOG(1) << __func__;

    complete_callback_ = std::move(callback);
    origin_id_ = base::UnguessableToken::Create();

    // Try provisioning for L3 first.
    auto result = media::MediaDrmBridge::CreateWithoutSessionSupport(
        kWidevineKeySystem, origin_id_.ToString(),
        media::MediaDrmBridge::SECURITY_LEVEL_3, "L3 provisioning",
        create_fetcher_cb_);
    if (!result.has_value()) {
      // Unable to create mediaDrm for L3, so try L1.
      DVLOG(1) << "Unable to create MediaDrmBridge for L3, CreateCdmStatus: "
               << (media::StatusCodeType)result.code();
      ProvisionLevel1(false);
      return;
    }

    media_drm_bridge_ = std::move(result).value();

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
    media_drm_bridge_.reset();
    auto result = media::MediaDrmBridge::CreateWithoutSessionSupport(
        kWidevineKeySystem, origin_id_.ToString(),
        media::MediaDrmBridge::SECURITY_LEVEL_1, "L1 provisioning",
        create_fetcher_cb_);
    if (!result.has_value()) {
      // Unable to create MediaDrm for L1, so quit. Note that L3 provisioning
      // may or may not have worked.
      DVLOG(1) << "Unable to create MediaDrmBridge for L1, CreateCdmStatus: "
               << (media::StatusCodeType)result.code();
      ProvisionDone(L3_success, false);
      return;
    }

    media_drm_bridge_ = std::move(result).value();

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
        .Run(success ? std::make_optional(origin_id_) : std::nullopt);
    delete this;
  }

  media::CreateFetcherCB create_fetcher_cb_;
  ProvisionedOriginIdCB complete_callback_;
  base::UnguessableToken origin_id_;
  scoped_refptr<media::MediaDrmBridge> media_drm_bridge_;
};

// Provisioning runs on a separate background sequence. This kicks off the
// process, calling |callback| when done. |provisioning_result_cb_for_testing|
// is provided for testing.
void StartProvisioning(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory,
    MediaDrmOriginIdManager::ProvisioningResultCB
        provisioning_result_cb_for_testing,
    MediaDrmProvisionHelper::ProvisionedOriginIdCB callback) {
  DVLOG(1) << __func__;

  if (provisioning_result_cb_for_testing) {
    // MediaDrm can't provision an origin ID during unittests, so use
    // |provisioning_result_cb_for_testing| to generate one (or not, depending
    // on the test case).
    std::move(callback).Run(provisioning_result_cb_for_testing.Run());
    return;
  }

  if (!pending_shared_url_loader_factory) {
    // No fetcher available, so don't bother trying to provision.
    std::move(callback).Run(std::nullopt);
    return;
  }

  // MediaDrmProvisionHelper will delete itself when it's done.
  auto* helper =
      new MediaDrmProvisionHelper(std::move(pending_shared_url_loader_factory));
  helper->Provision(std::move(callback));
}

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
  const raw_ptr<MediaDrmOriginIdManager> parent_;
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
    // Special handling for Android R due to a bug in the OS can cause
    // MediaDrm::getProvisionRequest() to crash. We check here so that if the
    // preference is updated it should be persisted before provisioning is
    // actually attempted after |kStartupDelay|.
    bool should_attempt_provisioning = true;
    if (IsAndroidR()) {
      ScopedDictPrefUpdate update(pref_service_, kMediaDrmOriginIds);
      should_attempt_provisioning = ShouldAttemptProvisioning(*update);
      if (should_attempt_provisioning) {
        // Provisioning will be attempted, so record the current time.
        SetLastProvisioningTime(*update);
      }
    }

    // Running this after a delay of |kStartupDelay| in order to not do too much
    // extra work when the profile is loaded.
    if (should_attempt_provisioning) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MediaDrmOriginIdManager::PreProvisionIfNecessary,
                         weak_factory_.GetWeakPtr()),
          kStartupDelay);
    }
  }

  // In order to determine how devices are pre-provisioning origin IDs, post a
  // task to check how many pre-provisioned origin IDs are available after a
  // delay of |kCheckDelay|.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
        .Run(GetOriginIdStatus::kFailure, std::nullopt);
    pending_provisioned_origin_id_cbs_.pop();
  }
}

void MediaDrmOriginIdManager::PreProvisionIfNecessary() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If pre-provisioning already running, no need to start it again.
  if (is_provisioning_)
    return;

  // Checking if per-application provisioning is supported is known to be
  // expensive (see crbug.com/1366106). Calling it on a low priority thread
  // to avoid slowing down the main thread, and then resuming pre-provisioning
  // back on this thread (as access to the PrefService must be done on the
  // UI thread).
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &media::MediaDrmBridge::IsPerApplicationProvisioningSupported),
      base::BindOnce(&MediaDrmOriginIdManager::ResumePreProvisionIfNecessary,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmOriginIdManager::ResumePreProvisionIfNecessary(
    bool is_per_application_provisioning_supported) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_per_application_provisioning_supported_ =
      is_per_application_provisioning_supported;

  // On devices that need to, check that the user has recently requested
  // an origin ID. If not, then skip pre-provisioning on those devices.
  ScopedDictPrefUpdate update(pref_service_, kMediaDrmOriginIds);
  if (!CanPreProvision(is_per_application_provisioning_supported, *update)) {
    // Disable any network monitoring, if it exists.
    network_observer_.reset();
    return;
  }

  // No need to pre-provision if there are already enough existing
  // pre-provisioned origin IDs.
  if (CountAvailableOriginIds(*update) >= kMaxPreProvisionedOriginIds) {
    // Disable any network monitoring, if it exists.
    network_observer_.reset();
    return;
  }

  // Attempt to pre-provision more origin IDs in the near future. This can
  // be done on a low priority sequence in the background.
  is_provisioning_ = true;
  StartProvisioningAsync(/*run_in_background=*/true);
}

void MediaDrmOriginIdManager::GetOriginId(ProvisionedOriginIdCB callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // See if there is one already pre-provisioned that can be used.
  base::UnguessableToken origin_id = TakeFirstOriginId(pref_service_);

  // Start a task to pre-provision more origin IDs if we are currently
  // not doing so.
  if (!is_provisioning_) {
    is_provisioning_ = true;

    // If there is an origin ID available then we need to replace it, but this
    // can be done in the background. If there are none available, we need one
    // now as the user is trying to play protected content.
    StartProvisioningAsync(/*run_in_background=*/!origin_id.is_empty());
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

void MediaDrmOriginIdManager::StartProvisioningAsync(bool run_in_background) {
  DVLOG(1) << __func__ << " run_in_background: " << run_in_background;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_provisioning_);

  // Run StartProvisioning() later. This is done on a separate thread to avoid
  // scroll jank, especially when pre-provisioning is happening (as the origin
  // IDs aren't needed for the current page, so it can run at low priority).
  // However, if a user needs a provisioned origin ID immediately, then run at
  // higher priority. See crbug.com/1366106 for details.
  const base::TaskPriority priority = run_in_background
                                          ? base::TaskPriority::BEST_EFFORT
                                          : base::TaskPriority::USER_VISIBLE;

  // Provisioning requires accessing the network to handle the actual
  // provisioning request. If access is not currently available, then the
  // request will fail and OriginIdProvisioned() will setup a observer
  // for when network access is available again. When testing the network
  // is not accessible, but prefer to call |provisioning_result_cb_for_testing_|
  // from the generated sequence.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_shared_url_loader_factory;
  auto* network_context_manager =
      g_browser_process->system_network_context_manager();
  if (network_context_manager) {
    // Fetching the license will run on a different sequence, so clone
    // SharedURLLoaderFactory to create an unbound one that can be used
    // on any thread/sequence.
    pending_shared_url_loader_factory =
        network_context_manager->GetSharedURLLoaderFactory()->Clone();
  }

  // Note that MediaDrmBridge requires the use of SingleThreadTaskRunner.
  scoped_refptr<base::SingleThreadTaskRunner> provisioning_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), priority});
  provisioning_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&StartProvisioning,
                     std::move(pending_shared_url_loader_factory),
                     provisioning_result_cb_for_testing_,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &MediaDrmOriginIdManager::OriginIdProvisioned,
                         weak_factory_.GetWeakPtr()))));
}

void MediaDrmOriginIdManager::OriginIdProvisioned(
    const MediaDrmOriginId& origin_id) {
  DVLOG(1) << __func__
           << " origin_id: " << (origin_id ? origin_id->ToString() : "null");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_provisioning_);

  // On Android R, clear |kLastProvisioningAttemptTimeToken| as provisioning()
  // didn't crash.
  if (IsAndroidR()) {
    ScopedDictPrefUpdate update(pref_service_, kMediaDrmOriginIds);
    RemoveLastProvisioningTime(*update);
  }

  if (!origin_id) {
    // Unable to provision an origin ID, most likely due to being unable to
    // connect to a provisioning server or a failure in the MediaDrm code. Set
    // up a NetworkObserver to detect when we're connected to a network so that
    // we can try again. If there is already a NetworkObserver and provisioning
    // has failed multiple times, stop watching for network changes.
    if (!network_observer_)
      network_observer_ = std::make_unique<NetworkObserver>(this);
    else if (network_observer_->MaxAttemptsExceeded())
      network_observer_.reset();

    // Log the failure for tracking purposes.
    ReportProvisioningResultUMA(
        content::GetNetworkConnectionTracker()->IsOffline()
            ? ProvisioningResult::kFailedWhileOffline
            : ProvisioningResult::kFailedWhileOnline);

    if (!pending_provisioned_origin_id_cbs_.empty()) {
      // This failure results from a user request (as opposed to
      // pre-provisioning having been started).

      if (!IsPerApplicationProvisioningSupported()) {
        // Token is only required if per application provisioning is not
        // supported.
        SetExpirableToken(pref_service_);
      }

      // As this failed, satisfy all pending requests by returning false.
      base::queue<ProvisionedOriginIdCB> pending_requests;
      pending_requests.swap(pending_provisioned_origin_id_cbs_);
      while (!pending_requests.empty()) {
        std::move(pending_requests.front())
            .Run(GetOriginIdStatus::kFailure, std::nullopt);
        pending_requests.pop();
      }
    }

    is_provisioning_ = false;
    return;
  }

  // Success, for at least one level. Log the success.
  ReportProvisioningResultUMA(ProvisioningResult::kSuccess);

  // Pass |origin_id| to the first requestor if somebody is waiting for it.
  // Otherwise add it to the list of available origin IDs in the preference.
  if (!pending_provisioned_origin_id_cbs_.empty()) {
    std::move(pending_provisioned_origin_id_cbs_.front())
        .Run(GetOriginIdStatus::kSuccessWithNewlyProvisionedOriginId,
             origin_id);
    pending_provisioned_origin_id_cbs_.pop();
  } else {
    ScopedDictPrefUpdate update(pref_service_, kMediaDrmOriginIds);
    AddOriginId(*update, origin_id.value());

    // If we already have enough pre-provisioned origin IDs, we're done.
    // Stop watching for network change events.
    if (CountAvailableOriginIds(*update) >= kMaxPreProvisionedOriginIds) {
      network_observer_.reset();
      RemoveExpirableToken(*update);
      is_provisioning_ = false;
      return;
    }
  }

  // Create another pre-provisioned origin ID asynchronously. If there is
  // no pending requestor, then this is simply pre-provisioning another one,
  // and can be safely run in the background. If there is a request, run
  // at higher priority to quickly satisfy the request.
  StartProvisioningAsync(
      /*run_in_background=*/pending_provisioned_origin_id_cbs_.empty());
}

bool MediaDrmOriginIdManager::IsPerApplicationProvisioningSupported() {
  // `is_per_application_provisioning_supported_` should be set in
  // PreProvisionIfNecessary(). However, in case it's not (e.g. flag
  // kMediaDrmPreprovisioningAtStartup is disabled), determine it now.
  if (!is_per_application_provisioning_supported_.has_value()) {
    is_per_application_provisioning_supported_ =
        media::MediaDrmBridge::IsPerApplicationProvisioningSupported();
  }
  return is_per_application_provisioning_supported_.value();
}

void MediaDrmOriginIdManager::RecordCountOfPreprovisionedOriginIds() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const auto& pref = pref_service_->GetDict(kMediaDrmOriginIds);
  int available_origin_ids = CountAvailableOriginIds(pref);

  if (IsPerApplicationProvisioningSupported()) {
    base::UmaHistogramExactLinear(
        "Media.EME.MediaDrm.PreprovisionedOriginId.PerAppProvisioningDevice",
        available_origin_ids, kUMAMaxPreProvisionedOriginIds);
  } else {
    base::UmaHistogramExactLinear(
        "Media.EME.MediaDrm.PreprovisionedOriginId.NonPerAppProvisioningDevice",
        available_origin_ids, kUMAMaxPreProvisionedOriginIds);
  }
}
