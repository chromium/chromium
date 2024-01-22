// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "base/barrier_callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "pine_contents_view.h"
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
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// TODO(sammiequon|zxdan): Localize all these strings.
// TODO(sammiequon|zxdan): Match specs.
// TODO(sammiequon|zxdan): Replace all hardcoded colors with tokens.

constexpr int kMaxItems = 4;

// Constants for `PineItemView`.
constexpr gfx::Size kFaviconPreferredSize(16, 16);
constexpr int kItemChildSpacing = 10;
constexpr gfx::Size kItemIconBackgroundPreferredSize(40, 40);
constexpr int kItemIconBackgroundRounding = 10;
constexpr gfx::Size kItemIconPreferredSize(32, 32);
constexpr int kItemTitleFontSize = 16;

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
 public:
  METADATA_HEADER(PineItemView);

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
      delegate->GetFaviconForUrl(
          url,
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
      Layout();
    }
  }

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> image_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<PineItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PineItemView, views::BoxLayoutView)
END_METADATA

// The right side contents (in LTR) of the `PineContentsView`. It is a vertical
// list of `PineItemView`, with each view representing an app. Shows a maximum
// of `kMaxItems` items.
class PineItemsContainerView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(PineItemsContainerView);

  explicit PineItemsContainerView(const PineContentsView::AppsData& apps) {
    const int elements = static_cast<int>(apps.size());
    CHECK_GT(elements, 0);
    CHECK_LE(elements, kMaxItems);

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
    for (const auto& [app_id, favicons] : apps) {
      std::string title;
      // `cache` might be null in a test environment. In that case, we will use
      // an empty title.
      if (cache) {
        cache->ForOneApp(app_id, [&title](const apps::AppUpdate& update) {
          title = update.Name();
        });
      }

      PineItemView* item_view =
          AddChildView(std::make_unique<PineItemView>(title, favicons));

      // The callback may be called synchronously.
      delegate->GetIconForAppId(
          app_id, kAppIdImageSize,
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

BEGIN_METADATA(PineItemsContainerView, views::BoxLayoutView)
END_METADATA

}  // namespace

PineContentsView::PineContentsView(const gfx::ImageSkia& pine_image) {
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
                          .SetText(u"Restore")
                          .SetPillButtonType(
                              PillButton::Type::kPrimaryWithoutIcon),
                      views::Builder<PillButton>()
                          .SetText(u"No thanks")
                          .SetPillButtonType(
                              PillButton::Type::kSecondaryWithoutIcon)),
              views::Builder<views::View>().CopyAddressTo(&spacer),
              views::Builder<views::ImageButton>(
                  views::CreateVectorImageButtonWithNativeTheme(
                      base::BindRepeating(
                          &PineContentsView::OnSettingsButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
                      kSettingsIcon, kSettingsIconSize))
                  .CopyAddressTo(&settings_button_view_)
                  .SetBackground(views::CreateRoundedRectBackground(
                      SK_ColorWHITE, kSettingsIconSize))
                  .SetTooltipText(u"Settings"))
          .Build());

  views::AsViewClass<views::BoxLayoutView>(spacer->parent())
      ->SetFlexForView(spacer, 1);

  if (pine_image.isNull()) {
    // TODO(sammiequon|zxdan): Remove this temporary data used for testing.
    AppsData kTestingAppsData = {
        {"mgndgikekgjfcpckkfioiadnlibdjbkf",  // Chrome
         {"https://www.cnn.com/", "https://www.youtube.com/",
          "https://www.google.com/"}},
        {"njfbnohfdkmbmnjapinfcopialeghnmh", {}},  // Camera
        {"odknhmnlageboeamepcngndbggdpaobj", {}},  // Settings
        {"fkiggjmkendpmbegkagpmagjepfkpmeb", {}},  // Files
    };
    PineItemsContainerView* container_view = AddChildView(
        std::make_unique<PineItemsContainerView>(kTestingAppsData));
    container_view->SetPreferredSize(kItemsContainerPreferredSize);
  } else {
    views::ImageView* preview =
        AddChildView(std::make_unique<views::ImageView>());
    preview->SetImage(pine_image);
    // TODO(minch): Make this respect the aspect ratio of the screenshot.
    preview->SetImageSize(kItemsContainerPreferredSize);
  }
}

PineContentsView::~PineContentsView() = default;

// static
std::unique_ptr<views::Widget> PineContentsView::Create(
    aura::Window* root,
    const gfx::ImageSkia& pine_image) {
  auto contents_view = std::make_unique<PineContentsView>(pine_image);
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
      settings_button_view_->GetWidget(), /*button_controller=*/nullptr,
      settings_button_view_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kBubbleRight, ui::MENU_SOURCE_NONE);
}

void PineContentsView::OnMenuClosed() {
  context_menu_model_.reset();
  menu_model_adapter_.reset();
  menu_runner_.reset();
}

BEGIN_METADATA(PineContentsView, views::BoxLayoutView)
END_METADATA

}  // namespace ash
