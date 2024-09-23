// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/update/extended_updates_notice_quick_settings_view.h"
#include "ash/system/update/quick_settings_notice_view_pixeltest_base.h"

namespace ash {
namespace {

using ExtendedUpdatesNoticeQuickSettingsViewPixelTest =
    QuickSettingsNoticeViewPixelTestBase;

}  // namespace

TEST_F(ExtendedUpdatesNoticeQuickSettingsViewPixelTest, Basics) {
  AddChildView(std::make_unique<ExtendedUpdatesNoticeQuickSettingsView>());
  DiffView(/*revision_number=*/1);
}

}  // namespace ash
