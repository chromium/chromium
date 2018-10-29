// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/android_content_suggestions_notifier.h"

#include <jni.h>
#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/ntp_snippets/features.h"
#include "components/prefs/pref_service.h"
#include "jni/ContentSuggestionsNotifier_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using base::android::JavaParamRef;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::kNotificationsFeature;
using ntp_snippets::kNotificationsIgnoredLimitParam;
using ntp_snippets::kNotificationsIgnoredDefaultLimit;

AndroidContentSuggestionsNotifier::AndroidContentSuggestionsNotifier() =
    default;

bool AndroidContentSuggestionsNotifier::SendNotification(
    const ContentSuggestion::ID& id,
    const GURL& url,
    const base::string16& title,
    const base::string16& text,
    const gfx::Image& image,
    base::Time timeout_at,
    int priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  SkBitmap skimage = image.AsImageSkia().GetRepresentation(1.0f).GetBitmap();
  if (skimage.empty())
    return false;

  jlong timeout_at_millis = timeout_at.ToJavaTime();
  if (timeout_at == base::Time::Max()) {
    timeout_at_millis = std::numeric_limits<jlong>::max();
  }

  if (Java_ContentSuggestionsNotifier_showNotification(
          env, id.category().id(),
          base::android::ConvertUTF8ToJavaString(env, id.id_within_category()),
          base::android::ConvertUTF8ToJavaString(env, url.spec()),
          base::android::ConvertUTF16ToJavaString(env, title),
          base::android::ConvertUTF16ToJavaString(env, text),
          gfx::ConvertToJavaBitmap(&skimage), timeout_at_millis, priority)) {
    DVLOG(1) << "Displayed notification for " << id;
    return true;
  } else {
    DVLOG(1) << "Suppressed notification for " << url.spec();
    return false;
  }
}

void AndroidContentSuggestionsNotifier::HideNotification(
    const ContentSuggestion::ID& id,
    ContentSuggestionsNotificationAction why) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotifier_hideNotification(
      env, id.category().id(),
      base::android::ConvertUTF8ToJavaString(env, id.id_within_category()),
      static_cast<int>(why));
}

void AndroidContentSuggestionsNotifier::HideAllNotifications(
    ContentSuggestionsNotificationAction why) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotifier_hideAllNotifications(env,
                                                       static_cast<int>(why));
}

void AndroidContentSuggestionsNotifier::FlushCachedMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotifier_flushCachedMetrics(env);
}

bool AndroidContentSuggestionsNotifier::RegisterChannel(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ContentSuggestionsNotifier_registerChannel(env, enabled);
}

void AndroidContentSuggestionsNotifier::UnregisterChannel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ContentSuggestionsNotifier_unregisterChannel(env);
}

static void JNI_ContentSuggestionsNotifier_RecordNotificationOptOut(
    JNIEnv* env,
    const JavaParamRef<jclass>& class_object,
    jint reason) {
  RecordContentSuggestionsNotificationOptOut(
      static_cast<ContentSuggestionsNotificationOptOut>(reason));
}

static void JNI_ContentSuggestionsNotifier_RecordNotificationAction(
    JNIEnv* env,
    const JavaParamRef<jclass>& class_object,
    jint action) {
  RecordContentSuggestionsNotificationAction(
      static_cast<ContentSuggestionsNotificationAction>(action));
}

static void JNI_ContentSuggestionsNotifier_ReceiveFlushedMetrics(
    JNIEnv* env,
    const JavaParamRef<jclass>& class_object,
    jint tap_count,
    jint dismissal_count,
    jint hide_deadline_count,
    jint hide_expiry_count,
    jint hide_frontmost_count,
    jint hide_disabled_count,
    jint hide_shutdown_count,
    jint consecutive_ignored) {
  DVLOG(1) << "Flushing metrics: tap_count=" << tap_count
           << "; dismissal_count=" << dismissal_count
           << "; hide_deadline_count=" << hide_deadline_count
           << "; hide_expiry_count=" << hide_expiry_count
           << "; hide_frontmost_count=" << hide_frontmost_count
           << "; hide_disabled_count=" << hide_disabled_count
           << "; hide_shutdown_count=" << hide_shutdown_count
           << "; consecutive_ignored=" << consecutive_ignored;
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  PrefService* prefs = profile->GetPrefs();

  for (int i = 0; i < tap_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::TAP);
  }
  for (int i = 0; i < dismissal_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::DISMISSAL);
  }
  for (int i = 0; i < hide_deadline_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::HIDE_DEADLINE);
  }
  for (int i = 0; i < hide_expiry_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::HIDE_EXPIRY);
  }
  for (int i = 0; i < hide_frontmost_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::HIDE_FRONTMOST);
  }
  for (int i = 0; i < hide_disabled_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::HIDE_DISABLED);
  }
  for (int i = 0; i < hide_shutdown_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        ContentSuggestionsNotificationAction::HIDE_SHUTDOWN);
  }

  const bool was_enabled =
      ContentSuggestionsNotifier::ShouldSendNotifications(prefs);
  if (tap_count == 0) {
    // There were no taps, consecutive_ignored has not been reset and continues
    // from where it left off. If there was a tap, then Java has provided us
    // with the number of ignored notifications since that point.
    consecutive_ignored +=
        prefs->GetInteger(prefs::kContentSuggestionsConsecutiveIgnoredPrefName);
  }
  prefs->SetInteger(prefs::kContentSuggestionsConsecutiveIgnoredPrefName,
                    consecutive_ignored);
  const bool is_enabled =
      ContentSuggestionsNotifier::ShouldSendNotifications(prefs);
  if (was_enabled && !is_enabled) {
    RecordContentSuggestionsNotificationOptOut(
        ContentSuggestionsNotificationOptOut::IMPLICIT);
  }
}
