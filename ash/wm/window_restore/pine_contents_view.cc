// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "base/barrier_callback.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// TODO(http://b/322359738): Localize all these strings.
// TODO(http://b/322360273): Match specs.
// TODO(http://b/322360373): Replace all hardcoded colors with tokens.
// TODO(hewer): Update `SetFontList()` to use
// `ash::TypographyProvider`.

constexpr int kMaxItems = 4;

// Constants for `PineItemView`.
constexpr gfx::Size kFaviconPreferredSize(16, 16);
constexpr int kItemChildSpacing = 10;
constexpr gfx::Size kItemIconBackgroundPreferredSize(40, 40);
constexpr int kItemIconBackgroundRounding = 10;
constexpr gfx::Size kItemIconPreferredSize(32, 32);
constexpr int kItemTitleFontSize = 16;

// Constants for `PineItemsOverflowView`.
constexpr int kOverflowMinElements = kMaxItems + 1;
constexpr int kOverflowMinThreshold = kMaxItems - 1;
constexpr int kOverflowMaxElements = 7;
constexpr int kOverflowMaxThreshold = kOverflowMaxElements - 1;
constexpr int kOverflowTriangleElements = 6;
constexpr int kOverflowIconSpacing = 2;
constexpr int kOverflowBackgroundRounding = 20;
constexpr int kOverflowCountBackgroundRounding = 9;
constexpr gfx::Size kOverflowIconPreferredSize(20, 20);
constexpr gfx::Size kOverflowCountPreferredSize(18, 18);

// Constants for `PineItemsContainerView`.
constexpr int kAppIdImageSize = 64;
constexpr int kItemsContainerChildSpacing = 10;
constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);
constexpr int kItemsContainerRounding = 15;
constexpr gfx::Size kItemsContainerPreferredSize(
    320,
    kItemsContainerInsets.height() +
        kItemIconBackgroundPreferredSize.height() * kMaxItems +
        kItemsContainerChildSpacing * (kMaxItems - 1));

// Constants for `PineContentsView`.
constexpr int kButtonContainerChildSpacing = 10;
constexpr int kContentsChildSpacing = 20;
constexpr gfx::Insets kContentsInsets = gfx::Insets::VH(15, 15);
constexpr int kContentsRounding = 20;
constexpr int kContentsTitleFontSize = 22;
constexpr int kContentsDescriptionFontSize = 14;
constexpr int kLeftContentsChildSpacing = 20;
constexpr int kSettingsIconSize = 24;

// Represents an app that will be shown in the pine widget. Contains the app
// title and app icon. Optionally contains a couple favicons depending on the
// app.
// TODO(sammiequon): Add ASCII art.
class PineItemView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemView, views::BoxLayoutView)

 public:
  PineItemView(const std::string& app_title,
               const std::vector<std::string>& favicons) {
    SetBetweenChildSpacing(kItemChildSpacing);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);

    AddChildView(views::Builder<views::ImageView>()
                     .CopyAddressTo(&image_view_)
                     .SetBackground(views::CreateRoundedRectBackground(
                         SK_ColorLTGRAY, kItemIconBackgroundRounding))
                     .SetImageSize(kItemIconPreferredSize)
                     .SetPreferredSize(kItemIconBackgroundPreferredSize)
                     .Build());

    views::Label* app_title_label;
    AddChildView(views::Builder<views::Label>()
                     .CopyAddressTo(&app_title_label)
                     .SetEnabledColor(SK_ColorBLACK)
                     .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                                kItemTitleFontSize,
                                                gfx::Font::Weight::BOLD))
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .SetText(base::ASCIIToUTF16(app_title))
                     .Build());
    SetFlexForView(app_title_label, 1);

    if (favicons.empty()) {
      return;
    }

    // Use a barrier callback so that we only layout once after all favicons are
    // added as views.
    auto barrier = base::BarrierCallback<const gfx::ImageSkia&>(
        /*num_callbacks=*/favicons.size(),
        /*done_callback=*/base::BindOnce(&PineItemView::OnAllFaviconsLoaded,
                                         weak_ptr_factory_.GetWeakPtr()));

    auto* delegate = Shell::Get()->saved_desk_delegate();
    for (const std::string& url : favicons) {
      // TODO(b/325638530): When lacros is active, this needs to supply a valid
      // profile id.
      delegate->GetFaviconForUrl(
          url, /*lacros_profile_id=*/0,
          base::BindOnce(&PineItemView::OnOneFaviconLoaded, GetWeakPtr(),
                         barrier),
          &cancelable_favicon_task_tracker_);
    }
  }

  PineItemView(const PineItemView&) = delete;
  PineItemView& operator=(const PineItemView&) = delete;
  ~PineItemView() override = default;

  views::ImageView* image_view() { return image_view_; }

  base::WeakPtr<PineItemView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnOneFaviconLoaded(
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      const gfx::ImageSkia& favicon) {
    std::move(callback).Run(favicon);
  }

  void OnAllFaviconsLoaded(const std::vector<gfx::ImageSkia>& favicons) {
    bool needs_layout = false;
    for (const gfx::ImageSkia& favicon : favicons) {
      if (favicon.isNull()) {
        continue;
      }

      needs_layout = true;
      AddChildView(views::Builder<views::ImageView>()
                       // TODO: The border is temporary for more contrast until
                       // specs are ready.
                       .SetBorder(views::CreateRoundedRectBorder(
                           /*thickness=*/1,
                           /*corner_radius=*/kFaviconPreferredSize.width(),
                           SK_ColorBLACK))
                       .SetImageSize(kFaviconPreferredSize)
                       .SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
                           favicon, skia::ImageOperations::RESIZE_BEST,
                           kFaviconPreferredSize))
                       .Build());
    }

    // If at least one favicon was added, relayout.
    if (needs_layout) {
      DeprecatedLayoutImmediately();
    }
  }

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> image_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<PineItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PineItemView)
END_METADATA

// An alternative to `PineItemView` when there are more than four windows in
// `apps` and the remaining information needs to be condensed.
class PineItemsOverflowView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemsOverflowView, views::BoxLayoutView)

 public:
  explicit PineItemsOverflowView(
      const PineContentsData::AppsInfos& apps_infos) {
    const int elements = static_cast<int>(apps_infos.size());
    CHECK_GE(elements, kOverflowMinElements);

    // TODO(hewer): Fix margins so the icons and text are aligned with
    // `PineItemView` elements.
    SetBetweenChildSpacing(kItemChildSpacing);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);

    // TODO(sammiequon): Handle case where the app is not ready or installed.
    auto* delegate = Shell::Get()->saved_desk_delegate();

    // Save the views so they can be used later (e.g., for callbacks).
    views::BoxLayoutView* top_row_view;
    views::BoxLayoutView* bottom_row_view;

    // Create a series of `BoxLayoutView`s to represent a 1x2 row, a triangle
    // with one element on top and two on the bottom, or a 2x2 box. The triangle
    // is specific to the 3-window overflow case, and is why we prefer a
    // `BoxLayout` over a `TableLayout` to keep things uniform.
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetCrossAxisAlignment(
                views::BoxLayout::CrossAxisAlignment::kCenter)
            .SetBetweenChildSpacing(kOverflowIconSpacing)
            .SetBackground(views::CreateRoundedRectBackground(
                SK_ColorLTGRAY, kOverflowBackgroundRounding))
            .AddChildren(
                views::Builder<views::BoxLayoutView>()
                    .CopyAddressTo(&top_row_view)
                    .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                    .SetMainAxisAlignment(
                        views::BoxLayout::MainAxisAlignment::kCenter)
                    .SetCrossAxisAlignment(
                        views::BoxLayout::CrossAxisAlignment::kStretch)
                    .SetBetweenChildSpacing(kOverflowIconSpacing),
                views::Builder<views::BoxLayoutView>()
                    .CopyAddressTo(&bottom_row_view)
                    .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                    .SetMainAxisAlignment(
                        views::BoxLayout::MainAxisAlignment::kCenter)
                    .SetCrossAxisAlignment(
                        views::BoxLayout::CrossAxisAlignment::kStretch)
                    .SetBetweenChildSpacing(kOverflowIconSpacing))
            .Build());

    // Populate the `BoxLayoutView`s with window icons or a count of any excess
    // windows.
    for (int i = kOverflowMinThreshold; i < elements; ++i) {
      // If there are 5 or more overflow windows, save the last spot in the
      // bottom row to count the remaining windows.
      if (elements > kOverflowMaxElements && i >= kOverflowMaxThreshold) {
        views::Label* count_label;
        bottom_row_view->AddChildView(
            views::Builder<views::Label>()
                .CopyAddressTo(&count_label)
                // TODO(hewer): Cut off the maximum number of digits to
                // display.
                .SetText(base::FormatNumber(elements - kOverflowMaxThreshold))
                .SetPreferredSize(kOverflowCountPreferredSize)
                .SetEnabledColor(cros_tokens::kCrosSysOnPrimaryContainer)
                .SetBackground(views::CreateThemedRoundedRectBackground(
                    cros_tokens::kCrosSysPrimaryContainer,
                    kOverflowCountBackgroundRounding))
                .Build());
        TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                              *count_label);
        break;
      }

      // Add the image view to the correct row based on the total number of
      // elements and the current index.
      views::BoxLayoutView* row_view =
          // If there are 6 elements (3 overflow elements), we will want to
          // display the overflow elements in a triangle. Thus, we will only add
          // the first element (i == 3) to the top row.
          (elements == kOverflowTriangleElements &&
           i == kOverflowMinThreshold) ||
                  // Otherwise, we can add the first two elements (i == 3 || i
                  // == 4) to the top row, as the view will be in a 1x2 or 2x2
                  // configuration.
                  (elements != kOverflowTriangleElements &&
                   i < kOverflowMinElements)
              ? top_row_view
              : bottom_row_view;
      views::ImageView* image_view = row_view->AddChildView(
          views::Builder<views::ImageView>()
              .SetImageSize(kOverflowIconPreferredSize)
              .SetPreferredSize(kOverflowIconPreferredSize)
              .Build());

      // Insert `image_view` into a map so it can be retrieved in a callback.
      image_view_map_[i] = image_view;

      // The callback may be called synchronously.
      const PineContentsData::AppInfo& app_info = apps_infos[i];
      delegate->GetIconForAppId(
          app_info.app_id, kAppIdImageSize,
          base::BindOnce(&PineItemsOverflowView::SetIconForIndex,
                         weak_ptr_factory_.GetWeakPtr(), i));
    }

    // Add a text label displaying the count of the remaining windows.
    views::Label* remaining_windows_label;
    AddChildView(views::Builder<views::Label>()
                     .CopyAddressTo(&remaining_windows_label)
                     .SetEnabledColor(SK_ColorBLACK)
                     .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                                kItemTitleFontSize,
                                                gfx::Font::Weight::BOLD))
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .SetText(l10n_util::GetPluralStringFUTF16(
                         IDS_ASH_FOREST_WINDOW_OVERFLOW_COUNT,
                         elements - kOverflowMinThreshold))
                     .Build());
    SetFlexForView(remaining_windows_label, 1);
  }

  PineItemsOverflowView(const PineItemsOverflowView&) = delete;
  PineItemsOverflowView& operator=(const PineItemsOverflowView&) = delete;
  ~PineItemsOverflowView() override = default;

  void SetIconForIndex(int index, const gfx::ImageSkia& icon) {
    views::ImageView* image_view = image_view_map_[index];
    CHECK(image_view);
    image_view->SetImage(ui::ImageModel::FromImageSkia(icon));
  }

 private:
  base::flat_map<int, views::ImageView*> image_view_map_;
  base::WeakPtrFactory<PineItemsOverflowView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PineItemsOverflowView)
END_METADATA

// The right side contents (in LTR) of the `PineContentsView`. It is a
// vertical list of `PineItemView`, with each view representing an app. Shows
// a maximum of `kMaxItems` items.
class PineItemsContainerView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemsContainerView, views::BoxLayoutView)

 public:
  explicit PineItemsContainerView(
      const PineContentsData::AppsInfos& apps_infos) {
    const int elements = static_cast<int>(apps_infos.size());
    CHECK_GT(elements, 0);

    SetBackground(views::CreateRoundedRectBackground(SK_ColorWHITE,
                                                     kItemsContainerRounding));
    SetBetweenChildSpacing(kItemsContainerChildSpacing);
    SetInsideBorderInsets(kItemsContainerInsets);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetOrientation(views::BoxLayout::Orientation::kVertical);

    // TODO(sammiequon): Handle case where the app is not ready or installed.
    apps::AppRegistryCache* cache =
        apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
            Shell::Get()->session_controller()->GetActiveAccountId());
    auto* delegate = Shell::Get()->saved_desk_delegate();

    for (int i = 0; i < elements; ++i) {
      const PineContentsData::AppInfo& app_info = apps_infos[i];
      // If there are more than four elements, we will need to save the last
      // space for the overflow view to condense the remaining info.
      if (elements >= kOverflowMinElements && i >= kOverflowMinThreshold) {
        AddChildView(std::make_unique<PineItemsOverflowView>(apps_infos));
        break;
      }

      std::string title;
      // `cache` might be null in a test environment. In that case, we will
      // use an empty title.
      if (cache) {
        cache->ForOneApp(
            app_info.app_id,
            [&title](const apps::AppUpdate& update) { title = update.Name(); });
      }

      // TODO(hewer|sammiequon): `PineItemView` should just take `app_info` and
      // `cache` as a constructor argument.
      PineItemView* item_view = AddChildView(
          std::make_unique<PineItemView>(title, app_info.tab_urls));

      // The callback may be called synchronously.
      delegate->GetIconForAppId(
          app_info.app_id, kAppIdImageSize,
          base::BindOnce(
              [](base::WeakPtr<PineItemView> item_view_ptr,
                 const gfx::ImageSkia& icon) {
                if (item_view_ptr) {
                  item_view_ptr->image_view()->SetImage(
                      ui::ImageModel::FromImageSkia(icon));
                }
              },
              item_view->GetWeakPtr()));
    }
  }
  PineItemsContainerView(const PineItemsContainerView&) = delete;
  PineItemsContainerView& operator=(const PineItemsContainerView&) = delete;
  ~PineItemsContainerView() override = default;
};

BEGIN_METADATA(PineItemsContainerView)
END_METADATA

}  // namespace

PineContentsView::PineContentsView() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, kContentsRounding));
  SetBetweenChildSpacing(kContentsChildSpacing);
  SetInsideBorderInsets(kContentsInsets);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  views::View* spacer;
  AddChildView(
      // This box layout view is the container for the left hand side (in LTR)
      // of the contents view. It contains the title, buttons container and
      // settings button.
      views::Builder<views::BoxLayoutView>()
          .SetBetweenChildSpacing(kLeftContentsChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetPreferredSize(kItemsContainerPreferredSize)
          .AddChildren(
              // Title.
              views::Builder<views::Label>()
                  .SetEnabledColor(SK_ColorBLACK)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                             kContentsTitleFontSize,
                                             gfx::Font::Weight::BOLD))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetText(u"Welcome Back"),
              // Description.
              views::Builder<views::Label>()
                  .SetEnabledColor(SK_ColorBLACK)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                             kContentsDescriptionFontSize,
                                             gfx::Font::Weight::NORMAL))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetText(u"Continue from where you left off?"),
              // This box layout view is the container for the "No thanks" and
              // "Restore" pill buttons.
              views::Builder<views::BoxLayoutView>()
                  .SetBetweenChildSpacing(kButtonContainerChildSpacing)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .AddChildren(
                      views::Builder<PillButton>()
                          .CopyAddressTo(&restore_button_for_testing_)
                          .SetCallback(base::BindRepeating(
                              &PineContentsView::OnRestoreButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()))
                          .SetPillButtonType(
                              PillButton::Type::kPrimaryWithoutIcon)
                          .SetText(u"Restore"),
                      views::Builder<PillButton>()
                          .SetCallback(base::BindRepeating(
                              &PineContentsView::OnCancelButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()))
                          .SetPillButtonType(
                              PillButton::Type::kSecondaryWithoutIcon)
                          .SetText(u"No thanks")),
              views::Builder<views::View>().CopyAddressTo(&spacer),
              views::Builder<views::ImageButton>(
                  views::CreateVectorImageButtonWithNativeTheme(
                      base::BindRepeating(
                          &PineContentsView::OnSettingsButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
                      kSettingsIcon, kSettingsIconSize))
                  .CopyAddressTo(&settings_button_)
                  .SetBackground(views::CreateRoundedRectBackground(
                      SK_ColorWHITE, kSettingsIconSize))
                  .SetTooltipText(u"Settings"))
          .Build());

  views::AsViewClass<views::BoxLayoutView>(spacer->parent())
      ->SetFlexForView(spacer, 1);

  const PineContentsData* pine_contents_data =
      Shell::Get()->pine_controller()->pine_contents_data();
  CHECK(pine_contents_data);
  if (pine_contents_data->image.isNull()) {
    PineItemsContainerView* container_view =
        AddChildView(std::make_unique<PineItemsContainerView>(
            pine_contents_data->apps_infos));
    container_view->SetPreferredSize(kItemsContainerPreferredSize);
  } else {
    views::ImageView* preview =
        AddChildView(std::make_unique<views::ImageView>());
    preview->SetImage(pine_contents_data->image);
    // TODO(minch): Make this respect the aspect ratio of the screenshot.
    preview->SetImageSize(kItemsContainerPreferredSize);
  }
}

PineContentsView::~PineContentsView() = default;

// static
std::unique_ptr<views::Widget> PineContentsView::Create(aura::Window* root) {
  auto contents_view = std::make_unique<PineContentsView>();
  gfx::Rect contents_bounds = root->GetBoundsInScreen();
  contents_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());

  views::Widget::InitParams params;
  params.bounds = contents_bounds;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);
  params.name = "PineWidget";
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = desks_util::GetActiveDeskContainerForRoot(root);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetLayer()->SetFillsBoundsOpaquely(false);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

void PineContentsView::OnRestoreButtonPressed() {
  if (PineContentsData* pine_contents_data =
          Shell::Get()->pine_controller()->pine_contents_data()) {
    if (pine_contents_data->restore_callback) {
      // Destroys `this`.
      std::move(pine_contents_data->restore_callback).Run();
    }
  }
}

void PineContentsView::OnCancelButtonPressed() {
  if (PineContentsData* pine_contents_data =
          Shell::Get()->pine_controller()->pine_contents_data()) {
    if (pine_contents_data->cancel_callback) {
      // Destroys `this`.
      std::move(pine_contents_data->cancel_callback).Run();
    }
  }
}

void PineContentsView::OnSettingsButtonPressed() {
  context_menu_model_ = std::make_unique<PineContextMenuModel>();
  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      context_menu_model_.get(),
      base::BindRepeating(&PineContentsView::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));

  std::unique_ptr<views::MenuItemView> root_menu_item =
      menu_model_adapter_->CreateMenu();
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  menu_runner_ =
      std::make_unique<views::MenuRunner>(std::move(root_menu_item), run_types);
  menu_runner_->RunMenuAt(
      settings_button_->GetWidget(), /*button_controller=*/nullptr,
      settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kBubbleRight, ui::MENU_SOURCE_NONE);
}

void PineContentsView::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  context_menu_model_.reset();
}

BEGIN_METADATA(PineContentsView)
END_METADATA

}  // namespace ash
