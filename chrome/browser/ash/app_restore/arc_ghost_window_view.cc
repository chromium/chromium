// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace {

class Throbber : public views::View {
 public:
  explicit Throbber(uint32_t color) : color_(color) {
    start_time_ = base::TimeTicks::Now();
    timer_.Start(
        FROM_HERE, base::Milliseconds(30),
        base::BindRepeating(&Throbber::SchedulePaint, base::Unretained(this)));
    SchedulePaint();  // paint right away
  }
  Throbber(const Throbber&) = delete;
  Throbber operator=(const Throbber&) = delete;
  ~Throbber() override { timer_.Stop(); }

  void OnPaint(gfx::Canvas* canvas) override {
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
    gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color_,
                               elapsed_time);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // A valid role must be set prior to setting the name.
    node_data->role = ax::mojom::Role::kProgressIndicator;
    node_data->SetName(
        l10n_util::GetStringUTF16(IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_THROBBER));
  }

 private:
  uint32_t color_;              // Throbber color.
  base::TimeTicks start_time_;  // Time when Start was called.
  base::RepeatingTimer timer_;  // Used to schedule Run calls.
};

}  // namespace

namespace ash::full_restore {

ArcGhostWindowView::ArcGhostWindowView(int throbber_diameter,
                                       uint32_t theme_color) {
  InitLayout(theme_color, throbber_diameter);
}

ArcGhostWindowView::~ArcGhostWindowView() = default;

void ArcGhostWindowView::InitLayout(uint32_t theme_color, int diameter) {
  SetBackground(views::CreateSolidBackground(theme_color));
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());

  auto* throbber = AddChildView(std::make_unique<Throbber>(
      color_utils::GetColorWithMaxContrast(theme_color)));
  throbber->SetPreferredSize(gfx::Size(diameter, diameter));
  throbber->GetViewAccessibility().OverrideRole(ax::mojom::Role::kImage);

  // TODO(sstan): Set window title and accessible name from saved data.
}

void ArcGhostWindowView::LoadIcon(const std::string& app_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  DCHECK(profile);

  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
      apps::AppType::kArc, app_id, apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      icon_loaded_cb_for_testing_.is_null()
          ? base::BindOnce(&ArcGhostWindowView::OnIconLoaded,
                           weak_ptr_factory_.GetWeakPtr())
          : std::move(icon_loaded_cb_for_testing_));
}

void ArcGhostWindowView::OnIconLoaded(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  icon_view_->SetImage(icon_value->uncompressed);
  icon_view_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_ICON));
}

BEGIN_METADATA(ArcGhostWindowView, views::View)
END_METADATA

}  // namespace ash::full_restore
