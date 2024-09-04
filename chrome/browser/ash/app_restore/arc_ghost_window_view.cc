// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"

#include "ash/components/arc/arc_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_shell_surface.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace ash::full_restore {

namespace {

constexpr char kGhostWindowTypeHistogram[] = "Arc.GhostWindowViewType";
constexpr int kAppIconSizeNewStyle = 64;
constexpr int kThrobberDiameterOriginalStyle = 24;
constexpr int kThrobberDiameterNewStyle = 24;
constexpr int kSpaceBetweenThrobberAndMessage = 12;
constexpr int kSpaceBetweenIconAndMessage = 48;

gfx::ImageSkia ResizeAndShadowedImage(const gfx::ImageSkia& image,
                                      gfx::Size size) {
  // TODO(sstan): Clear these definitions.
  // The shadow defined in ash/shelf/shelf_app_button.cc
  const std::vector<gfx::ShadowValue> kShadows = {
      gfx::ShadowValue(gfx::Vector2d(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
      gfx::ShadowValue(gfx::Vector2d(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };
  return gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      gfx::ImageSkiaOperations::CreateResizedImage(
          image, skia::ImageOperations::RESIZE_BEST, size),
      kShadows);
}

// Ghost window view type enumeration; Used for UMA counter.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GhostWindowType {
  kIconSpinning = 0,
  kIconSpinningWithFixupText = 1,
  kMaxValue = kIconSpinningWithFixupText,
};

bool IsGhostWindowNewStyleEnabled() {
  return base::FeatureList::IsEnabled(arc::kGhostWindowNewStyle);
}

std::u16string GetGhostWindowAppLaunchString(const std::string& app_name) {
  return l10n_util::GetStringFUTF16(IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_MESSAGE,
                                    base::UTF8ToUTF16(app_name));
}

std::u16string GetGhostWindowAppLaunchAodString() {
  return l10n_util::GetStringUTF16(
      IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_AOD_MESSAGE);
}

class Throbber : public views::View {
  METADATA_HEADER(Throbber, views::View)

 public:
  explicit Throbber(uint32_t color) : color_(color) {
    start_time_ = base::TimeTicks::Now();
    timer_.Start(
        FROM_HERE, base::Milliseconds(30),
        base::BindRepeating(&Throbber::SchedulePaint, base::Unretained(this)));
    SchedulePaint();  // paint right away
    GetViewAccessibility().SetRole(ax::mojom::Role::kProgressIndicator);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_THROBBER));
  }
  Throbber(const Throbber&) = delete;
  Throbber operator=(const Throbber&) = delete;
  ~Throbber() override { timer_.Stop(); }

  void OnPaint(gfx::Canvas* canvas) override {
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
    gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color_,
                               elapsed_time);
  }

 private:
  uint32_t color_;              // Throbber color.
  base::TimeTicks start_time_;  // Time when Start was called.
  base::RepeatingTimer timer_;  // Used to schedule Run calls.
};

BEGIN_METADATA(Throbber)
END_METADATA

}  // namespace

ArcGhostWindowView::ArcGhostWindowView(
    ArcGhostWindowShellSurface* shell_surface,
    const std::string& app_name)
    : app_name_(app_name),
      ghost_window_type_(arc::GhostWindowType::kAppLaunch),
      shell_surface_(shell_surface) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
}

ArcGhostWindowView::~ArcGhostWindowView() = default;

void ArcGhostWindowView::SetThemeColor(uint32_t theme_color) {
  theme_color_ = theme_color;
}

void ArcGhostWindowView::SetGhostWindowViewType(arc::GhostWindowType type) {
  ghost_window_type_ = type;
  RemoveAllChildViews();

  // DarkLightModeController maybe null in test env.
  if (type != arc::GhostWindowType::kFullRestore &&
      IsGhostWindowNewStyleEnabled() && DarkLightModeController::Get()) {
    // New style use ChromeOS system provided background color.
    auto color = cros_styles::ResolveColor(
        cros_styles::ColorName::kBgColor,
        DarkLightModeController::Get()->IsDarkModeEnabled());
    SetBackground(views::CreateSolidBackground(color));
    if (shell_surface_)
      shell_surface_->OnSetFrameColors(color, color);
  } else {
    // Use ARC app's theme color.
    SetBackground(views::CreateSolidBackground(theme_color_));
  }

  if (type == arc::GhostWindowType::kFullRestore ||
      !IsGhostWindowNewStyleEnabled()) {
    // If not enabled new style flag, all types will use original UI.
    AddChildView(views::Builder<views::ImageView>()
                     .SetImage(icon_raw_data_)
                     .SetAccessibleName(l10n_util::GetStringUTF16(
                         IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_ICON))
                     .SetID(ContentID::ID_ICON_IMAGE)
                     .Build());

    auto* throbber = AddChildView(std::make_unique<Throbber>(
        color_utils::GetColorWithMaxContrast(theme_color_)));
    throbber->SetPreferredSize(gfx::Size(kThrobberDiameterOriginalStyle,
                                         kThrobberDiameterOriginalStyle));
    throbber->GetViewAccessibility().SetRole(ax::mojom::Role::kImage);
    throbber->SetID(ContentID::ID_THROBBER);
    // TODO(sstan): Set window title and accessible name from saved data.
  } else {
    if (type == arc::GhostWindowType::kFixup) {
      AddCommonChildrenViews();
      AddChildrenViewsForFixupType();
    } else if (type == arc::GhostWindowType::kAppLaunch) {
      AddCommonChildrenViews();
      AddChildrenViewsForAppLaunchType();
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  DeprecatedLayoutImmediately();
}

void ArcGhostWindowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // DarkLightModeController maybe null in test env.
  if (!IsGhostWindowNewStyleEnabled() ||
      ghost_window_type_ == arc::GhostWindowType::kFullRestore ||
      !DarkLightModeController::Get()) {
    return;
  }
  auto color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor,
      DarkLightModeController::Get()->IsDarkModeEnabled());
  SetBackground(views::CreateSolidBackground(color));
  if (shell_surface_)
    shell_surface_->OnSetFrameColors(color, color);
}

void ArcGhostWindowView::LoadIcon(const std::string& app_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
  DCHECK(profile);

  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));

  apps::AppServiceProxyFactory::GetForProfile(profile)->LoadIcon(
      app_id, apps::IconType::kStandard,
      SharedAppListConfig::instance().default_grid_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      icon_loaded_cb_for_testing_.is_null()
          ? base::BindOnce(&ArcGhostWindowView::OnIconLoaded,
                           weak_ptr_factory_.GetWeakPtr())
          : std::move(icon_loaded_cb_for_testing_));
}

void ArcGhostWindowView::OnIconLoaded(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard)
    return;

  icon_raw_data_ = icon_value->uncompressed;
  SetGhostWindowViewType(ghost_window_type_);
}

void ArcGhostWindowView::AddCommonChildrenViews() {
  static_cast<views::BoxLayout*>(GetLayoutManager())
      ->set_between_child_spacing(kSpaceBetweenIconAndMessage);
  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(ResizeAndShadowedImage(
                       icon_raw_data_,
                       gfx::Size(kAppIconSizeNewStyle, kAppIconSizeNewStyle)))
                   .SetAccessibleName(l10n_util::GetStringUTF16(
                       IDS_ARC_GHOST_WINDOW_APP_LAUNCHING_ICON))
                   .SetID(ContentID::ID_ICON_IMAGE)
                   .Build());
}

void ArcGhostWindowView::AddChildrenViewsForFixupType() {
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetBetweenChildSpacing(kSpaceBetweenThrobberAndMessage)
          .AddChildren(
              views::Builder<views::Throbber>()
                  .SetID(ContentID::ID_THROBBER)
                  .SetPreferredSize(gfx::Size(kThrobberDiameterNewStyle,
                                              kThrobberDiameterNewStyle)),
              views::Builder<views::Label>()
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ARC_GHOST_WINDOW_APP_FIXUP_MESSAGE))
                  .SetTextStyle(views::style::STYLE_SECONDARY)
                  .SetMultiLine(true)
                  .SetID(ContentID::ID_MESSAGE_LABEL))
          .Build());

  static_cast<views::Throbber*>(GetViewByID(ContentID::ID_THROBBER))->Start();
  base::UmaHistogramEnumeration(kGhostWindowTypeHistogram,
                                GhostWindowType::kIconSpinningWithFixupText);
}

void ArcGhostWindowView::AddChildrenViewsForAppLaunchType() {
  auto app_launch_message = arc::ArcSessionManager::Get()->IsActivationDelayed()
                                ? GetGhostWindowAppLaunchAodString()
                                : GetGhostWindowAppLaunchString(app_name_);
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetBetweenChildSpacing(kSpaceBetweenThrobberAndMessage)
          .AddChildren(
              views::Builder<views::Throbber>()
                  .SetID(ContentID::ID_THROBBER)
                  .SetPreferredSize(gfx::Size(kThrobberDiameterNewStyle,
                                              kThrobberDiameterNewStyle)),
              views::Builder<views::Label>()
                  .SetText(app_launch_message)
                  .SetTextContext(views::style::CONTEXT_LABEL)
                  .SetTextStyle(views::style::STYLE_SECONDARY)
                  .SetMultiLine(true)
                  .SetID(ContentID::ID_MESSAGE_LABEL))
          .Build());

  static_cast<views::Throbber*>(GetViewByID(ContentID::ID_THROBBER))->Start();
  base::UmaHistogramEnumeration(kGhostWindowTypeHistogram,
                                GhostWindowType::kIconSpinning);
}
BEGIN_METADATA(ArcGhostWindowView)
END_METADATA

}  // namespace ash::full_restore
