// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/android_push_notification_manager.h"

#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace android {

namespace {

// All of the code within OptimizationGuideStore uses OnceClosure's and trying
// to insert the usage of a <void(bool)> callback to indicate success/failure is
// not worth the effort. This class is a workaround.
//
// The pattern of the PushNotificationManager is to run a OnceClosure when an
// action succeeds, and to have the closure not be run on failure. When the
// closure is not run, any objects that are owned by the callback (like a
// unique_ptr that is passed to the callback-invoked method) are destroyed.
//
// This helper takes advantage of that destruction to detect when an action has
// failed because the callback was never run and then dropped out of scope. When
// this class is destroyed, if the given |on_failure| closure has not be reset,
// then it is run. Calling |Disarm| will reset the failure callback, so when the
// async action succeeds, then |Disarm| should be called before |this| can be
// destroyed.
//
//
// For example:
//
// void DisarmHelperOnSuccess(
//        unique_ptr<DroppedSuccessCallbackHelper> helper,
//        base::OnceClosure internal_on_success) {
//    helper->Disarm();
//    std::move(internal_on_success).Run();
// }
//
// base::OnceClosure internal_on_failure =
//    base::BindOnce(&MyClass::HandleFailure);
// base::OnceClosure internal_on_success =
//    base::BindOnce(&MyClass::HandleSuccess);
//
// unique_ptr<DroppedSuccessCallbackHelper> helper =
//    CreateAndArm(std::move(internal_on_failure));
//
// base::OnceClosure call_me_on_success_else_destroy =
//    base::BindOnce(&DisarmHelperOnSuccess,
//                    std::move(helper),
//                    std::move(internal_on_success));
class DroppedSuccessCallbackHelper {
 public:
  static std::unique_ptr<DroppedSuccessCallbackHelper> CreateAndArm(
      base::OnceClosure on_failure) {
    return std::make_unique<DroppedSuccessCallbackHelper>(
        std::move(on_failure));
  }

  explicit DroppedSuccessCallbackHelper(base::OnceClosure on_failure)
      : on_failure_(std::move(on_failure)) {}
  ~DroppedSuccessCallbackHelper() {
    bool did_succeed = !on_failure_;

    if (report_result_to_boolean_histogram_) {
      base::UmaHistogramBoolean(*report_result_to_boolean_histogram_,
                                did_succeed);
    }

    if (!did_succeed) {
      std::move(on_failure_).Run();
    }
  }

  void SetReportResultHistogram(const std::string& histogram) {
    report_result_to_boolean_histogram_ = histogram;
  }

  void Disarm() { on_failure_.Reset(); }

 private:
  base::OnceClosure on_failure_;
  absl::optional<std::string> report_result_to_boolean_histogram_;
};

// Called on success of an action to disarm the helper.
void SimpleDisarmHelper(std::unique_ptr<DroppedSuccessCallbackHelper> helper) {
  helper->Disarm();
}

// Called when all the push notifications for the given optimization type have
// been processed. This clears the Android cache and disarms the helper.
void OnOptimizationTypeHandled(
    std::unique_ptr<DroppedSuccessCallbackHelper> helper,
    proto::OptimizationType opt_type) {
  helper->Disarm();
  OptimizationGuideBridge::ClearCacheForOptimizationType(opt_type);
}

class ScopedBooleanHistogramRecorder {
 public:
  explicit ScopedBooleanHistogramRecorder(const std::string& histogram_name)
      : histogram_name_(histogram_name) {}
  ~ScopedBooleanHistogramRecorder() {
    base::UmaHistogramBoolean(histogram_name_, sample_);
  }

  void SetSample(bool sample) { sample_ = sample; }

 private:
  const std::string histogram_name_;
  bool sample_ = false;
};

}  // namespace

AndroidPushNotificationManager::~AndroidPushNotificationManager() = default;
AndroidPushNotificationManager::AndroidPushNotificationManager(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

void AndroidPushNotificationManager::SetDelegate(
    PushNotificationManager::Delegate* delegate) {
  delegate_ = delegate;
}

void AndroidPushNotificationManager::OnDelegateReady() {
  DCHECK(delegate_);
  DCHECK(features::IsPushNotificationsEnabled());

  // Quickly check that nothing overflowed. That way we don't risk some
  // notifications being processed just before a purge sweeps everything out.
  base::flat_set<proto::OptimizationType> overflowed_types =
      OptimizationGuideBridge::GetOptTypesThatOverflowedPushNotifications();
  bool did_overflow = !overflowed_types.empty();
  base::UmaHistogramBoolean("OptimizationGuide.PushNotifications.DidOverflow",
                            did_overflow);
  if (did_overflow) {
    OnNeedToPurgeStore();
    return;
  }

  size_t cached_notifications_total = 0;
  for (proto::OptimizationType opt_type :
       OptimizationGuideBridge::GetOptTypesWithPushNotifications()) {
    std::vector<proto::HintNotificationPayload> notifications =
        OptimizationGuideBridge::GetCachedNotifications(opt_type);
    cached_notifications_total += notifications.size();

    // The delegate expects to only get one type of a key representation at a
    // time, so separate those out.
    std::map<proto::KeyRepresentation, base::flat_set<std::string>>
        hints_keys_by_key_rep;
    for (const proto::HintNotificationPayload& notification : notifications) {
      if (!notification.has_hint_key())
        continue;
      if (!notification.has_key_representation())
        continue;

      if (hints_keys_by_key_rep.find(notification.key_representation()) ==
          hints_keys_by_key_rep.end()) {
        hints_keys_by_key_rep.emplace(notification.key_representation(),
                                      base::flat_set<std::string>{});
      }
      hints_keys_by_key_rep.find(notification.key_representation())
          ->second.emplace(notification.hint_key());
    }

    if (hints_keys_by_key_rep.empty()) {
      continue;
    }

    // The helper here is used only for tracking success and logging that to
    // metrics. In this case, nothing needs to be done in the event of failure.
    auto helper = DroppedSuccessCallbackHelper::CreateAndArm(base::DoNothing());
    helper->SetReportResultHistogram(
        "OptimizationGuide.PushNotifications."
        "CachedNotificationsHandledSuccessfully");

    // The barrier closure will once run the given once closure after it is run
    // |hints_keys_by_key_rep.size()| times.
    base::RepeatingClosure barrier =
        base::BarrierClosure(hints_keys_by_key_rep.size(),
                             base::BindOnce(&OnOptimizationTypeHandled,
                                            std::move(helper), opt_type));
    for (const auto& pair : hints_keys_by_key_rep) {
      delegate_->RemoveFetchedEntriesByHintKeys(barrier, pair.first,
                                                pair.second);
    }
  }

  base::UmaHistogramCounts100(
      "OptimizationGuide.PushNotifications.CachedNotificationCount",
      cached_notifications_total);
}

void AndroidPushNotificationManager::OnNeedToPurgeStore() {
  DCHECK(delegate_);

  delegate_->PurgeFetchedEntries(
      base::BindOnce(&AndroidPushNotificationManager::OnPurgeCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AndroidPushNotificationManager::OnNewPushNotification(
    const proto::HintNotificationPayload& notification) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PushNotifications.GotPushNotification", true);

  if (!delegate_) {
    // Cache the notification into Android shared preference.
    OnNewPushNotificationNotHandled(notification);
    return;
  }

  if (!notification.has_hint_key())
    return;

  if (!notification.has_key_representation())
    return;

  DispatchPayload(notification);
  InvalidateHints(notification);
}

void AndroidPushNotificationManager::InvalidateHints(
    const proto::HintNotificationPayload& notification) {
  // If the notification can't be handled right now, make sure it gets pushed
  // back to Android to be cached.
  auto helper = DroppedSuccessCallbackHelper::CreateAndArm(base::BindOnce(
      &AndroidPushNotificationManager::OnNewPushNotificationNotHandled,
      weak_ptr_factory_.GetWeakPtr(), notification));

  helper->SetReportResultHistogram(
      "OptimizationGuide.PushNotifications."
      "PushNotificationHandledSuccessfully");

  delegate_->RemoveFetchedEntriesByHintKeys(
      base::BindOnce(&SimpleDisarmHelper, std::move(helper)),
      notification.key_representation(), {notification.hint_key()});
}

void AndroidPushNotificationManager::OnPurgeCompleted() {
  for (int int_opt_type = proto::OptimizationType_MIN;
       int_opt_type <= proto::OptimizationType_MAX; int_opt_type++) {
    OptimizationGuideBridge::ClearCacheForOptimizationType(
        static_cast<proto::OptimizationType>(int_opt_type));
  }
}

void AndroidPushNotificationManager::DispatchPayload(
    const proto::HintNotificationPayload& notification) {
  // No custom payload or optimization type.
  if (!notification.has_payload() || !notification.has_optimization_type()) {
    return;
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      notification.optimization_type(),
      static_cast<optimization_guide::proto::OptimizationType>(
          optimization_guide::proto::OptimizationType_ARRAYSIZE));

  for (Observer& observer : observers_) {
    observer.OnNotificationPayload(notification.optimization_type(),
                                   notification.payload());
  }
}

void AndroidPushNotificationManager::OnNewPushNotificationNotHandled(
    const proto::HintNotificationPayload& notification) {
  OptimizationGuideBridge::OnNotificationNotHandledByNative(notification);
}

void AndroidPushNotificationManager::AddObserver(
    PushNotificationManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void AndroidPushNotificationManager::RemoveObserver(
    PushNotificationManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace android
}  // namespace optimization_guide
