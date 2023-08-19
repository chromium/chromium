// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/simple_alert_infobar_creator.h"

#include <memory>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

void CreateSimpleAlertInfoBar(
    infobars::ContentInfoBarManager* infobar_manager,
    infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
    const gfx::VectorIcon* vector_icon,
    const std::u16string& message,
    bool auto_expire,
    bool should_animate,
    bool closeable) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<SimpleAlertInfoBarDelegate>(
          infobar_identifier, vector_icon, message, auto_expire, should_animate,
          closeable)));
}
