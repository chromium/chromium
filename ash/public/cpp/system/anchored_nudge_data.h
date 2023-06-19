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

// Describes the contents of a System Nudge (AnchoredNudge), which is a notifier
// that may anchor to an `anchor_view` and informs users about something that
// might enhance their experience immediately. See the "Educational Nudges"
// section in go/notifier-framework for example usages.
// TODO(b/285988235): `AnchoredNudge` will replace the existing `SystemNudge`
// and take over its name.
struct ASH_PUBLIC_EXPORT AnchoredNudgeData {
  AnchoredNudgeData(const std::string& id,
                    NudgeCatalogName catalog_name,
                    const std::u16string& body_text,
                    views::View* anchor_view);
  AnchoredNudgeData(AnchoredNudgeData&& other);
  AnchoredNudgeData& operator=(AnchoredNudgeData&& other);
  ~AnchoredNudgeData();

  // Required system nudge elements.
  std::string id;
  NudgeCatalogName catalog_name;
  std::u16string body_text;

  // Optional system nudge view elements. If not empty, a leading image or nudge
  // title will be created.
  ui::ImageModel image_model;
  std::u16string title_text;

  // Optional system nudge buttons. If the text is not empty, the respective
  // button will be created. Pressing the button will execute its callback, if
  // any, followed by the nudge being closed. `second_button_text` should only
  // be set if `dismiss_text` has also been set.
  // TODO(b/285023559): Add a `ChainedCancelCallback` class instead of a
  // `RepeatingClosure` so we don't have to manually modify the provided
  // callbacks in the manager.
  std::u16string dismiss_text;
  base::RepeatingClosure dismiss_callback;

  std::u16string second_button_text;
  base::RepeatingClosure second_button_callback;

  // Unowned. Must outlive the `AnchoredNudge`.
  // TODO(b/285988197): Make setting an `anchor_view` optional. Nudges without
  // an anchor will show on the leading bottom of the screen.
  raw_ptr<views::View, DanglingUntriaged> anchor_view;

  // Used to set bubble placement in relation to the anchor view.
  // A value of `BOTTOM_CENTER` means that the nudge will be anchored from its
  // bottom center to the anchor view.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::BOTTOM_CENTER;

  // To disable dismiss via timer, set `has_infinite_duration_` to true.
  // A nudge with infinite duration will be displayed until the dismiss button
  // on the nudge is clicked, or when it is destroyed due to other reasons (e.g.
  // anchor view is deleted, user locks session, etc.)
  bool has_infinite_duration = false;

  // Nudge action callbacks.
  HoverStateChangeCallback hover_state_change_callback;
  AnchoredNudgeClickCallback nudge_click_callback;
  AnchoredNudgeDismissCallback nudge_dimiss_callback;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_ANCHORED_NUDGE_DATA_H_
