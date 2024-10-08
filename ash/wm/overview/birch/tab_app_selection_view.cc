// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_view.h"

#include "ash/birch/birch_coral_item.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/style/typography.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
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

  explicit TabAppSelectionItemView(InitParams params)
      : type_(params.type), owner_(params.owner) {
    // TODO(http://b/361326120): Add a default icon.
    views::Label* title;
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
                .CopyAddressTo(&title)
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
           const gfx::ImageSkia& icon) {
          if (item_view) {
            item_view->image_->SetImage(
                icon.isNull() ? ui::ImageModel::FromVectorIcon(kDefaultAppIcon)
                              : ui::ImageModel::FromImageSkia(icon));
          }
        },
        weak_ptr_factory_.GetWeakPtr());

    switch (params.type) {
      case InitParams::Type::kTab: {
        delegate->GetFaviconForUrl(params.identifier, /*lacros_profile_id=*/0,
                                   std::move(set_icon_image_callback),
                                   &cancelable_favicon_task_tracker_);
        title->SetText(base::UTF8ToUTF16(params.identifier));
        return;
      }
      case InitParams::Type::kApp: {
        //  The callback may be called synchronously.
        delegate->GetIconForAppId(params.identifier, kImageSize,
                                  std::move(set_icon_image_callback));

        // Retrieve the title from the app registry cache, which may be null in
        // tests.
        if (apps::AppRegistryCache* cache =
                apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
                    Shell::Get()->session_controller()->GetActiveAccountId())) {
          cache->ForOneApp(params.identifier,
                           [&title](const apps::AppUpdate& update) {
                             title->SetText(base::UTF8ToUTF16(update.Name()));
                           });
        }
        return;
      }
    }
  }
  TabAppSelectionItemView(const TabAppSelectionItemView&) = delete;
  TabAppSelectionItemView& operator=(const TabAppSelectionItemView&) = delete;
  ~TabAppSelectionItemView() override = default;

  InitParams::Type type() const { return type_; }

  bool selected() const { return selected_; }
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

  void RemoveCloseButton() {
    if (!close_button_) {
      return;
    }
    RemoveChildViewT(std::exchange(close_button_, nullptr));
  }

  // views::BoxLayoutView:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    SetSelected(true);
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    SetSelected(false);
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTap) {
      owner_->OnItemTapped(this);
    }
  }
  void OnFocus() override { SetSelected(true); }
  void OnBlur() override { SetSelected(false); }

 private:
  void OnCloseButtonPressed() {
    // `this` will be destroyed.
    owner_->OnCloseButtonPressed(this);
  }

  const InitParams::Type type_;

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
TabAppSelectionView::TabAppSelectionView(BirchCoralItem* coral_item) {
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, kContainerCornerRadius, 0));

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(/*min_height=*/0,
                             /*max_height=*/kScrollViewMaxHeight);
  // This applies a non-rounded rectangle themed background. We set this to
  // std::nullopt and apply a rounded rectangle background above on the whole
  // view. We still need to set the viewport rounded corner radius to clip the
  // child backgrounds when they are hovered over.
  scroll_view_->SetBackgroundThemeColorId(std::nullopt);
  scroll_view_->SetBorder(std::make_unique<views::HighlightBorder>(
      kContainerCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  scroll_view_->SetViewportRoundedCornerRadius(kContainerCornerRadius);
  scroll_view_->SetDrawOverflowIndicator(false);

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
  const size_t num_tabs = coral_item->page_urls().size();
  const size_t num_apps = coral_item->app_ids().size();
  item_views_.reserve(num_tabs + num_apps);
  const bool show_close_button = (num_tabs + num_apps) > kMinItems;
  auto create_item_view =
      [&](TabAppSelectionItemView::InitParams::Type type,
          const std::string& identifier) {
        TabAppSelectionItemView::InitParams params;
        params.type = type;
        params.identifier = identifier;
        params.owner = this;
        params.show_close_button = show_close_button;
        auto* item_view = contents->AddChildView(
            std::make_unique<TabAppSelectionItemView>(std::move(params)));
        item_views_.push_back(item_view);
      };

  if (num_tabs > 0) {
    contents->AddChildView(CreateSubtitle(u"Tabs", kTabSubtitleID));
    for (const GURL& gurl : coral_item->page_urls()) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kTab,
                       gurl.spec());
    }
  }

  if (num_apps > 0) {
    contents->AddChildView(CreateSubtitle(u"Apps", kAppSubtitleID));
    for (const std::string& app_id : coral_item->app_ids()) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kApp, app_id);
    }
  }

  scroll_view_->SetContents(std::move(contents));
}

TabAppSelectionView::~TabAppSelectionView() = default;

void TabAppSelectionView::ProcessKeyEvent(ui::KeyEvent* event) {
  switch (event->key_code()) {
    case ui::VKEY_UP:
      AdvanceSelection(/*reverse=*/true);
      break;
    case ui::VKEY_DOWN:
      AdvanceSelection(/*reverse=*/false);
      break;
    default:
      break;
  }
}

void TabAppSelectionView::AdvanceSelection(bool reverse) {
  std::optional<size_t> selected_index;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    if (item_views_[i]->selected()) {
      selected_index = i;
      break;
    }
  }

  // If nothing is currently selected, then select the first or last item
  // depending on `reverse`.
  if (!selected_index) {
    if (reverse) {
      item_views_.back()->SetSelected(true);
    } else {
      item_views_.front()->SetSelected(true);
    }
    return;
  }

  size_t new_selected_index;
  if (reverse) {
    // Decrease the index unless the current selection is the first item, then
    // wrap around.
    if (*selected_index == 0) {
      new_selected_index = item_views_.size() - 1;
    } else {
      new_selected_index = *selected_index - 1;
    }
  } else {
    // Increase the index unless the current selection is the last item, then
    // wrap around.
    if (*selected_index == item_views_.size() - 1) {
      new_selected_index = 0;
    } else {
      new_selected_index = *selected_index + 1;
    }
  }

  item_views_[new_selected_index]->SetSelected(true);
  item_views_[*selected_index]->SetSelected(false);
}

void TabAppSelectionView::OnCloseButtonPressed(
    TabAppSelectionItemView* sender) {
  TabAppSelectionItemView::InitParams::Type sender_type = sender->type();
  std::erase(item_views_, sender);
  scroll_view_->contents()->RemoveChildViewT(sender);

  // Remove the subtitle(s) if necessary.
  bool remove_subtitle = true;
  for (TabAppSelectionItemView* item : item_views_) {
    if (sender_type == item->type()) {
      remove_subtitle = false;
      break;
    }
  }

  if (remove_subtitle) {
    std::optional<ViewID> id;
    switch (sender_type) {
      case TabAppSelectionItemView::InitParams::Type::kTab:
        id = kTabSubtitleID;
        break;
      case TabAppSelectionItemView::InitParams::Type::kApp:
        id = kAppSubtitleID;
        break;
    }
    CHECK(id.has_value());
    scroll_view_->contents()->RemoveChildViewT(GetViewByID(*id));
  }

  if (item_views_.size() > kMinItems) {
    return;
  }

  // Remove all close buttons if we have 3 elements or less. This function won't
  // be called again.
  for (TabAppSelectionItemView* item : item_views_) {
    item->RemoveCloseButton();
  }
}

void TabAppSelectionView::OnItemTapped(TabAppSelectionItemView* sender) {
  for (TabAppSelectionItemView* item : item_views_) {
    if (item != sender) {
      item->SetSelected(false);
    } else {
      item->SetSelected(!item->selected());
    }
  }
}

BEGIN_METADATA(TabAppSelectionView)
END_METADATA

}  // namespace ash
