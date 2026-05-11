// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/known_interception_disclosure_message_delegate.h"

#include "base/memory/singleton.h"
#include "base/strings/strcat.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

KnownInterceptionDisclosureMessageDelegate::
    KnownInterceptionDisclosureMessageDelegate(
        content::WebContents* web_contents)
    : content::WebContentsUserData<KnownInterceptionDisclosureMessageDelegate>(
          *web_contents),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
}

KnownInterceptionDisclosureMessageDelegate::
    ~KnownInterceptionDisclosureMessageDelegate() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void KnownInterceptionDisclosureMessageDelegate::MaybeShow() {
  if (message_) {
    return;
  }

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::KNOWN_INTERCEPTION_DISCLOSURE,
      base::BindOnce(
          &KnownInterceptionDisclosureMessageDelegate::HandlePrimaryActionClick,
          base::Unretained(this)),
      base::BindOnce(
          &KnownInterceptionDisclosureMessageDelegate::HandleDismissCallback,
          base::Unretained(this)));

  message_->SetTitle(l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_HEADER));
  message_->SetDescription(
      l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_BODY1));
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_KNOWN_INTERCEPTION_INFOBAR_BUTTON_TEXT));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_WARNING));
  message_->EnableLargeIcon(true);

  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_IC_HELP_24DP));
  message_->SetSecondaryIconContentDescription(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  message_->SetSecondaryActionCallback(base::BindRepeating(
      [](content::WebContents* web_contents) {
        web_contents->OpenURL(
            content::OpenURLParams(
                GURL("chrome://connection-monitoring-detected/"),
                content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                ui::PAGE_TRANSITION_LINK, false /* is_renderer_initiated */),
            /*navigation_handle_callback=*/{});
      },
      web_contents_));

  // Keep message up for one minute.
  message_->SetDuration(60000);

  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kUrgent);
}

void KnownInterceptionDisclosureMessageDelegate::HandlePrimaryActionClick() {
  KnownInterceptionDisclosureCooldown::GetInstance()->Activate(profile_);
}

void KnownInterceptionDisclosureMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  KnownInterceptionDisclosureCooldown::GetInstance()->Activate(profile_);
  message_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(KnownInterceptionDisclosureMessageDelegate);
