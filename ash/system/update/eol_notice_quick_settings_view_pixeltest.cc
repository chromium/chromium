// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "ash/system/update/quick_settings_notice_view_pixeltest_base.h"

namespace ash {
namespace {

using EolNoticeQuickSettingsViewPixelTest =
    QuickSettingsNoticeViewPixelTestBase;

}  // namespace

TEST_F(EolNoticeQuickSettingsViewPixelTest, Basics) {
  AddChildView(std::make_unique<EolNoticeQuickSettingsView>());
  DiffView(/*revision_number=*/0);
}

}  // namespace ash
