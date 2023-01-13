// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_nudge.h"

#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// The size of the icon.
constexpr int kIconSize = 20;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

// The minimum width of the label.
constexpr int kMinLabelWidth = 200;

constexpr char kPhoneHubNudgeName[] = "PhoneHubNudge";

const gfx::VectorIcon kEmptyIcon;

}  // namespace

PhoneHubNudge::PhoneHubNudge(std::u16string nudge_content)
    : SystemNudge(kPhoneHubNudgeName,
                  NudgeCatalogName::kPhoneHub,
                  kIconSize,
                  kIconLabelSpacing,
                  kNudgePadding),
      nudge_content_(nudge_content) {}

PhoneHubNudge::~PhoneHubNudge() = default;

std::unique_ptr<SystemNudgeLabel> PhoneHubNudge::CreateLabelView() const {
  return std::make_unique<SystemNudgeLabel>(nudge_content_, kMinLabelWidth);
}

// TODO (b/264715338) Polish Nudge.
const gfx::VectorIcon& PhoneHubNudge::GetIcon() const {
  return kEmptyIcon;
}

std::u16string PhoneHubNudge::GetAccessibilityText() const {
  return nudge_content_;
}

}  // namespace ash