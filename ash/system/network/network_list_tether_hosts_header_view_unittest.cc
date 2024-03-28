// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_tether_hosts_header_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_list_header_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class NetworkListTetherHostsHeaderViewTest : public AshTestBase {
 public:
  ~NetworkListTetherHostsHeaderViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(features::kInstantHotspotRebrand);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    std::unique_ptr<NetworkListTetherHostsHeaderView>
        network_list_tether_hosts_header_view =
            std::make_unique<NetworkListTetherHostsHeaderView>(
                base::BindRepeating(
                    &NetworkListTetherHostsHeaderViewTest::OnHeaderClicked,
                    weak_ptr_factory_.GetWeakPtr()));

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    network_list_tether_hosts_header_view_ = widget_->SetContentsView(
        std::move(network_list_tether_hosts_header_view));

    // Wait for network state and device change events to be handled.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  HoverHighlightView* GetEntryRow() {
    return FindViewById<HoverHighlightView*>(static_cast<int>(
        NetworkListTetherHostsHeaderView::
            NetworkListTetherHostsHeaderViewChildId::kEntryRow));
  }

  bool ChevronHasIcon(const gfx::VectorIcon& icon) {
    auto* chevron_view = FindViewById<views::ImageView*>(static_cast<int>(
        NetworkListTetherHostsHeaderView::
            NetworkListTetherHostsHeaderViewChildId::kChevron));
    const SkBitmap* actual_bitmap = chevron_view->GetImage().bitmap();

    auto expected_icon =
        ui::ImageModel::FromVectorIcon(icon, cros_tokens::kCrosSysOnSurface);

    const SkBitmap* expected_bitmap =
        expected_icon.Rasterize(chevron_view->GetColorProvider()).bitmap();

    return gfx::BitmapsAreEqual(*actual_bitmap, *expected_bitmap);
  }

  int get_expanded_callback_execution_count() {
    return expanded_callback_call_count_;
  }

  NetworkListTetherHostsHeaderView* get_header() {
    return network_list_tether_hosts_header_view_;
  }

 private:
  template <class T>
  T FindViewById(int id) {
    return static_cast<T>(
        network_list_tether_hosts_header_view_->GetViewByID(id));
  }
  void OnHeaderClicked() { expanded_callback_call_count_++; }

  size_t expanded_callback_call_count_ = 0u;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<NetworkListTetherHostsHeaderView, DanglingUntriaged>
      network_list_tether_hosts_header_view_;

  base::WeakPtrFactory<NetworkListTetherHostsHeaderViewTest> weak_ptr_factory_{
      this};
};

TEST_F(NetworkListTetherHostsHeaderViewTest,
       ClickingEntryRowTogglesExpandedValue) {
  // Expect the header starts expanded.
  EXPECT_TRUE(get_header()->is_expanded());
  EXPECT_TRUE(ChevronHasIcon(kChevronUpIcon));

  // Click once - expect it is no longer expanded.
  auto* entry_row = GetEntryRow();
  LeftClickOn(entry_row);
  EXPECT_EQ(get_expanded_callback_execution_count(), 1);
  EXPECT_FALSE(get_header()->is_expanded());
  EXPECT_TRUE(ChevronHasIcon(kChevronDownIcon));

  // Click again - expect it is expanded.
  LeftClickOn(entry_row);
  EXPECT_EQ(get_expanded_callback_execution_count(), 2);
  EXPECT_TRUE(get_header()->is_expanded());
  EXPECT_TRUE(ChevronHasIcon(kChevronUpIcon));
}

// TODO(b/298254852): check for correct label ID and icon.
// TODO(b/300155715): parameterize test suite on whether kQsRevamp is
// enabled/disabled.

}  // namespace ash
