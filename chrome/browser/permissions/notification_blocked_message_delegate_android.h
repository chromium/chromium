// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_NOTIFICATION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_NOTIFICATION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/permissions/permissions_client.h"

namespace content {
class WebContents;
}

namespace permissions {
class PermissionPromptAndroid;
}

// A message ui that displays a notification permission request, which is an
// alternative ui to the mini infobar.
class NotificationBlockedMessageDelegate
    : public permissions::PermissionsClient::PermissionMessageDelegate {
 public:
  // Delegate to mock out the |PermissionPromptAndroid| for testing.
  class Delegate {
   public:
    Delegate();
    Delegate(const base::WeakPtr<permissions::PermissionPromptAndroid>&
                 permission_prompt);
    virtual ~Delegate();
    virtual void Accept();
    virtual void Deny();
    virtual void Closing();
    virtual bool IsPromptDestroyed();
    virtual bool ShouldUseQuietUI();

   private:
    base::WeakPtr<permissions::PermissionPromptAndroid> permission_prompt_;
  };

  NotificationBlockedMessageDelegate(content::WebContents* web_contents,
                                     std::unique_ptr<Delegate> delegate);
  ~NotificationBlockedMessageDelegate() override;

 private:
  friend class NotificationBlockedMessageDelegateAndroidTest;

  explicit NotificationBlockedMessageDelegate(
      content::WebContents* web_contents);

  void HandlePrimaryActionClick();
  void HandleDismissCallback(messages::DismissReason reason);
  void HandleManageClick();

  void DismissInternal();

  std::unique_ptr<messages::MessageWrapper> message_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_NOTIFICATION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_
