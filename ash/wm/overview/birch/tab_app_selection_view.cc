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

// If the menu has two items or less, do not allow deleting.
constexpr int kMinItems = 2;

std::unique_ptr<views::Label> CreateSubtitle(const std::u16string& text,
                                             int id) {
  return views::Builder<views::Label>()
      .SetText(text)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
      .SetProperty(views::kMarginsKey, kSubtitleMargins)
      .SetID(id)
      .CustomConfigure(base::BindOnce([](views::Label* label) {
        TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                              *label);
      }))
      .Build();
}

}  // namespace

// -----------------------------------------------------------------------------
// TabAppSelectionView::TabAppSelectionItemView:
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
class TabAppSelectionView::TabAppSelectionItemView
    : public views::BoxLayoutView {
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

    raw_ptr<TabAppSelectionView> owner;

    bool show_close_button = true;
  };

  explicit TabAppSelectionItemView(InitParams params) : owner_(params.owner) {
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

    if (params.show_close_button) {
      close_button_ = AddChildView(std::make_unique<CloseButton>(
          base::BindOnce(&TabAppSelectionItemView::OnCloseButtonPressed,
                         base::Unretained(this)),
          CloseButton::Type::kMediumFloating));
      close_button_->SetVisible(false);
      close_button_->SetID(TabAppSelectionView::kCloseButtonID);
    }

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

  void RemoveCloseButton() {
    if (!close_button_) {
      return;
    }
    RemoveChildViewT(close_button_.ExtractAsDangling().get());
  }

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
  void OnCloseButtonPressed() {
    // `this` will be destroyed.
    owner_->OnCloseButtonPressed(this);
  }

  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }

    selected_ = selected;
    if (close_button_) {
      close_button_->SetVisible(selected);
    }
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

  const raw_ptr<TabAppSelectionView> owner_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;
  base::WeakPtrFactory<TabAppSelectionItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(TabAppSelectionView, TabAppSelectionItemView)
END_METADATA

// -----------------------------------------------------------------------------
// TabAppSelectionView:
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
  const int num_tabs = 3;
  const int num_apps = 2;
  tab_item_views_.reserve(num_tabs);
  app_item_views_.reserve(num_apps);
  const bool show_close_button = (num_tabs + num_apps) > kMinItems;
  auto create_item_view =
      [&](TabAppSelectionItemView::InitParams::Type type,
          const std::string& identifier,
          std::vector<raw_ptr<TabAppSelectionItemView>>& container) {
        TabAppSelectionItemView::InitParams params;
        params.type = type;
        params.identifier = identifier;
        params.owner = this;
        params.show_close_button = show_close_button;
        auto* item_view = contents->AddChildView(
            std::make_unique<TabAppSelectionItemView>(std::move(params)));
        container.push_back(item_view);
      };

  if (num_tabs > 0) {
    contents->AddChildView(CreateSubtitle(u"Tabs", kTabSubtitleID));
    for (int i = 0; i < num_tabs; ++i) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kTab,
                       "https://www.nhl.com/", tab_item_views_);
    }
  }

  if (num_apps > 0) {
    contents->AddChildView(CreateSubtitle(u"Apps", kAppSubtitleID));
    for (int i = 0; i < num_apps; ++i) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kApp,
                       "odknhmnlageboeamepcngndbggdpaobj", app_item_views_);
    }
  }

  scroll_view_->SetContents(std::move(contents));
}

TabAppSelectionView::~TabAppSelectionView() = default;

void TabAppSelectionView::OnCloseButtonPressed(
    TabAppSelectionItemView* sender) {
  std::erase(tab_item_views_, sender);
  std::erase(app_item_views_, sender);
  scroll_view_->contents()->RemoveChildViewT(sender);

  // Remove the subtitle if necessary.
  if (tab_item_views_.empty()) {
    if (views::View* subtitle = GetViewByID(kTabSubtitleID)) {
      scroll_view_->contents()->RemoveChildViewT(subtitle);
    }
  }

  if (app_item_views_.empty()) {
    if (views::View* subtitle = GetViewByID(kAppSubtitleID)) {
      scroll_view_->contents()->RemoveChildViewT(subtitle);
    }
  }

  if (tab_item_views_.size() + app_item_views_.size() > kMinItems) {
    return;
  }

  // Remove all close buttons if we have 3 elements or less. This function won't
  // be called again.
  for (TabAppSelectionItemView* item : tab_item_views_) {
    item->RemoveCloseButton();
  }
  for (TabAppSelectionItemView* item : app_item_views_) {
    item->RemoveCloseButton();
  }
}

BEGIN_METADATA(TabAppSelectionView)
END_METADATA

}  // namespace ash
