// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_prompt_android.h"

#include <memory>

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      permission_request_notification_(nullptr),
      permission_infobar_(nullptr),
      weak_factory_(this) {
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (infobar_service &&
      GroupedPermissionInfoBarDelegate::ShouldShowMiniInfobar(
          profile, GetContentSettingType(0u /* position */))) {
    permission_infobar_ = GroupedPermissionInfoBarDelegate::Create(
        weak_factory_.GetWeakPtr(), infobar_service);
    infobar_service->AddObserver(this);
    return;
  }

  if (PermissionRequestNotificationAndroid::ShouldShowAsNotification(
          profile, GetContentSettingType(0u /* position */))) {
    permission_request_notification_ =
        PermissionRequestNotificationAndroid::Create(web_contents_, delegate_);
    return;
  }

  PermissionDialogDelegate::Create(web_contents_, this);
}

PermissionPromptAndroid::~PermissionPromptAndroid() {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (!infobar_service)
    return;
  // RemoveObserver before RemoveInfoBar to not get notified about the removal
  // of the `permission_infobar_` infobar.
  infobar_service->RemoveObserver(this);
  if (permission_infobar_) {
    infobar_service->RemoveInfoBar(permission_infobar_);
  }
}

void PermissionPromptAndroid::UpdateAnchorPosition() {
  NOTREACHED() << "UpdateAnchorPosition is not implemented";
}

gfx::NativeWindow PermissionPromptAndroid::GetNativeWindow() {
  NOTREACHED() << "GetNativeWindow is not implemented";
  return nullptr;
}

PermissionPrompt::TabSwitchingBehavior
PermissionPromptAndroid::GetTabSwitchingBehavior() {
  if (permission_request_notification_)
    return permission_request_notification_->GetTabSwitchingBehavior();
  return TabSwitchingBehavior::kKeepPromptAlive;
}

void PermissionPromptAndroid::Closing() {
  delegate_->Closing();
}

void PermissionPromptAndroid::Accept() {
  delegate_->Accept();
}

void PermissionPromptAndroid::Deny() {
  delegate_->Deny();
}

size_t PermissionPromptAndroid::PermissionCount() const {
  return delegate_->Requests().size();
}

ContentSettingsType PermissionPromptAndroid::GetContentSettingType(
    size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  CHECK_LT(position, requests.size());
  return requests[position]->GetContentSettingsType();
}

// Grouped permission requests can only be Mic+Camera or Camera+Mic
static void CheckValidRequestGroup(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK_EQ(static_cast<size_t>(2), requests.size());
  DCHECK_EQ(requests[0]->GetOrigin(), requests[1]->GetOrigin());
  DCHECK((requests[0]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_MIC &&
          requests[1]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA) ||
         (requests[0]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA &&
          requests[1]->GetPermissionRequestType() ==
              PermissionRequestType::PERMISSION_MEDIASTREAM_MIC));
}

int PermissionPromptAndroid::GetIconId() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconId();
  CheckValidRequestGroup(requests);
  return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 PermissionPromptAndroid::GetTitleText() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetTitleText();
  CheckValidRequestGroup(requests);
  return l10n_util::GetStringUTF16(
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_PERMISSION_TITLE);
}

base::string16 PermissionPromptAndroid::GetMessageText() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1)
    return requests[0]->GetMessageText();
  CheckValidRequestGroup(requests);
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
      url_formatter::FormatUrlForSecurityDisplay(
          requests[0]->GetOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

void PermissionPromptAndroid::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                               bool animate) {
  if (infobar != permission_infobar_)
    return;

  permission_infobar_ = nullptr;
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  if (infobar_service)
    infobar_service->RemoveObserver(this);
}

void PermissionPromptAndroid::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  permission_infobar_ = nullptr;
  manager->RemoveObserver(this);
}

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return std::make_unique<PermissionPromptAndroid>(web_contents, delegate);
}
