// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
#define ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

namespace ash {

using HoverStateChangeCallback =
    base::RepeatingCallback<void(bool is_hovering)>;
using AnchoredNudgeClickCallback = base::RepeatingCallback<void()>;
using AnchoredNudgeDismissCallback = base::RepeatingCallback<void()>;

// Describes the contents of an AnchoredNudge, which is a notifier that anchors
// to an `anchor_view` and informs users about something that might enhance
// their experience immediately. See the "Educational Nudges" section in
// go/notifier-framework for example usages.
struct ASH_PUBLIC_EXPORT AnchoredNudgeData {
  AnchoredNudgeData(const std::string& id,
                    AnchoredNudgeCatalogName catalog_name,
                    const std::u16string& text,
                    views::View* anchor_view);
  AnchoredNudgeData(AnchoredNudgeData&& other);
  AnchoredNudgeData& operator=(AnchoredNudgeData&& other);
  ~AnchoredNudgeData();

  std::string id;
  AnchoredNudgeCatalogName catalog_name;
  std::u16string text;

  // Unowned. Must outlive the `AnchoredNudge`.
  raw_ptr<views::View> anchor_view;

  HoverStateChangeCallback hover_state_change_callback = base::DoNothing();
  AnchoredNudgeClickCallback nudge_click_callback = base::DoNothing();
  AnchoredNudgeDismissCallback nudge_dimiss_callback = base::DoNothing();

  // Used to set bubble placement in relation to the anchor view.
  // A value of `BOTTOM_CENTER` means that the nudge will be anchored from its
  // bottom center to the anchor view.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::BOTTOM_CENTER;

  // If `dismiss_text` is not empty, a dismiss button will be created.
  // Pressing the button will execute `dismiss_callback`, if any, followed by
  // the nudge being closed.
  std::u16string dismiss_text;
  // TODO(b/285023559): Add and use a `ChainedCancelCallback` class instead of a
  // `RepeatingClosure` so we don't have to manually modify the provided
  // callbacks in the manager.
  base::RepeatingClosure dismiss_callback;

  // If `second_button_text` is not empty, a second button will be created.
  // Pressing the button will execute `second_button_callback`, if any, followed
  // by the nudge being closed.
  // TODO(b/283159669): Will use `SystemToastStyle` with a second button
  // temporarily for M116, migrate to `DialogStyle` once implemented.
  std::u16string second_button_text;
  // TODO(b/285023559): Add and use a `ChainedCancelCallback` class instead of a
  // `RepeatingClosure` so we don't have to manually modify the provided
  // callbacks in the manager.
  base::RepeatingClosure second_button_callback;

  // To disable dismiss via timer, set `has_infinite_duration_` to true.
  // A nudge with infinite duration will be displayed until the dismiss button
  // on the nudge is clicked, or when it is destroyed due to other reasons (e.g.
  // anchor view is deleted, user locks session, etc.)
  bool has_infinite_duration = false;

  // If `leading_icon` has a value other than `kNoneIcon` it will place a
  // leading icon next to the nudge text.
  const gfx::VectorIcon* leading_icon = &gfx::kNoneIcon;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
