// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/anchored_nudge_data.h"

#include <algorithm>
#include <utility>

#include "ash/strings/grit/ash_strings.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

AnchoredNudgeData::AnchoredNudgeData(const std::string& id,
                                     NudgeCatalogName catalog_name,
                                     const std::u16string& body_text,
                                     views::View* anchor_view)
    : id(std::move(id)),
      catalog_name(catalog_name),
      body_text(body_text),
      anchor_view_tracker_(std::make_unique<views::ViewTracker>()) {
  SetAnchorView(anchor_view);
}

AnchoredNudgeData::AnchoredNudgeData(AnchoredNudgeData&& other) = default;

AnchoredNudgeData& AnchoredNudgeData::operator=(AnchoredNudgeData&& other) =
    default;

AnchoredNudgeData::~AnchoredNudgeData() = default;

void AnchoredNudgeData::SetAnchorView(views::View* anchor_view) {
  anchor_view_tracker_->SetView(anchor_view);
  is_anchored_ = anchor_view != nullptr;
}

}  // namespace ash
