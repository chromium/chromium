// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

namespace ash {

// Describes the contents of an AnchoredNudge, which is a notifier that anchors
// to an `anchor_view` and informs users about something that might enhance
// their experience immediately. See the "Educational Nudges" section in
// go/notifier-framework for example usages.
struct ASH_PUBLIC_EXPORT AnchoredNudgeData {
  AnchoredNudgeData(
      const std::string& id,
      AnchoredNudgeCatalogName catalog_name,
      const std::u16string& text,
      views::View* anchor_view,
      bool has_dismiss_button = false,
      const std::u16string& custom_dismiss_text = std::u16string(),
      base::RepeatingClosure dismiss_callback = base::RepeatingClosure(),
      const gfx::VectorIcon& leading_icon = gfx::kNoneIcon);
  AnchoredNudgeData(AnchoredNudgeData&& other);
  AnchoredNudgeData& operator=(AnchoredNudgeData&& other);
  ~AnchoredNudgeData();

  std::string id;
  AnchoredNudgeCatalogName catalog_name;
  std::u16string text;

  // Unowned. Must outlive the `AnchoredNudge` created with this by observing
  // its `OnViewIsDeleting()` in `AnchoredNudgeManagerImpl`.
  raw_ptr<views::View> anchor_view;

  views::BubbleBorder::Arrow arrow = views::BubbleBorder::BOTTOM_CENTER;

  // If `has_dismiss_button` is true, it will use the default dismiss text
  // unless a non-empty `custom_dismiss_text` is given.
  std::u16string dismiss_text;

  // To disable dismiss via timer, set `has_infinite_duration_` to true.
  // A nudge with infinite duration will be displayed until the dismiss button
  // on the nudge is clicked, or when it is destroyed due to other reasons (e.g.
  // anchor view is deleted, user locks session, etc.)
  bool has_infinite_duration = false;

  // TODO(b/259100049): We should turn this into a `OnceClosure`.
  base::RepeatingClosure dismiss_callback;
  const gfx::VectorIcon* leading_icon;
  base::OnceClosure expired_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
