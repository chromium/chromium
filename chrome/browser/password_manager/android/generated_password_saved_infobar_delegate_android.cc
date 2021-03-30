// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/generated_password_saved_infobar_delegate_android.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/android/password_manager_launcher_android.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

GeneratedPasswordSavedInfoBarDelegateAndroid::
    ~GeneratedPasswordSavedInfoBarDelegateAndroid() {}

void GeneratedPasswordSavedInfoBarDelegateAndroid::OnInlineLinkClicked() {
  password_manager_launcher::ShowPasswordSettings(
      InfoBarService::WebContentsFromInfoBar(infobar()),
      password_manager::ManagePasswordsReferrer::
          kPasswordGenerationConfirmation);
}

GeneratedPasswordSavedInfoBarDelegateAndroid::
    GeneratedPasswordSavedInfoBarDelegateAndroid()
    : button_label_(l10n_util::GetStringUTF16(IDS_OK)) {
  std::u16string link = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_LINK);

  size_t offset = 0;
  message_text_ =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE);
  details_message_text_ = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT, link, &offset);
  inline_link_range_ = gfx::Range(offset, offset + link.length());
}

infobars::InfoBarDelegate::InfoBarIdentifier
GeneratedPasswordSavedInfoBarDelegateAndroid::GetIdentifier() const {
  return GENERATED_PASSWORD_SAVED_INFOBAR_DELEGATE_ANDROID;
}

int GeneratedPasswordSavedInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_SAVE_PASSWORD;
}
