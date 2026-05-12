// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NotificationManager_jni.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfNotificationReceiver_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace send_tab_to_self {

namespace {

std::optional<std::string> GetScrollToTextFragmentFromEntry(
    const SendTabToSelfEntry& entry) {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfPropagateScrollPosition) ||
      entry.GetPageContext().scroll_position.IsEmpty()) {
    return std::nullopt;
  }

  shared_highlighting::TextFragment tf =
      entry.GetPageContext()
          .scroll_position.text_fragment.ToSharedHighlightingTextFragment();

  return tf.ToEscapedString(shared_highlighting::TextFragment::
                                EscapedStringFormat::kWithoutTextDirective);
}

}  // namespace

AndroidNotificationHandler::AndroidNotificationHandler(
    SendTabToSelfModel* send_tab_to_self_model)
    : send_tab_to_self_model_((send_tab_to_self_model)) {}

AndroidNotificationHandler::~AndroidNotificationHandler() {}

void AndroidNotificationHandler::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  std::vector<SendTabToSelfEntry> vector_copy;

  for (const SendTabToSelfEntry* entry : new_entries) {
    vector_copy.push_back(*entry);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AndroidNotificationHandler::DisplayNewEntriesOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(vector_copy)));
}

void AndroidNotificationHandler::DisplayNewEntriesOnUIThread(
    const std::vector<SendTabToSelfEntry>& new_entries) {
  for (const SendTabToSelfEntry& entry : new_entries) {
    JNIEnv* env = AttachCurrentThread();

    // Set the expiration to 10 days from when the notification is displayed.
    base::Time expiration_time = entry.GetSharedTime() + base::Days(10);

    ScopedJavaLocalRef<jclass> send_tab_to_self_notification_receiver_class =
        Java_SendTabToSelfNotificationReceiver_getSendTabToSelfNotificationReciever(
            env);

    std::optional<std::string> internal_scroll_to_text_fragment =
        GetScrollToTextFragmentFromEntry(entry);

    Java_NotificationManager_showNotification(
        env, ConvertUTF8ToJavaString(env, entry.GetGUID()),
        ConvertUTF8ToJavaString(env, entry.GetURL().spec()),
        ConvertUTF8ToJavaString(env, entry.GetTitle()),
        ConvertUTF8ToJavaString(env, entry.GetDeviceName()),
        expiration_time.InMillisecondsSinceUnixEpoch(),
        send_tab_to_self_notification_receiver_class,
        internal_scroll_to_text_fragment
            ? ConvertUTF8ToJavaString(env, *internal_scroll_to_text_fragment)
            : nullptr);
  }
}

void AndroidNotificationHandler::DismissEntries(
    const std::vector<std::string>& guids) {
  JNIEnv* env = AttachCurrentThread();

  for (const std::string& guid : guids) {
    Java_NotificationManager_hideNotification(
        env, ConvertUTF8ToJavaString(env, guid));
  }
}

}  // namespace send_tab_to_self

DEFINE_JNI(NotificationManager)
DEFINE_JNI(SendTabToSelfNotificationReceiver)
