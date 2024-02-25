// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_

#include <queue>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/messages/android/message_wrapper.h"

namespace content {
class WebContents;
}

class GURL;
class Profile;

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Responsible for displaying notifications on Android. Overrides
// ReceivingUIHandler so that it is called for all updates to share
// entries.
class AndroidNotificationHandler : public ReceivingUiHandler {
 public:
  explicit AndroidNotificationHandler(Profile* profile);
  ~AndroidNotificationHandler() override;

  const Profile* profile() const override;

  void UpdateWebContents(content::WebContents* web_contents);

 private:
  void DisplayNewEntriesOnUIThread(
      const std::vector<SendTabToSelfEntry>& new_entries);

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;

  void OnMessageOpened(GURL url, std::string guid);
  // Called whenever the message is dismissed (e.g. after timeout or because the
  // user already accepted or declined the message).
  void OnMessageDismissed(messages::MessageWrapper* message,
                          std::string guid,
                          messages::DismissReason dismiss_reason);

  // Messages that have not yet been queued due to no active WebContents.
  std::queue<std::unique_ptr<messages::MessageWrapper>> pending_messages_;

  // Messages that have already been enqueued via
  // messages::MessageDispatcherBridge.
  std::vector<std::unique_ptr<messages::MessageWrapper>> queued_messages_;

  raw_ptr<Profile> profile_;

  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtrFactory<AndroidNotificationHandler> weak_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
