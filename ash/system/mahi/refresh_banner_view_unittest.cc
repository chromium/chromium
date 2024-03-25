// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/refresh_banner_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using RefreshBannerViewTest = views::ViewsTestBase;

TEST_F(RefreshBannerViewTest, ShowsCorrectTitle) {
  FakeMahiManager manager;
  chromeos::ScopedMahiManagerSetter manager_setter(&manager);
  RefreshBannerView banner_view;

  const std::u16string kContentTitle = u"New content";
  manager.set_content_title(kContentTitle);
  banner_view.Show();

  EXPECT_EQ(
      views::AsViewClass<views::Label>(
          banner_view.GetViewByID(mahi_constants::ViewId::kBannerTitleLabel))
          ->GetText(),
      l10n_util::GetStringFUTF16(IDS_ASH_MAHI_REFRESH_BANNER_LABEL_TEXT,
                                 kContentTitle));
}

}  // namespace
}  // namespace ash
