// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void NaClInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(new NaClInfoBarDelegate())));
}

NaClInfoBarDelegate::NaClInfoBarDelegate() = default;

NaClInfoBarDelegate::~NaClInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
NaClInfoBarDelegate::GetIdentifier() const {
  return NACL_INFOBAR_DELEGATE;
}

std::u16string NaClInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL NaClInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome/?p=ib_nacl");
}

std::u16string NaClInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_NACL_APP_MISSING_ARCH_MESSAGE);
}

int NaClInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
