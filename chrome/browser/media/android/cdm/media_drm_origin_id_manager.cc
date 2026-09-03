// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/locale_utils.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
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
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

// The storage will be managed by PrefService. All data will be stored in a
// dictionary under the key "media.media_drm_origin_ids". The dictionary is
// structured as follows:
//
// {
//     "origin_ids": [ $origin_id, ... ]
//     "last_provisioning_attempt_time": $last_provisioning_attempt_time,
// }
//
// "last_provisioning_attempt_time" is only used on Android R due to bugs in
// the OS. The OS can get into a weird state where provisioning attempts crash,
// although rebooting the device is expected to clear this condition. However,
// as the code attempts to pre-provision some origin IDs if needed shortly
// after launch, this can result in Chrome randomly crashing every time it is
// started. "last_provisioning_attempt_time" represents the last time a
// provisioning attempt was made (roughly, as the attempt is posted as a
// delayed task). If set, another attempt at provisioning won't be made until
// |kProvisioningDelta| has passed. If provisioning returns, then provisioning
// doesn't crash, so this value is cleared and provisioning can be checked
// every time Chrome starts.
// Note that this does not affect requests for an origin. If a page needs one,
// then provisioning will be attempted. This may still crash.
// TODO(b/253295050): Remove this workaround if Android R patched to fix this.

namespace {

const char kMediaDrmOriginIds[] = "media.media_drm_origin_ids";
const char kOriginIds[] = "origin_ids";
const char kLastProvisioningAttemptTimeToken[] =
    "last_provisioning_attempt_time";

std::string_view GetDetailedUserAgent() {
  // Use NoDestructor to avoid computing this string multiple times for every
  // provisioning request.
  static const base::NoDestructor<std::string> user_agent([] {
    std::string locale = base::android::GetDefaultLocaleString();
    // Example Format: Widevine CDM v1.0 (Linux; U; Android 35;
    // en-US; Build/BP1A.250505.005; user)
    return base::StringPrintf(
        "Widevine CDM v1.0 (Linux; U; Android %d; %s; Build/%s; %s)",
        base::android::android_info::sdk_int(), locale.c_str(),
        base::android::android_info::android_build_id(),
        base::android::android_info::build_type());
  }());
  return *user_agent;
}

// The maximum number of origin IDs to pre-provision. Chosen to be small to
// minimize provisioning server load.
// TODO(jrummell): Adjust this value if needed after initial launch.
constexpr int kMaxPreProvisionedOriginIds = 2;

// The maximum number of origin IDs logged to UMA.
constexpr int kUMAMaxPreProvisionedOriginIds = 10;

// Only try provisioning once a week (Android R only due to crashes).
constexpr base::TimeDelta kProvisioningDelta = base::Days(7);

// Time to wait before attempting pre-provisioning at startup (if enabled).
constexpr base::TimeDelta kStartupDelay = base::Minutes(1);

// Time to wait before logging number of pre-provisioned origin IDs at startup
constexpr base::TimeDelta kCheckDelay = base::Minutes(5);
static_assert(kCheckDelay > kStartupDelay,
              "Must allow time for pre-provisioning to run first");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProvisioningResult)
enum class ProvisioningResult {
  kSuccess = 0,
  kFailedWhileOnline = 1,
  kFailedWhileOffline = 2,
  kMaxValue = kFailedWhileOffline,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaDrmProvisioningResult)

void ReportProvisioningResultUMA(ProvisioningResult result) {
  base::UmaHistogramEnumeration("Media.EME.MediaDrm.Provisioning", result);
}
// On Android R a bug in the OS can cause MediaDrm::getProvisionRequest()
// to crash. As this runs shortly after startup, Chrome will be unusable
// if that happens. So use |kLastProvisioningAttemptTimeToken| to keep track of
// the last attempt to pre-provision, and don't try again within
// |kProvisioningDelta|. Value is cleared if provisioning returns, so on devices
// without the crash things will work as normal.
// TODO(b/253295050): Remove this workaround if Android R patched to fix this.

bool IsAndroidR() {
  return base::android::android_info::sdk_int() ==
         base::android::android_info::SDK_VERSION_R;
}

bool ShouldAttemptProvisioning(base::DictValue& origin_id_dict) {
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

void SetLastProvisioningTime(base::DictValue& origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(IsAndroidR());

  origin_id_dict.Set(kLastProvisioningAttemptTimeToken,
                     base::TimeToValue(base::Time::Now()));
}

void RemoveLastProvisioningTime(base::DictValue& origin_id_dict) {
  DVLOG(3) << __func__;
  DCHECK(IsAndroidR());

  origin_id_dict.Remove(kLastProvisioningAttemptTimeToken);
}

int CountAvailableOriginIds(const base::DictValue& origin_id_dict) {
  DVLOG(3) << __func__;

  const base::ListValue* origin_ids = origin_id_dict.FindList(kOriginIds);
  if (!origin_ids)
    return 0;

  DVLOG(3) << "count: " << origin_ids->size();
  return origin_ids->size();
}

base::UnguessableToken TakeFirstOriginId(PrefService* const pref_service) {
  DVLOG(3) << __func__;

  ScopedDictPrefUpdate update(pref_service, kMediaDrmOriginIds);

  base::ListValue* origin_ids = update->FindList(kOriginIds);
  if (!origin_ids)
    return base::UnguessableToken::Null();

  if (origin_ids->empty())
    return base::UnguessableToken::Null();

  auto first_entry = origin_ids->begin();
  auto result = base::ValueToUnguessableToken(*first_entry);
  origin_ids->erase(first_entry);

  return result.value_or(base::UnguessableToken::Null());
}

void AddOriginId(base::DictValue& origin_id_dict,
                 const base::UnguessableToken& origin_id) {
  DVLOG(3) << __func__;
  base::ListValue* origin_ids = origin_id_dict.EnsureList(kOriginIds);
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
        base::BindRepeating(&content::CreateProvisionFetcherWithUserAgent,
                            network::SharedURLLoaderFactory::Create(
                                std::move(pending_shared_url_loader_factory)),
                            GetDetailedUserAgent());
  }

  void Provision(ProvisionedOriginIdCB callback) {
    DVLOG(1) << __func__;

    complete_callback_ = std::move(callback);
    origin_id_ = base::UnguessableToken::Create();

    // Try provisioning for L3 first.
    auto result = media::MediaDrmBridge::CreateWithoutSessionSupport(
        kWidevineKeySystem, origin_id_.ToString(),
        media::MediaDrmBridge::SECURITY_LEVEL_SW_SECURE_CRYPTO,
        "L3 provisioning", create_fetcher_cb_);
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
        media::MediaDrmBridge::SECURITY_LEVEL_HW_SECURE_ALL, "L1 provisioning",
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

const net::BackoffEntry::Policy kProvisioningBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    .num_errors_to_ignore = 0,

    // Initial delay in ms: 10 seconds.
    .initial_delay_ms = 1000 * 10,

    // Factor by which the waiting time is multiplied.
    .multiply_factor = 2.0,

    // Fuzzing percentage (jitter): 20%.
    .jitter_factor = 0.2,

    // Maximum amount of time we are willing to delay our request: 1 hour.
    .maximum_backoff_ms = 1000 * 60 * 60,

    // Time to keep an entry from being discarded: never.
    .entry_lifetime_ms = -1,

    // If true, we always use a delay of initial_delay_ms, even before
    // we've seen num_errors_to_ignore errors.
    .always_use_initial_delay = false,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProvisioningNetworkRetryResult)
enum class ProvisioningNetworkRetryResult {
  kRetryAttempted = 0,
  kIgnoredByBackoff = 1,
  kMaxValue = kIgnoredByBackoff,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:MediaDrmProvisioningNetworkRetryResult)

void ReportProvisioningNetworkRetryUMA(ProvisioningNetworkRetryResult result) {
  base::UmaHistogramEnumeration("Media.EME.MediaDrm.ProvisioningNetworkRetry",
                                result);
}

}  // namespace

// Watch for the device being connected to a network and call
// PreProvisionIfNecessary(). This object is owned by MediaDrmOriginIdManager
// and will be deleted when the manager goes away, so it is safe to keep a
// direct reference to the manager.
class MediaDrmOriginIdManager::NetworkObserver
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit NetworkObserver(MediaDrmOriginIdManager* parent)
      : parent_(parent),
        backoff_entry_(std::in_place, &kProvisioningBackoffPolicy) {
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
  void OnConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    if (type == net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
      return;
    }

    if (backoff_entry_->ShouldRejectRequest()) {
      ReportProvisioningNetworkRetryUMA(
          ProvisioningNetworkRetryResult::kIgnoredByBackoff);

      // If we are currently connected but in backoff, schedule a retry.
      if (!retry_timer_.IsRunning()) {
        retry_timer_.Start(
            FROM_HERE, backoff_entry_->GetTimeUntilRelease(),
            base::BindOnce(
                &MediaDrmOriginIdManager::NetworkObserver::OnRetryTimerExpired,
                base::Unretained(this)));
      }
      return;
    }
    ReportProvisioningNetworkRetryUMA(
        ProvisioningNetworkRetryResult::kRetryAttempted);

    ++number_of_attempts_;
    parent_->PreProvisionIfNecessary();
  }

  void InformOfRequest(bool succeeded) {
    backoff_entry_->InformOfRequest(succeeded);
  }

  void SetTickClockForTesting(const base::TickClock* clock) {
    backoff_entry_.emplace(&kProvisioningBackoffPolicy, clock);
  }

 private:
  void OnRetryTimerExpired() {
    // Timer expired, check if we're still connected before retrying.
    if (!content::GetNetworkConnectionTracker()->IsOffline()) {
      parent_->PreProvisionIfNecessary();
    }
  }

  // Use of raw pointer is okay as |parent_| owns this object.
  const raw_ptr<MediaDrmOriginIdManager> parent_;
  int number_of_attempts_ = 0;
  std::optional<net::BackoffEntry> backoff_entry_;
  base::OneShotTimer retry_timer_;
};

// static
void MediaDrmOriginIdManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMediaDrmOriginIds);
}

MediaDrmOriginIdManager::MediaDrmOriginIdManager(
    PrefService* pref_service,
    base::PassKey<MediaDrmOriginIdManagerFactory>)
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

void MediaDrmOriginIdManager::SetTickClockForTesting(  // IN-TEST
    const base::TickClock* clock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!network_observer_) {
    network_observer_ = std::make_unique<NetworkObserver>(this);
  }
  network_observer_->SetTickClockForTesting(clock);  // IN-TEST
}

void MediaDrmOriginIdManager::PreProvisionIfNecessary() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If pre-provisioning already running, no need to start it again.
  if (is_provisioning_)
    return;

  ResumePreProvisionIfNecessary();
}

void MediaDrmOriginIdManager::ResumePreProvisionIfNecessary() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // No need to pre-provision if there are already enough existing
  // pre-provisioned origin IDs.
  ScopedDictPrefUpdate update(pref_service_, kMediaDrmOriginIds);
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
  // higher priority. See crbug.com/40866724 for details.
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
    if (!network_observer_) {
      network_observer_ = std::make_unique<NetworkObserver>(this);
    } else if (network_observer_->MaxAttemptsExceeded()) {
      network_observer_.reset();
    }

    if (network_observer_) {
      network_observer_->InformOfRequest(/*succeeded=*/false);
    }

    // Log the failure for tracking purposes.
    ReportProvisioningResultUMA(
        content::GetNetworkConnectionTracker()->IsOffline()
            ? ProvisioningResult::kFailedWhileOffline
            : ProvisioningResult::kFailedWhileOnline);

    if (!pending_provisioned_origin_id_cbs_.empty()) {
      // This failure results from a user request (as opposed to
      // pre-provisioning having been started).

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

  if (network_observer_) {
    // Reset backoff on success so that subsequent retries start with fresh
    // delay timings.
    network_observer_->InformOfRequest(/*succeeded=*/true);
  }

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

void MediaDrmOriginIdManager::RecordCountOfPreprovisionedOriginIds() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const auto& pref = pref_service_->GetDict(kMediaDrmOriginIds);
  int available_origin_ids = CountAvailableOriginIds(pref);

  base::UmaHistogramExactLinear(
      "Media.EME.MediaDrm.PreprovisionedOriginId.PerAppProvisioningDevice",
      available_origin_ids, kUMAMaxPreProvisionedOriginIds);
}
