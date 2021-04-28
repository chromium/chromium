// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/simple_alert_infobar_creator.h"

#include <memory>

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

void CreateSimpleAlertInfoBar(
    InfoBarService* infobar_service,
    infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
    const gfx::VectorIcon* vector_icon,
    const std::u16string& message,
    bool auto_expire,
    bool should_animate) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<SimpleAlertInfoBarDelegate>(
          infobar_identifier, vector_icon, message, auto_expire,
          should_animate)));
}
