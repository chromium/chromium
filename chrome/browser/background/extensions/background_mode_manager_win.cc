// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/extensions/background_mode_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notifier_id.h"

const char kAppInstalledNotifierId[] = "background-mode.app-installed";

void BackgroundModeManager::DisplayClientInstalledNotification(
    const std::u16string& name) {
  // Create a status tray notification balloon explaining to the user what has
  // been installed.
  CreateStatusTrayIcon();
  status_icon_->DisplayBalloon(
      gfx::ImageSkia(),
      l10n_util::GetStringUTF16(IDS_BACKGROUND_APP_INSTALLED_BALLOON_TITLE),
      l10n_util::GetStringFUTF16(IDS_BACKGROUND_APP_INSTALLED_BALLOON_BODY,
                                 name,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kAppInstalledNotifierId));
}
