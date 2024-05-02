// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/permissions/permission_blocked_dialog_controller_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace permissions {
class PermissionPromptAndroid;
}

// A message ui that displays a notification permission request, which is an
// alternative ui to the mini infobar.
class PermissionBlockedMessageDelegate
    : public PermissionBlockedDialogController::Delegate,
      public permissions::PermissionsClient::PermissionMessageDelegate,
      public content::WebContentsObserver {
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
    virtual void SetManageClicked();
    virtual void SetLearnMoreClicked();
    virtual bool ShouldUseQuietUI();
    virtual std::optional<permissions::PermissionUiSelector::QuietUiReason>
    ReasonForUsingQuietUi();
    virtual ContentSettingsType GetContentSettingsType();

   private:
    base::WeakPtr<permissions::PermissionPromptAndroid> permission_prompt_;
  };

  PermissionBlockedMessageDelegate(content::WebContents* web_contents,
                                     std::unique_ptr<Delegate> delegate);
  ~PermissionBlockedMessageDelegate() override;

 protected:
  // PermissionBlockedDialogController::Delegate:
  void OnContinueBlocking() override;
  void OnAllowForThisSite() override;
  void OnLearnMoreClicked() override;
  void OnOpenedSettings() override;
  void OnDialogDismissed() override;
  ContentSettingsType GetContentSettingsType() override;

  // content::WebContentsObserver implementation.
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

 private:
  friend class PermissionBlockedMessageDelegateAndroidTest;

  void HandlePrimaryActionClick();
  void HandleDismissCallback(messages::DismissReason reason);
  void HandleManageClick();

  void DismissInternal();

  // `message_` and `dialog_controller_` can not be alive at the same moment,
  // since message ui and dialog ui won't show together.
  std::unique_ptr<messages::MessageWrapper> message_;
  std::unique_ptr<PermissionBlockedDialogController> dialog_controller_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<Delegate> delegate_;

  // Whether we should re-show the dialog to users when users return to the tab.
  bool should_reshow_dialog_on_focus_ = false;
  bool has_interacted_with_dialog_ = false;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_BLOCKED_MESSAGE_DELEGATE_ANDROID_H_
