// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfNotificationReceiver_jni.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/share/android/jni_headers/NotificationManager_jni.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_wrapper.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

content::WebContents* GetWebContentsForProfile(Profile* profile) {
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile)
      continue;

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents)
        return web_contents;
    }
  }
  return nullptr;
}

}  // namespace

namespace send_tab_to_self {

AndroidNotificationHandler::AndroidNotificationHandler(Profile* profile)
    : profile_(profile) {}

AndroidNotificationHandler::~AndroidNotificationHandler() {
  while (!queued_messages_.empty()) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        queued_messages_.at(0).get(), messages::DismissReason::UNKNOWN);
  }
  DCHECK(queued_messages_.size() == 0);
}

void AndroidNotificationHandler::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (const SendTabToSelfEntry* entry : new_entries) {
    if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfV2)) {
      web_contents_ = GetWebContentsForProfile(profile_);

      std::unique_ptr<messages::MessageWrapper> message =
          std::make_unique<messages::MessageWrapper>(
              messages::MessageIdentifier::SEND_TAB_TO_SELF);

      message->SetActionClick(base::BindOnce(
          &AndroidNotificationHandler::OnMessageOpened, base::Unretained(this),
          entry->GetURL(), entry->GetGUID()));
      message->SetDismissCallback(base::BindOnce(
          &AndroidNotificationHandler::OnMessageDismissed,
          base::Unretained(this), message.get(), entry->GetGUID()));

      message->SetTitle(l10n_util::GetStringFUTF16(
          IDS_SEND_TAB_TO_SELF_MESSAGE,
          base::UTF8ToUTF16(entry->GetDeviceName())));
      message->SetDescription(
          url_formatter::FormatUrlForSecurityDisplay(entry->GetURL()));
      message->SetDescriptionMaxLines(1);
      message->SetPrimaryButtonText(
          l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_MESSAGE_OPEN));
      message->SetIconResourceId(
          ResourceMapper::MapToJavaDrawableId(IDR_SEND_TAB_TO_SELF));

      // TODO(crbug.com/1220129): A valid WebContents shouldn't be needed here.
      if (web_contents_) {
        messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
            message.get(), web_contents_->GetTopLevelNativeWindow(),
            messages::MessagePriority::kNormal);
        queued_messages_.push_back(std::move(message));
      } else {
        pending_messages_.push(std::move(message));
      }
    } else {
      JNIEnv* env = AttachCurrentThread();

      // Set the expiration to 10 days from when the notification is displayed.
      base::Time expiraton_time =
          entry->GetSharedTime() + base::TimeDelta::FromDays(10);

      ScopedJavaLocalRef<jclass> send_tab_to_self_notification_receiver_class =
          Java_SendTabToSelfNotificationReceiver_getSendTabToSelfNotificationReciever(
              env);

      Java_NotificationManager_showNotification(
          env, ConvertUTF8ToJavaString(env, entry->GetGUID()),
          ConvertUTF8ToJavaString(env, entry->GetURL().spec()),
          ConvertUTF8ToJavaString(env, entry->GetTitle()),
          ConvertUTF8ToJavaString(env, entry->GetDeviceName()),
          expiraton_time.ToJavaTime(),
          send_tab_to_self_notification_receiver_class);
    }
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

void AndroidNotificationHandler::OnMessageOpened(GURL url, std::string guid) {
  DCHECK(web_contents_);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  params.should_replace_current_entry = false;
  web_contents_->OpenURL(params);
  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
                    ->GetSendTabToSelfModel();
  model->DismissEntry(guid);
}

void AndroidNotificationHandler::OnMessageDismissed(
    messages::MessageWrapper* message,
    std::string guid,
    messages::DismissReason dismiss_reason) {
  for (unsigned int i = 0; i < queued_messages_.size(); i++) {
    if (queued_messages_.at(i).get() == message) {
      queued_messages_.erase(queued_messages_.begin() + i);
      auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile_)
                        ->GetSendTabToSelfModel();
      model->DismissEntry(guid);
    }
  }
}

void AndroidNotificationHandler::UpdateWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
  while (!pending_messages_.empty()) {
    queued_messages_.push_back(std::move(pending_messages_.front()));
    pending_messages_.pop();
    messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
        std::move(queued_messages_.back().get()),
        web_contents_->GetTopLevelNativeWindow(),
        messages::MessagePriority::kNormal);
  }
}

}  // namespace send_tab_to_self
