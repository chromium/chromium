// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/app_stream_launcher_view.h"

#include <cmath>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/app_stream_launcher_item.h"
#include "ash/system/phonehub/app_stream_launcher_list_item.h"
#include "ash/system/phonehub/app_stream_launcher_view.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Insets for the vertical scroll bar.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(1, 0, 1, 1);

constexpr auto kHeaderDefaultSpacing = gfx::Insets::VH(0, 0);

constexpr gfx::Size kDefaultAppListScrollViewSize = gfx::Size(400, 400);

// The horizontal interior margin for the apps page container - i.e. the margin
// between the apps page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 25;

// Number of columns of apps in the grid
constexpr int kColumns = 4;

constexpr int kRowHeight = 70;

constexpr auto kHeaderViewInsets = gfx::Insets::TLBR(25, 15, 15, 15);

constexpr int kAppViewWidth = 50;

constexpr int kHeaderChildrenSpacing = 20;

// The padding between different sections within the apps page. Also used for
// interior apps page container margin.
constexpr int kVerticalPaddingBetweenSections = 16;

constexpr int kAppListItemHorizontalMargin = 16;
constexpr int kAppListItemSpacing = 8;

}  // namespace

AppStreamLauncherView::AppStreamLauncherView(
    phonehub::PhoneHubManager* phone_hub_manager)
    : phone_hub_manager_(phone_hub_manager) {
  SetID(PhoneHubViewID::kAppStreamLauncherView);

  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetInteriorMargin(gfx::Insets::VH(0, 0))
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(false)
      .SetDefault(views::kMarginsKey, kHeaderDefaultSpacing)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  AddChildView(CreateHeaderView());

  auto* app_list_view = AddChildView(CreateAppListView());
  gfx::Size launcher_size;
  if (phone_hub_manager->GetAppStreamLauncherDataModel() &&
      phone_hub_manager->GetAppStreamLauncherDataModel()->launcher_height() >
          kDefaultAppListScrollViewSize.height()) {
    launcher_size = gfx::Size(
        phone_hub_manager->GetAppStreamLauncherDataModel()->launcher_width(),
        phone_hub_manager->GetAppStreamLauncherDataModel()->launcher_height());
  } else {
    launcher_size = kDefaultAppListScrollViewSize;
  }
  app_list_view->SetPreferredSize(launcher_size);
  app_list_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width =*/false)
          .WithWeight(1));

  phone_hub_manager->GetUserActionRecorder()->RecordUiOpened();

  UpdateFromDataModel();
  if (phone_hub_manager->GetAppStreamLauncherDataModel())
    phone_hub_manager->GetAppStreamLauncherDataModel()->AddObserver(this);
}

AppStreamLauncherView::~AppStreamLauncherView() {
  if (phone_hub_manager_->GetAppStreamLauncherDataModel())
    phone_hub_manager_->GetAppStreamLauncherDataModel()->RemoveObserver(this);
}

// The behavior is inspired from ash/app_list/views/app_list_bubble_apps_page.cc
std::unique_ptr<views::View> AppStreamLauncherView::CreateAppListView() {
  // The entire page scrolls.
  auto scroll_view = std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view->SetDrawOverflowIndicator(false);
  // Don't paint a background. The bubble already has one.
  scroll_view->SetBackgroundColor(std::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer.
  scroll_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Set up scroll bars.
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_view->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/false)
          .WithWeight(1));

  auto* layout =
      scroll_contents->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  if (features::IsEcheLauncherListViewEnabled()) {
    layout->SetInteriorMargin(gfx::Insets::VH(kVerticalPaddingBetweenSections,
                                              kAppListItemHorizontalMargin));
  } else {
    layout->SetInteriorMargin(gfx::Insets::VH(kVerticalPaddingBetweenSections,
                                              kHorizontalInteriorMargin));
  }

  // All apps section.
  items_container_ =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  items_container_->SetPaintToLayer();
  items_container_->layer()->SetFillsBoundsOpaquely(false);
  scroll_view->SetContents(std::move(scroll_contents));

  return scroll_view;
}

void AppStreamLauncherView::AppIconActivated(
    phonehub::Notification::AppMetadata app) {
  auto* interaction_handler_ =
      phone_hub_manager_->GetRecentAppsInteractionHandler();
  if (!interaction_handler_)
    return;
  interaction_handler_->NotifyRecentAppClicked(
      app, eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
}

void AppStreamLauncherView::UpdateFromDataModel() {
  if (!items_container_)
    return;
  items_container_->RemoveAllChildViews();
  if (!phone_hub_manager_->GetAppStreamLauncherDataModel())
    return;
  const std::vector<phonehub::Notification::AppMetadata>* apps_list =
      phone_hub_manager_->GetAppStreamLauncherDataModel()
          ->GetAppsListSortedByName();

  if (features::IsEcheLauncherListViewEnabled()) {
    CreateListView(apps_list);
  } else {
    CreateGridView(apps_list);
  }
}

std::unique_ptr<views::View> AppStreamLauncherView::CreateItemView(
    const phonehub::Notification::AppMetadata& app) {
  return std::make_unique<AppStreamLauncherItem>(
      base::BindRepeating(&AppStreamLauncherView::AppIconActivated,
                          base::Unretained(this), app),
      app);
}

std::unique_ptr<views::View> AppStreamLauncherView::CreateListItemView(
    const phonehub::Notification::AppMetadata& app) {
  return std::make_unique<AppStreamLauncherListItem>(
      base::BindRepeating(&AppStreamLauncherView::AppIconActivated,
                          base::Unretained(this), app),
      app);
}

std::unique_ptr<views::View> AppStreamLauncherView::CreateHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kHeaderViewInsets,
      kHeaderChildrenSpacing));

  header->SetBackground(views::CreateThemedSolidBackground(
      kColorAshControlBackgroundColorInactive));

  // Add arrowback button
  arrow_back_button_ = header->AddChildView(CreateButton(
      base::BindRepeating(&AppStreamLauncherView::OnArrowBackActivated,
                          weak_factory_.GetWeakPtr()),
      kEcheArrowBackIcon, IDS_APP_ACCNAME_BACK));

  views::Label* title = header->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  title->SetMultiLine(true);
  title->SetAllowCharacterBreak(true);
  title->SetProperty(views::kBoxLayoutFlexKey,
                     views::BoxLayoutFlexSpecification());
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_APP_STREAM_LAUNCHER_TITLE));

  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosHeadline1,
                                        *title);

  return header;
}

// Creates a button with the given callback, icon, and tooltip text.
// `message_id` is the resource id of the tooltip text of the icon.
std::unique_ptr<views::Button> AppStreamLauncherView::CreateButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int message_id) {
  SkColor color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  SkColor disabled_color = SkColorSetA(color, gfx::kDisabledControlAlpha);
  auto button = views::CreateVectorImageButton(std::move(callback));
  views::SetImageFromVectorIconWithColor(button.get(), icon, color,
                                         disabled_color);

  ash::StyleUtil::SetUpInkDropForButton(button.get(), gfx::Insets(),
                                        /*highlight_on_hover=*/false,
                                        /*highlight_on_focus=*/true);
  views::FocusRing::Get(button.get())
      ->SetColorId(static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing));

  button->SetTooltipText(l10n_util::GetStringUTF16(message_id));
  button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(button.get());

  return button;
}

void AppStreamLauncherView::OnArrowBackActivated() {
  phone_hub_manager_->GetAppStreamLauncherDataModel()
      ->SetShouldShowMiniLauncher(false);
}

void AppStreamLauncherView::ChildPreferredSizeChanged(View* child) {
  // Resize the bubble when the child change its size.
  PreferredSizeChanged();
}

void AppStreamLauncherView::ChildVisibilityChanged(View* child) {
  // Resize the bubble when the child change its visibility.
  PreferredSizeChanged();
}

phone_hub_metrics::Screen AppStreamLauncherView::GetScreenForMetrics() const {
  return phone_hub_metrics::Screen::kMiniLauncher;
}

void AppStreamLauncherView::OnBubbleClose() {
  RemoveAllChildViews();
}

void AppStreamLauncherView::OnAppListChanged() {
  if (!features::IsEcheSWAEnabled() || !features::IsEcheLauncherEnabled())
    return;
  UpdateFromDataModel();
}

void AppStreamLauncherView::CreateListView(
    const std::vector<phonehub::Notification::AppMetadata>* apps_list) {
  items_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(0, 0),
      kAppListItemSpacing));
  for (auto& app : *apps_list) {
    items_container_->AddChildView(CreateListItemView(app));
  }
}

void AppStreamLauncherView::CreateGridView(
    const std::vector<phonehub::Notification::AppMetadata>* apps_list) {
  auto* table_layout = items_container_->SetLayoutManager(
      std::make_unique<views::TableLayout>());
  int spacing = (kTrayMenuWidth - kHorizontalInteriorMargin * 2 -
                 kAppViewWidth * kColumns) /
                (kColumns - 1);
  for (int i = 0; i < kColumns; i++) {
    table_layout->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch, 1.0,
        views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    if (i != kColumns - 1) {
      table_layout->AddPaddingColumn(1.0, spacing);
    }
  }
  table_layout->AddRows(ceil((double)apps_list->size() / kColumns),
                        views::TableLayout::kFixedSize, kRowHeight);

  for (auto& app : *apps_list) {
    items_container_->AddChildView(CreateItemView(app));
  }
}

BEGIN_METADATA(AppStreamLauncherView)
END_METADATA

}  // namespace ash
