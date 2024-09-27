// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_METRICS_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_METRICS_H_

namespace notifications_uma {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisplayStatus {
  kSuccess = 0,
  kRoActivateFailed = 1,
  kConversionFailedInspectableToXmlIo = 2,
  kLoadXmlFailed = 3,
  kConversionFailedXmlIoToXml = 4,
  kCreateFactoryFailed = 5,
  kCreateToastNotificationFailed = 6,
  kCreateToastNotification2Failed = 7,
  kSettingGroupFailed = 8,
  kSettingTagFailed = 9,
  kGetGroupFailed = 10,
  kGetTagFailed = 11,
  kSuppressPopupFailed = 12,
  kAddToastDismissHandlerFailed = 13,
  kAddToastErrorHandlerFailed = 14,
  kShowingToastFailed = 15,
  kCreateToastNotificationManagerFailed = 16,
  kCreateToastNotifierWithIdFailed = 17,
  kDeprecatedDisabledForApplication = 18,
  kDeprecatedDisabledForUser = 19,
  kDeprecatedDisabledByGroupPolicy = 20,
  kDeprecatedDisabledByManifest = 21,
  kMaxValue = kDeprecatedDisabledByManifest,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CloseStatus {
  kSuccess = 0,
  kGetToastHistoryFailed = 1,
  kRemovingToastFailed = 2,
  kEmptyAumi = 3,
  kNotificationNotFound = 4,
  kMaxValue = kNotificationNotFound,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistoryStatus {
  kSuccess = 0,
  kCreateToastNotificationManagerFailed = 1,
  kQueryToastManagerStatistics2Failed = 2,
  kGetToastHistoryFailed = 3,
  kMaxValue = kGetToastHistoryFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayedStatus {
  kSuccess = 0,
  kSuccessWithGetAtFailure = 1,
  kGetToastHistoryFailed = 2,
  kQueryToastNotificationHistory2Failed = 3,
  kGetHistoryWithIdFailed = 4,
  kGetSizeFailed = 5,
  kMaxValue = kGetSizeFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayedLaunchIdStatus {
  kSuccess = 0,
  kDecodeLaunchIdFailed = 1,
  kMaxValue = kDecodeLaunchIdFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetNotificationLaunchIdStatus {
  kSuccess = 0,
  kNotificationGetContentFailed = 1,
  kGetElementsByTagFailed = 2,
  kMissingToastElementInDoc = 3,
  kItemAtFailed = 4,
  kGetAttributesFailed = 5,
  kGetNamedItemFailed = 6,
  kGetFirstChildFailed = 7,
  kGetNodeValueFailed = 8,
  kConversionToPropValueFailed = 9,
  kGetStringFailed = 10,
  kGetNamedItemNull = 11,
  kGetFirstChildNull = 12,
  kMaxValue = kGetFirstChildNull,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetSettingPolicy {
  kEnabled = 0,
  kDisabledForApplication = 1,
  kDisabledForUser = 2,
  kDisabledByGroupPolicy = 3,
  kDisabledByManifest = 4,
  kUnknown = 5,
  kMaxValue = kUnknown,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetSettingStatus {
  kSuccess = 0,
  kUnknownFailure = 1,
  kMaxValue = kUnknownFailure,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActivationStatus {
  kSuccess = 0,
  kDeprecatedGetProfileIdInvalidLaunchId = 1,
  kDeprecatedActivationInvalidLaunchId = 2,
  kInvalidLaunchId = 3,
  kMaxValue = kInvalidLaunchId,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HandleEventStatus {
  kSuccess = 0,
  kHandleEventLaunchIdInvalid = 1,
  kMaxValue = kHandleEventLaunchIdInvalid,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SetReadyCallbackStatus {
  kSuccess = 0,
  kShortcutMisconfiguration = 1 << 0,
  kComServerMisconfiguration = 1 << 1,
  kComNotInitializedObsolete = 1 << 2,  // No longer possible w/ Win10+ only.
  kMaxValue = kComNotInitializedObsolete,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnFailedStatus {
  kSuccess = 0,
  kGetErrorCodeFailed = 1,
  kMaxValue = kGetErrorCodeFailed,
};

// Methods to log histograms (to detect error rates in Native Notifications on
// Windows).
void LogDisplayHistogram(DisplayStatus status);
void LogCloseHistogram(CloseStatus status);
void LogHistoryHistogram(HistoryStatus status);
void LogGetDisplayedStatus(GetDisplayedStatus status);
void LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus status);
void LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus status);
void LogGetSettingPolicy(GetSettingPolicy policy);
void LogGetSettingStatus(GetSettingStatus status);
void LogGetSettingPolicyStartup(GetSettingPolicy policy);
void LogGetSettingStatusStartup(GetSettingStatus status);
void LogHandleEventStatus(HandleEventStatus status);
void LogActivationStatus(ActivationStatus status);
void LogSetReadyCallbackStatus(SetReadyCallbackStatus status);
void LogOnFailedStatus(OnFailedStatus status);

}  // namespace notifications_uma

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_METRICS_H_
