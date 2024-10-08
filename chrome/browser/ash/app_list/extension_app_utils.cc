// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/extension_app_utils.h"

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

namespace app_list {

bool ShouldShowInLauncher(const extensions::Extension* extension,
                          content::BrowserContext* context) {
  return ash::DemoSession::ShouldShowExtensionInAppLauncher(extension->id()) &&
         extensions::ui_util::ShouldDisplayInAppLauncher(extension, context);
}

void AddMenuItemIconsForSystemApps(const std::string& app_id,
                                   ui::SimpleMenuModel* menu_model,
                                   int start_index,
                                   int count) {
  if (app_id != extension_misc::kFilesManagerAppId)
    return;

  for (int i = 0; i < count; ++i) {
    const int index = start_index + i;
    if (menu_model->GetLabelAt(index) ==
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW)) {
      menu_model->SetIcon(index, ui::ImageModel::FromVectorIcon(
                                     views::kNewWindowIcon, ui::kColorMenuIcon,
                                     ash::kAppContextMenuIconSize));
    }
  }
}

}  // namespace app_list
