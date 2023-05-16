// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/anchored_nudge_data.h"

#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// TODO(b/280499122): Simplify by using ActionButtonParams struct.
std::u16string GetDismissText(const std::u16string& custom_dismiss_text,
                              bool has_dismiss_button) {
  if (!has_dismiss_button) {
    return {};
  }

  return !custom_dismiss_text.empty()
             ? custom_dismiss_text
             : l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON);
}

}  // namespace

AnchoredNudgeData::AnchoredNudgeData(const std::string& id,
                                     AnchoredNudgeCatalogName catalog_name,
                                     const std::u16string& text,
                                     views::View* anchor_view,
                                     // TODO(b/280499122): Condense "dismiss"
                                     // vars into ActionButtonParams struct.
                                     bool has_dismiss_button,
                                     const std::u16string& custom_dismiss_text,
                                     base::RepeatingClosure dismiss_callback,
                                     const gfx::VectorIcon& leading_icon)
    : id(std::move(id)),
      catalog_name(catalog_name),
      text(text),
      anchor_view(anchor_view),
      dismiss_text(GetDismissText(custom_dismiss_text, has_dismiss_button)),
      dismiss_callback(std::move(dismiss_callback)),
      leading_icon(&leading_icon) {}

AnchoredNudgeData::AnchoredNudgeData(AnchoredNudgeData&& other) = default;

AnchoredNudgeData& AnchoredNudgeData::operator=(AnchoredNudgeData&& other) =
    default;

AnchoredNudgeData::~AnchoredNudgeData() = default;

}  // namespace ash
