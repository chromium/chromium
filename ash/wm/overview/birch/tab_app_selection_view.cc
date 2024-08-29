// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// TODO(http://b/361326120): The below are hardcoded temporary values.
constexpr gfx::Insets kItemInsets = gfx::Insets::VH(0, 12);
constexpr int kImageSize = 30;
constexpr gfx::Size kImagePreferredSize(50, 50);
constexpr int kScrollViewMaxHeight = 400;

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
        .SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetInsideBorderInsets(kItemInsets)
        .SetNotifyEnterExitOnChild(true)
        .AddChildren(views::Builder<views::ImageView>()
                         .CopyAddressTo(&image_)
                         .SetImage(ui::ImageModel::FromVectorIcon(
                             kDefaultAppIcon, cros_tokens::kCrosSysOnPrimary))
                         .SetImageSize(gfx::Size(kImageSize, kImageSize))
                         .SetPreferredSize(kImagePreferredSize),
                     views::Builder<views::Label>()
                         .SetText(u"Title")
                         .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                         .SetProperty(views::kBoxLayoutFlexKey,
                                      views::BoxLayoutFlexSpecification()))
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

 private:
  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }

    selected_ = selected;
    close_button_->SetVisible(selected);
    // TODO(http://b/361326120): Used a themed background.
    SetBackground(selected_ ? views::CreateSolidBackground(gfx::kGoogleGrey600)
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
  // TODO(http://b/361326120): Remove temporary background.
  SetBackgroundColor(SK_ColorBLUE);
  ClipHeightTo(/*min_height=*/0, /*max_height=*/kScrollViewMaxHeight);

  auto contents =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .Build();

  // TODO(http://b/361326120): Grab the lists of tabs and apps from the model or
  // provider.
  contents->AddChildView(views::Builder<views::Label>()
                             .SetText(u"Tabs")
                             .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                             .Build());
  for (int i = 0; i < 10; ++i) {
    TabAppSelectionItemView::InitParams params;
    params.type = TabAppSelectionItemView::InitParams::Type::kTab;
    params.identifier = "https://www.nhl.com/";
    params.close_callback = base::BindOnce(
        &TabAppSelectionView::OnCloseButtonPressed, base::Unretained(this));
    contents->AddChildView(
        std::make_unique<TabAppSelectionItemView>(std::move(params)));
  }
  contents->AddChildView(views::Builder<views::Label>()
                             .SetText(u"Apps")
                             .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                             .Build());
  for (int i = 0; i < 8; ++i) {
    TabAppSelectionItemView::InitParams params;
    params.type = TabAppSelectionItemView::InitParams::Type::kApp;
    params.identifier = "odknhmnlageboeamepcngndbggdpaobj";
    params.close_callback = base::BindOnce(
        &TabAppSelectionView::OnCloseButtonPressed, base::Unretained(this));
    contents->AddChildView(
        std::make_unique<TabAppSelectionItemView>(std::move(params)));
  }

  SetContents(std::move(contents));
}

TabAppSelectionView::~TabAppSelectionView() = default;

void TabAppSelectionView::OnCloseButtonPressed(views::View* sender) {
  contents()->RemoveChildViewT(sender);
}

BEGIN_METADATA(TabAppSelectionView)
END_METADATA

}  // namespace ash
