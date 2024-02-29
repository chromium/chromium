// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/process_sharing_infobar_delegate.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

ProcessSharingInfobarDelegate::ProcessSharingInfobarDelegate(
    content::WebContents* web_contents)
    : inspected_web_contents_(web_contents->GetWeakPtr()) {}

ProcessSharingInfobarDelegate::~ProcessSharingInfobarDelegate() = default;

std::u16string ProcessSharingInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR);
}

int ProcessSharingInfobarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
ProcessSharingInfobarDelegate::GetIdentifier() const {
  return DEV_TOOLS_SHARED_PROCESS_DELEGATE;
}
