// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_CREATOR_H_
#define CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_CREATOR_H_

#include <string>

#include "base/types/pass_key.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "components/infobars/core/infobar_delegate.h"

namespace gfx {
struct VectorIcon;
}

namespace infobars {
class ContentInfoBarManager;
}

// Gated method for creating a simple alert infobar on Desktop.
//
// Creates a simple alert infobar and delegate and adds the infobar to
// |infobar_manager|. If |vector_icon| is not null, it will be shown.
// |infobar_identifier| names what class triggered the infobar for metrics.
//
// New Desktop infobars MUST use the centralized BrowserInfoBarManager framework
// (via InfoBarSpec) rather than calling CreateSimpleAlertInfoBar().
void CreateSimpleAlertInfoBar(
    base::PassKey<ConfirmInfoBarCreationAllowlist> pass_key,
    infobars::ContentInfoBarManager* infobar_manager,
    infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
    const gfx::VectorIcon* vector_icon,
    const std::u16string& message,
    bool auto_expire = true,
    bool should_animate = true,
    bool closeable = true,
    infobars::InfoBarDelegate::InfobarPriority infobar_priority =
        infobars::InfoBarDelegate::InfobarPriority::kDefault);

#endif  // CHROME_BROWSER_INFOBARS_SIMPLE_ALERT_INFOBAR_CREATOR_H_
