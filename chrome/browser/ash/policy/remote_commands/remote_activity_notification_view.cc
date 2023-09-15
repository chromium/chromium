// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_view.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "base/functional/callback_forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace policy {

// TODO(b/299143143): Update the notification view with UX requirements.
RemoteActivityNotificationView::RemoteActivityNotificationView(
    base::RepeatingClosure on_button_pressed) {
  views::Builder<views::View>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 20))
      .SetBackground(views::CreateSolidBackground(SK_ColorYELLOW))
      .BuildChildren();

  auto button = std::make_unique<views::MdTextButton>(
      std::move(on_button_pressed),
      l10n_util::GetStringUTF16(IDS_ASH_AUTOCLICK_SCROLL_CLOSE));
  button->SetBackground(views::CreateSolidBackground(SK_ColorRED));
  button->SetProminent(true);
  AddChildView(std::move(button));
}

BEGIN_METADATA(RemoteActivityNotificationView, views::View)
END_METADATA

}  // namespace policy
