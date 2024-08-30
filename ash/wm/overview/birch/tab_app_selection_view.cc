// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/style/typography.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// TODO(http://b/361326120): The below are hardcoded temporary values.
constexpr int kScrollViewMaxHeight = 400;

constexpr int kItemChildSpacing = 16;
constexpr gfx::Insets kItemInsets = gfx::Insets::VH(8, 16);
constexpr int kImageSize = 20;
constexpr gfx::Size kImagePreferredSize(20, 20);

constexpr gfx::Insets kContentsInsets = gfx::Insets::VH(8, 0);

constexpr gfx::RoundedCornersF kContainerCornerRadius(20.f, 20.f, 0.f, 0.f);

constexpr gfx::Insets kSubtitleMargins = gfx::Insets::VH(8, 16);

std::unique_ptr<views::Label> CreateSubtitle(const std::u16string& text) {
  return views::Builder<views::Label>()
      .SetText(text)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
      .SetProperty(views::kMarginsKey, kSubtitleMargins)
      .CustomConfigure(base::BindOnce([](views::Label* label) {
        TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                              *label);
      }))
      .Build();
}

// Represents either a tab that will be moved into a new browser on a new desk
// or an app that will be moved to the new desk.
//
//   +-------------------------------------------+
//   |  +---+   +-----------------------+  +---+ |
//   |  |   |   |                       |  |   | |
//   |  +---+   +-----------------------+  +---+ |
//   +--^---------------^------------------^-----+
//   ^  |               |                  |
//   |  `ImageView`     |                  `CloseButton` (Visible on hover)
//   |                  `Label`
//   |
//   `TabAppSelectionItemView`
class TabAppSelectionItemView : public views::BoxLayoutView {
  METADATA_HEADER(TabAppSelectionItemView, views::BoxLayoutView)

 public:
  struct InitParams {
    InitParams() = default;
    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;
    ~InitParams() = default;

    InitParams(InitParams&& other) = default;

    enum class Type {
      kTab,
      kApp,
    } type;

    // For tabs, `identifier` is an url spec. For apps, its the app id. These
    // will use the favicon and app services to fetch the favicon and app icon.
    std::string identifier;

    using CloseCallback = base::OnceCallback<void(views::View*)>;
    CloseCallback close_callback;
  };

  explicit TabAppSelectionItemView(InitParams params) {
    views::Builder<views::BoxLayoutView>(this)
        .SetAccessibleRole(ax::mojom::Role::kMenuItem)
        .SetAccessibleName(u"TempAccessibleName")
        .SetBetweenChildSpacing(kItemChildSpacing)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetFocusBehavior(views::View::FocusBehavior::ALWAYS)
        .SetInsideBorderInsets(kItemInsets)
        .SetNotifyEnterExitOnChild(true)
        .SetOrientation(views::LayoutOrientation::kHorizontal)
        .AddChildren(
            views::Builder<views::ImageView>()
                .CopyAddressTo(&image_)
                .SetImage(ui::ImageModel::FromVectorIcon(
                    kDefaultAppIcon, cros_tokens::kCrosSysOnPrimary))
                .SetImageSize(gfx::Size(kImageSize, kImageSize))
                .SetPreferredSize(kImagePreferredSize),
            views::Builder<views::Label>()
                .SetText(u"Title")
                .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                .SetProperty(views::kBoxLayoutFlexKey,
                             views::BoxLayoutFlexSpecification())
                .CustomConfigure(base::BindOnce([](views::Label* label) {
                  TypographyProvider::Get()->StyleLabel(
                      TypographyToken::kCrosButton2, *label);
                })))
        .BuildChildren();

    close_button_ = AddChildView(std::make_unique<CloseButton>(
        base::BindOnce(
            [](const base::WeakPtr<TabAppSelectionItemView>& item_view,
               InitParams::CloseCallback callback) {
              if (item_view) {
                std::move(callback).Run(item_view.get());
              }
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(params.close_callback)),
        CloseButton::Type::kMediumFloating));
    close_button_->SetVisible(false);

    auto* delegate = Shell::Get()->saved_desk_delegate();
    auto set_icon_image_callback = base::BindOnce(
        [](const base::WeakPtr<TabAppSelectionItemView>& item_view,
           const gfx::ImageSkia& favicon) {
          if (item_view) {
            item_view->image_->SetImage(favicon);
          }
        },
        weak_ptr_factory_.GetWeakPtr());

    switch (params.type) {
      case InitParams::Type::kTab: {
        delegate->GetFaviconForUrl(params.identifier, /*lacros_profile_id=*/0,
                                   std::move(set_icon_image_callback),
                                   &cancelable_favicon_task_tracker_);
        return;
      }
      case InitParams::Type::kApp: {
        //  The callback may be called synchronously.
        delegate->GetIconForAppId(params.identifier, kImageSize,
                                  std::move(set_icon_image_callback));
        return;
      }
    }
  }
  TabAppSelectionItemView(const TabAppSelectionItemView&) = delete;
  TabAppSelectionItemView& operator=(const TabAppSelectionItemView&) = delete;
  ~TabAppSelectionItemView() override = default;

  // views::BoxLayoutView:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    SetSelected(true);
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    SetSelected(false);
  }
  void OnFocus() override { SetSelected(true); }
  void OnBlur() override { SetSelected(false); }

 private:
  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }

    selected_ = selected;
    close_button_->SetVisible(selected);
    SetBackground(selected_ ? views::CreateThemedSolidBackground(
                                  cros_tokens::kCrosSysHoverOnSubtle)
                            : nullptr);
  }

  // True when the mouse is hovered over this view. The background is painted
  // differently.
  bool selected_ = false;

  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_;
  raw_ptr<CloseButton> close_button_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;
  base::WeakPtrFactory<TabAppSelectionItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(TabAppSelectionItemView)
END_METADATA

}  // namespace

TabAppSelectionView::TabAppSelectionView() {
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(/*min_height=*/0,
                             /*max_height=*/kScrollViewMaxHeight);
  // TODO(http://b/361326120): This applies a rectangle themed background. We
  // will need to set this to std::nullopt and apply a rounded rectangle
  // background elsewhere, or clip the contents after it has been set (painted
  // to a layer).
  scroll_view_->SetBackgroundThemeColorId(
      cros_tokens::kCrosSysSystemOnBaseOpaque);
  scroll_view_->SetBorder(std::make_unique<views::HighlightBorder>(
      kContainerCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  scroll_view_->SetViewportRoundedCornerRadius(kContainerCornerRadius);

  AddChildView(views::Builder<views::Separator>()
                   .SetColorId(cros_tokens::kCrosSysSeparator)
                   .SetOrientation(views::Separator::Orientation::kHorizontal)
                   .Build());

  auto contents =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .SetInsideBorderInsets(kContentsInsets)
          .Build();

  // TODO(http://b/361326120): Grab the lists of tabs and apps from the model or
  // provider.
  contents->AddChildView(CreateSubtitle(u"Tabs"));
  for (int i = 0; i < 10; ++i) {
    TabAppSelectionItemView::InitParams params;
    params.type = TabAppSelectionItemView::InitParams::Type::kTab;
    params.identifier = "https://www.nhl.com/";
    params.close_callback = base::BindOnce(
        &TabAppSelectionView::OnCloseButtonPressed, base::Unretained(this));
    contents->AddChildView(
        std::make_unique<TabAppSelectionItemView>(std::move(params)));
  }
  contents->AddChildView(CreateSubtitle(u"Apps"));
  for (int i = 0; i < 8; ++i) {
    TabAppSelectionItemView::InitParams params;
    params.type = TabAppSelectionItemView::InitParams::Type::kApp;
    params.identifier = "odknhmnlageboeamepcngndbggdpaobj";
    params.close_callback = base::BindOnce(
        &TabAppSelectionView::OnCloseButtonPressed, base::Unretained(this));
    contents->AddChildView(
        std::make_unique<TabAppSelectionItemView>(std::move(params)));
  }

  scroll_view_->SetContents(std::move(contents));
}

TabAppSelectionView::~TabAppSelectionView() = default;

void TabAppSelectionView::OnCloseButtonPressed(views::View* sender) {
  scroll_view_->contents()->RemoveChildViewT(sender);
}

BEGIN_METADATA(TabAppSelectionView)
END_METADATA

}  // namespace ash
