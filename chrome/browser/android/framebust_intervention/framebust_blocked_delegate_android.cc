// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/framebust_intervention/framebust_blocked_delegate_android.h"

#include "chrome/grit/generated_resources.h"
#include "components/blocked_content/android/popup_blocked_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace blocked_content {

bool FramebustBlockedMessageDelegate::ShowMessage(
    const GURL& blocked_url,
    HostContentSettingsMap* settings_map,
    OutcomeCallback intervention_callback) {
  if (message_ != nullptr) {  // update description only
    blocked_url_ = blocked_url;
    message_->SetDescription(url_formatter::FormatUrlForSecurityDisplay(
        blocked_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    return false;
  }

  intervention_callback_ = std::move(intervention_callback);
  url_ = GetWebContents().GetLastCommittedURL();
  blocked_url_ = blocked_url;

  // Unretained is safe because |this| will always outlive |message_| which owns
  // the callback.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::FRAMEBUST_BLOCKED,
      base::BindOnce(&FramebustBlockedMessageDelegate::HandleClick,
                     base::Unretained(this)),
      base::BindOnce(&FramebustBlockedMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));

  message->SetTitle(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_TITLE));
  message->SetDescription(url_formatter::FormatUrlForSecurityDisplay(
      blocked_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  messages::MessageDispatcherBridge* message_dispatcher_bridge =
      messages::MessageDispatcherBridge::Get();
  DCHECK(message_dispatcher_bridge->IsMessagesEnabledForEmbedder());

  message->SetSecondaryIconResourceId(
      message_dispatcher_bridge->MapToJavaDrawableId(IDR_ANDROID_SETTINGS));
  message->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_OPEN_REDIRECT));
  message->SetSecondaryActionCallback(
      base::BindRepeating(&FramebustBlockedMessageDelegate::HandleOpenLink,
                          base::Unretained(this)));

  settings_map_ = settings_map;
  allow_settings_changes_ = !PopupSettingManagedByPolicy(settings_map_, url_);

  // Don't allow the user to configure the setting in the UI if the setting
  // is managed by policy.
  int button_text_id =
      allow_settings_changes_ ? IDS_ALWAYS_ALLOW_REDIRECTS : IDS_OK;
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(button_text_id));
  message->SetIconResourceId(message_dispatcher_bridge->MapToJavaDrawableId(
      IDR_ANDROID_INFOBAR_BLOCKED_POPUPS));

  // On rare occasions, such as the moment when activity is being recreated
  // or destroyed, framebust blocked message will not be displayed and the
  // method will return false.
  if (!message_dispatcher_bridge->EnqueueMessage(
          message.get(), &GetWebContents(),
          messages::MessageScopeType::NAVIGATION,
          messages::MessagePriority::kNormal)) {
    return false;
  }

  message_ = std::move(message);
  return true;
}

FramebustBlockedMessageDelegate::~FramebustBlockedMessageDelegate() {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

FramebustBlockedMessageDelegate::FramebustBlockedMessageDelegate(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FramebustBlockedMessageDelegate>(
          *web_contents) {}

void FramebustBlockedMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  message_.reset();
  settings_map_ = nullptr;

  if (intervention_callback_)
    std::move(intervention_callback_).Run(InterventionOutcome::kAccepted);
}

void FramebustBlockedMessageDelegate::HandleClick() {
  if (!allow_settings_changes_) {
    return;
  }

  // Create exceptions. The allow pop-ups setting also allows redirects.
  settings_map_->SetNarrowestContentSetting(
      url_, url_, ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  HandleOpenLink();
}

void FramebustBlockedMessageDelegate::HandleOpenLink() {
  GetWebContents().OpenURL(
      content::OpenURLParams(blocked_url_, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});

  if (intervention_callback_)
    std::move(intervention_callback_)
        .Run(InterventionOutcome::kDeclinedAndNavigated);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FramebustBlockedMessageDelegate);

}  // namespace blocked_content
