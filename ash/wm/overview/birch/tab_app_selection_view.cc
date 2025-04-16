// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/tab_app_selection_view.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/coral_util.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "base/containers/flat_tree.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kScrollViewMaxHeight = 358;
// The space between the user feedback view and the tab app items view.
constexpr int kChildSpacing = 8;

constexpr int kItemChildSpacing = 16;
constexpr gfx::Insets kItemInsets = gfx::Insets::VH(8, 16);
constexpr int kImageSize = 20;
constexpr gfx::Size kImagePreferredSize(20, 20);

// UserFeedbackView.
constexpr int kUserFeedbackChildSpacing = 8;
constexpr gfx::Insets kUserFeedbackInsets(16);
constexpr gfx::RoundedCornersF kUserFeedbackContainerCornerRadius(20.f);

constexpr gfx::Insets kContentsInsets = gfx::Insets::VH(8, 0);

constexpr gfx::RoundedCornersF kTabAppItemsContainerCornerRadius(20.f,
                                                                 20.f,
                                                                 0.f,
                                                                 0.f);

constexpr gfx::Insets kSubtitleMargins = gfx::Insets::VH(8, 16);

// If the menu has two items or less, do not allow deleting.
constexpr int kMinItems = 2;

std::unique_ptr<views::Label> CreateSubtitle(int text_message_id, int id) {
  return views::Builder<views::Label>()
      .SetText(l10n_util::GetStringUTF16(text_message_id))
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetEnabledColor(cros_tokens::kCrosSysOnSurface)
      .SetProperty(views::kMarginsKey, kSubtitleMargins)
      .SetID(id)
      .CustomConfigure(base::BindOnce([](views::Label* label) {
        TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                              *label);
      }))
      .Build();
}

// Returns the pref service to use for coral policy prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
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

    // Title of the tab or app.
    std::string title;

    raw_ptr<TabAppSelectionView> owner;

    bool show_close_button = true;

    // Used by accessibility to speak "Menu item pos in size".
    // Indicates the initial position of this item in the parent selector view
    // and the number of elements in the parent selector view. Used by
    // accessibility to give spoken feedback: "Menu item `position_in_selector`
    // in `num_selector_elements`". The view accessibility will be updated when
    // an item is closed.
    int position_in_selector = 0;
    int num_selector_elements = 0;
  };

  explicit TabAppSelectionItemView(InitParams params)
      : type_(params.type),
        title_(base::UTF8ToUTF16(params.title)),
        identifier_(params.identifier),
        owner_(params.owner) {
    views::Builder<views::BoxLayoutView>(this)
        .SetAccessibleRole(ax::mojom::Role::kMenuItem)
        .SetAccessibleName(
            params.show_close_button
                ? l10n_util::GetStringFUTF16(
                      IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_ITEM_WITH_REMOVE_BUTTON,
                      title_)
                : title_)
        .SetBetweenChildSpacing(kItemChildSpacing)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
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
                .SetText(base::UTF8ToUTF16(params.title))
                .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                .SetProperty(views::kBoxLayoutFlexKey,
                             views::BoxLayoutFlexSpecification())
                .CustomConfigure(base::BindOnce([](views::Label* label) {
                  TypographyProvider::Get()->StyleLabel(
                      TypographyToken::kCrosButton2, *label);
                  label->GetViewAccessibility().SetIsIgnored(true);
                })))
        .BuildChildren();

    SetPositionAndSetSize(params.position_in_selector,
                          params.num_selector_elements);

    if (params.show_close_button) {
      close_button_ = AddChildView(std::make_unique<CloseButton>(
          base::BindOnce(&TabAppSelectionItemView::OnCloseButtonPressed,
                         base::Unretained(this)),
          CloseButton::Type::kMediumFloating));
      // Use enabled state and opacity to hide and show the button instead of
      // `SetVisible()` as the latter will invalidate the layout.
      close_button_->SetPaintToLayer();
      close_button_->layer()->SetFillsBoundsOpaquely(false);
      close_button_->layer()->SetOpacity(0.f);
      close_button_->SetEnabled(false);
      close_button_->SetID(TabAppSelectionView::kCloseButtonID);
      close_button_->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_ASH_BIRCH_CORAL_SELECTOR_ITEM_CLOSE_BUTTON_TOOLTIP, title_));
      views::FocusRing::Get(close_button_)
          ->SetHasFocusPredicate(base::BindRepeating(
              [](const TabAppSelectionView* owner, const views::View* view) {
                return owner->focus_view_ == view;
              },
              base::Unretained(owner_)));
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
        delegate->GetFaviconForUrl(params.identifier,
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

  InitParams::Type type() const { return type_; }
  std::string identifier() const { return identifier_; }
  CloseButton* close_button() { return close_button_; }

  void SetPositionAndSetSize(int position_in_selector,
                             int num_selector_elements) {
    GetViewAccessibility().SetPosInSet(position_in_selector);
    GetViewAccessibility().SetSetSize(num_selector_elements);
  }

  bool selected() const { return selected_; }
  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }

    selected_ = selected;
    if (close_button_) {
      close_button_->layer()->SetOpacity(selected ? 1.f : 0.f);
      close_button_->SetEnabled(selected);
    }
    SetBackground(selected_ ? views::CreateSolidBackground(
                                  cros_tokens::kCrosSysHoverOnSubtle)
                            : nullptr);
  }

  void RemoveCloseButton() {
    if (!close_button_) {
      return;
    }
    RemoveChildViewT(std::exchange(close_button_, nullptr));
    SetAccessibleName(title_);
  }

  // views::BoxLayoutView:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    owner_->MoveFocus(this);
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    owner_->MoveFocus(nullptr);
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTap) {
      owner_->OnItemTapped(this);
    }
  }
  void OnFocus() override { owner_->MoveFocus(this); }
  void OnBlur() override { owner_->MoveFocus(nullptr); }

  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    // Override accessibility focus behavior.
    if (action_data.action == ax::mojom::Action::kFocus) {
      owner_->MoveFocus(this);
      return true;
    }
    return views::BoxLayoutView::HandleAccessibleAction(action_data);
  }

 private:
  void OnCloseButtonPressed() {
    // `this` will be destroyed.
    owner_->OnCloseButtonPressed(this);
  }

  const InitParams::Type type_;
  const std::u16string title_;
  const std::string identifier_;

  // True when the mouse is hovered over this view. The background is painted
  // differently.
  bool selected_ = false;

  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_;
  raw_ptr<CloseButton> close_button_ = nullptr;

  const raw_ptr<TabAppSelectionView> owner_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;
  base::WeakPtrFactory<TabAppSelectionItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(TabAppSelectionView, TabAppSelectionItemView)
END_METADATA

// -----------------------------------------------------------------------------
// TabAppSelectionView::UserFeedbackView:
// A view that allows users to give feedback via the thumb up and thumb down
// buttons.
//
//   +-------------------------------------------+
//   |  +-----------------------+  +----++-+--+  |
//   |  |                       |  |    ||    |  |
//   |  +-----------------------+  +----++----+  |
//   +--^--------------------------^-----^-------+
//   ^  |                          |     |
//   |  `Label`                    |     'IconButton'(thumb down)
//   |                             'IconButton'(thumb up)
//   |
//   `UserFeedbackView`
// TODO(crbug.com/374117101): Add hover state for thumb up/down buttons.
class TabAppSelectionView::UserFeedbackView : public views::BoxLayoutView {
  METADATA_HEADER(UserFeedbackView, views::BoxLayoutView)

 public:
  explicit UserFeedbackView(TabAppSelectionView* owner) : owner_(owner) {
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    SetInsideBorderInsets(kUserFeedbackInsets);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBackground(views::CreateRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBaseOpaque,
        kUserFeedbackContainerCornerRadius, 0));
    SetBetweenChildSpacing(kUserFeedbackChildSpacing);
    SetBorder(std::make_unique<views::HighlightBorder>(
        kUserFeedbackContainerCornerRadius,
        views::HighlightBorder::Type::kHighlightBorderNoShadow));

    SetID(TabAppSelectionView::ViewID::kUserFeedbackID);
    SetAccessibleRole(ax::mojom::Role::kDialog);
    SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_BIRCH_CORAL_USER_FEEDBACK_DESCRIPTION));

    auto* feedback_label = AddChildView(std::make_unique<views::Label>());
    feedback_label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_BIRCH_CORAL_USER_FEEDBACK_DESCRIPTION));
    feedback_label->SetEnabledColor(cros_tokens::kCrosSysOnSurfaceVariant);
    feedback_label->SetMultiLine(true);
    feedback_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation2,
                                          *feedback_label);
    SetFlexForView(feedback_label, 1);

    auto* thumb_buttons_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    thumb_buttons_container->SetOrientation(
        views::BoxLayout::Orientation::kHorizontal);
    thumb_up_button_ =
        thumb_buttons_container->AddChildView(std::make_unique<IconButton>(
            base::BindOnce(&UserFeedbackView::OnThumbUpButtonPressed,
                           base::Unretained(this)),
            IconButton::Type::kMediumFloating, &kThumbUpIcon,
            l10n_util::GetStringUTF16(
                IDS_ASH_BIRCH_CORAL_THUMB_UP_ACCESSIBLE_NAME),
            /*is_togglable=*/true, /*has_border=*/false));
    thumb_up_button_->SetIconToggledColor(cros_tokens::kCrosSysOnPrimary);
    thumb_up_button_->SetBackgroundToggledColor(cros_tokens::kCrosSysPrimary);
    StyleUtil::SetUpInkDropForButton(thumb_up_button_, gfx::Insets(),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/false);
    thumb_up_button_->SetID(TabAppSelectionView::ViewID::kThumbsUpID);
    views::FocusRing::Get(thumb_up_button_)
        ->SetHasFocusPredicate(base::BindRepeating(
            [](const TabAppSelectionView* owner, const views::View* view) {
              return owner->focus_view_ == view;
            },
            base::Unretained(owner_)));

    thumb_down_button_ =
        thumb_buttons_container->AddChildView(std::make_unique<IconButton>(
            base::BindOnce(&UserFeedbackView::OnThumbDownButtonPressed,
                           base::Unretained(this)),
            IconButton::Type::kMediumFloating, &kThumbDownIcon,
            l10n_util::GetStringUTF16(
                IDS_ASH_BIRCH_CORAL_THUMB_DOWN_ACCESSIBLE_NAME),
            /*is_togglable=*/true, /*has_border=*/false));
    thumb_down_button_->SetIconToggledColor(cros_tokens::kCrosSysOnPrimary);
    thumb_down_button_->SetBackgroundToggledColor(cros_tokens::kCrosSysPrimary);
    StyleUtil::SetUpInkDropForButton(thumb_down_button_, gfx::Insets(),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/false);
    thumb_down_button_->SetID(TabAppSelectionView::ViewID::kThumbsDownID);
    views::FocusRing::Get(thumb_down_button_)
        ->SetHasFocusPredicate(base::BindRepeating(
            [](const TabAppSelectionView* owner, const views::View* view) {
              return owner->focus_view_ == view;
            },
            base::Unretained(owner_)));
  }

  UserFeedbackView(const UserFeedbackView&) = delete;
  UserFeedbackView& operator=(const UserFeedbackView&) = delete;
  ~UserFeedbackView() override = default;

 private:
  void OnThumbUpButtonPressed() {
    // `thumb_up_button_` should only be toggled one time.
    // Please note, the `thumb_up_button_` theoratically should be untoggled if
    // the `thumb_down_button_` is pressed and `thumb_down_button_` should be
    // untoggle if the `thumb_up_button_` is pressed. For now once
    // `thumb_down_button_` is pressed, it will exit overview mode. Hence we
    // don't support un-toggle for now.
    if (thumb_up_button_->toggled()) {
      return;
    }

    // Currently `thumb_up_button_` is set to toggled the first time it's
    // pressed and remains toggled the the rest of its life time. Hence remove
    // the hover effect for it the first time it's pressed.
    StyleUtil::SetUpInkDropForButton(thumb_up_button_, gfx::Insets(),
                                     /*highlight_on_hover=*/false,
                                     /*highlight_on_focus=*/false);
    thumb_up_button_->SetToggled(/*toggled=*/true);
    base::UmaHistogramBoolean("Ash.Birch.Coral.UserFeedback", true);
  }

  void OnThumbDownButtonPressed() {
    // `thumb_down_button_` should only be toggled one time.
    if (thumb_down_button_->toggled()) {
      return;
    }

    // Even overview mode will be ended and `this` will be destroyed after
    // `thumb_down_button_` is pressed, there's a very short amount time that
    // both buttons will be shown as toggled. Hence manually set
    // `thumb_up_button_` untoggled for correct visual effect.
    if (thumb_up_button_->toggled()) {
      thumb_up_button_->SetToggled(/*toggled=*/false);
    }

    thumb_down_button_->SetToggled(/*toggled=*/true);
    base::UmaHistogramBoolean("Ash.Birch.Coral.UserFeedback", false);
    if (auto* birch_bar_controller = BirchBarController::Get()) {
      birch_bar_controller->ProvideFeedbackForCoral();
    }
  }

  // `owner_` owns this.
  const raw_ptr<TabAppSelectionView> owner_;

  // Owned by the views hierarchy.
  raw_ptr<IconButton> thumb_up_button_;
  raw_ptr<IconButton> thumb_down_button_;
};

BEGIN_METADATA(TabAppSelectionView, UserFeedbackView)
END_METADATA

// -----------------------------------------------------------------------------
// TabAppSelectionView:
TabAppSelectionView::TabAppSelectionView(const base::Token& group_id,
                                         base::RepeatingClosure on_item_removed)
    : group_id_(group_id), on_item_removed_(on_item_removed) {
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBetweenChildSpacing(kChildSpacing);
  GetViewAccessibility().SetIsVertical(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CORAL_SELECTOR_ACCESSIBLE_NAME));

  if (coral_util::IsCoralFeedbackAllowedByPolicy(GetPrefService())) {
    AddChildView(std::make_unique<UserFeedbackView>(this));
  }

  auto* tab_app_items_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  tab_app_items_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  tab_app_items_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  tab_app_items_view->SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque,
      kTabAppItemsContainerCornerRadius, 0));

  scroll_view_ =
      tab_app_items_view->AddChildView(std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(/*min_height=*/0,
                             /*max_height=*/kScrollViewMaxHeight);
  // This applies a non-rounded rectangle themed background. We set this to
  // std::nullopt and apply a rounded rectangle background above on the whole
  // view. We still need to set the viewport rounded corner radius to clip the
  // child backgrounds when they are hovered over.
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->SetBorder(std::make_unique<views::HighlightBorder>(
      kTabAppItemsContainerCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  scroll_view_->SetViewportRoundedCornerRadius(
      kTabAppItemsContainerCornerRadius);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  tab_app_items_view->AddChildView(
      views::Builder<views::Separator>()
          .SetColorId(cros_tokens::kCrosSysSeparator)
          .SetOrientation(views::Separator::Orientation::kHorizontal)
          .Build());

  auto contents =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .SetInsideBorderInsets(kContentsInsets)
          .Build();

  // Grab the lists of tabs and apps from data provider.
  const coral::mojom::GroupPtr& group =
      BirchCoralProvider::Get()->GetGroupById(group_id_);
  coral_util::TabsAndApps tabs_apps =
      coral_util::SplitContentData(group->entities);

  const size_t num_tabs = tabs_apps.tabs.size();
  const size_t num_apps = tabs_apps.apps.size();
  item_views_.reserve(num_tabs + num_apps);
  const bool show_close_button = (num_tabs + num_apps) > kMinItems;
  auto create_item_view = [&](TabAppSelectionItemView::InitParams::Type type,
                              const std::string& identifier,
                              const std::string& title,
                              int position_in_selector) {
    TabAppSelectionItemView::InitParams params;
    params.type = type;
    params.identifier = identifier;
    params.title = title;
    params.owner = this;
    params.show_close_button = show_close_button;
    params.position_in_selector = position_in_selector;
    params.num_selector_elements = static_cast<int>(num_tabs + num_apps);
    auto* item_view = contents->AddChildView(
        std::make_unique<TabAppSelectionItemView>(std::move(params)));
    item_views_.push_back(item_view);
  };

  int position = 1;
  if (num_tabs > 0) {
    contents->AddChildView(CreateSubtitle(
        IDS_ASH_BIRCH_CORAL_SELECTOR_TAB_SUBTITLE, kTabSubtitleID));
    for (const coral::mojom::Tab& tab : tabs_apps.tabs) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kTab,
                       tab.url.spec(), tab.title, position++);
    }
  }

  if (num_apps > 0) {
    contents->AddChildView(CreateSubtitle(
        IDS_ASH_BIRCH_CORAL_SELECTOR_APP_SUBTITLE, kAppSubtitleID));
    for (const coral::mojom::App& app : tabs_apps.apps) {
      create_item_view(TabAppSelectionItemView::InitParams::Type::kApp, app.id,
                       app.title, position++);
    }
  }

  scroll_view_->SetContents(std::move(contents));
}

TabAppSelectionView::~TabAppSelectionView() = default;

void TabAppSelectionView::ClearSelection() {
  MoveFocus(nullptr);

  for (TabAppSelectionItemView* item : item_views_) {
    item->SetSelected(false);
  }
}

bool TabAppSelectionView::AdvanceFocusForTabKey(bool reverse) {
  std::vector<views::View*> focus_list = BuildFocusList();
  views::View* next_focus_view = nullptr;
  auto iter = std::ranges::find(focus_list, focus_view_);
  if (iter == focus_list.end()) {
    next_focus_view = reverse ? focus_list.back() : focus_list.front();
  } else {
    if (reverse && iter != focus_list.begin()) {
      next_focus_view = *(std::ranges::prev(iter));
    } else if (!reverse && std::ranges::next(iter) != focus_list.end()) {
      next_focus_view = *(std::ranges::next(iter));
    }
  }
  MoveFocus(next_focus_view);
  return !!next_focus_view;
}

void TabAppSelectionView::AdvanceFocusForArrowKey(bool up) {
  std::optional<size_t> selected_index = GetSelectedIndex();

  // If nothing is currently selected, then select the first or last item
  // depending on `reverse`.
  if (!selected_index) {
    MoveFocus(up ? item_views_.back() : item_views_.front());
    return;
  }

  size_t new_selected_index;
  if (up) {
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

  MoveFocus(item_views_[new_selected_index]);
}

void TabAppSelectionView::MaybePressOnFocusView() {
  if (!focus_view_) {
    return;
  }

  if (auto* button = views::Button::AsButton(focus_view_)) {
    button->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_SPACE, /*modifiers=*/ui::EF_IS_SYNTHESIZED));
  }
}

void TabAppSelectionView::RemoveItemBySystem(std::string_view identifier) {
  auto iter =
      std::ranges::find_if(item_views_, [&identifier](const auto& item_view) {
        return item_view->identifier() == identifier;
      });
  if (iter != item_views_.end()) {
    RemoveItemView(iter->get());
  }
}

std::optional<size_t> TabAppSelectionView::GetSelectedIndex() const {
  std::optional<size_t> selected_index;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    if (item_views_[i]->selected()) {
      selected_index = i;
      break;
    }
  }
  return selected_index;
}

std::vector<views::View*> TabAppSelectionView::BuildFocusList() {
  std::vector<views::View*> focus_list;
  focus_list.emplace_back(GetViewByID(ViewID::kUserFeedbackID));
  focus_list.emplace_back(GetViewByID(ViewID::kThumbsUpID));
  focus_list.emplace_back(GetViewByID(ViewID::kThumbsDownID));

  std::optional<size_t> selected_index = GetSelectedIndex();
  const raw_ptr<TabAppSelectionItemView>& item_view =
      selected_index ? item_views_[*selected_index] : item_views_[0];
  focus_list.emplace_back(item_view);
  if (item_view->close_button()) {
    focus_list.emplace_back(item_view->close_button());
  }

  return focus_list;
}

void TabAppSelectionView::MoveFocus(views::View* next_focus_view) {
  if (focus_view_ == next_focus_view) {
    return;
  }

  auto* prev_focus_view = focus_view_.get();
  focus_view_ = next_focus_view;

  SetBlur(prev_focus_view);
  SetFocus(focus_view_);
}

void TabAppSelectionView::SetFocus(views::View* focus_view) {
  if (!focus_view) {
    return;
  }

  for (const auto& item_view : item_views_) {
    if (item_view == focus_view || item_view->close_button() == focus_view) {
      item_view->SetSelected(true);
    }
  }

  if (auto* focus_ring = views::FocusRing::Get(focus_view)) {
    focus_ring->SchedulePaint();
  }

  focus_view->GetViewAccessibility().NotifyEvent(ax::mojom::Event::kSelection,
                                                 /*send_native_event=*/true);
}

void TabAppSelectionView::SetBlur(views::View* blur_view) {
  if (!blur_view) {
    return;
  }

  if (auto* focus_ring = views::FocusRing::Get(blur_view)) {
    focus_ring->SchedulePaint();
  }

  for (const auto& item_view : item_views_) {
    if (item_view == blur_view || item_view->close_button() == blur_view) {
      item_view->SetSelected(false);
    }
  }
}

void TabAppSelectionView::OnCloseButtonPressed(
    TabAppSelectionItemView* sender) {
  BirchCoralProvider::Get()->RemoveItemFromGroup(group_id_,
                                                 sender->identifier());
  RemoveItemView(sender);
}

void TabAppSelectionView::RemoveItemView(TabAppSelectionItemView* item_view) {
  TabAppSelectionItemView::InitParams::Type item_type = item_view->type();

  if (focus_view_ == item_view || focus_view_ == item_view->close_button()) {
    MoveFocus(this);
  }

  std::erase(item_views_, item_view);
  scroll_view_->contents()->RemoveChildViewT(item_view);

  // Remove the subtitle(s) if necessary.
  bool remove_subtitle = true;
  for (TabAppSelectionItemView* item : item_views_) {
    if (item_type == item->type()) {
      remove_subtitle = false;
      break;
    }
  }

  if (remove_subtitle) {
    std::optional<ViewID> id;
    switch (item_type) {
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

  // Update the items' accessibility and remove all close buttons once if we
  // have `kMinItems` left. This function won't be called again.
  const int num_items = static_cast<int>(item_views_.size());
  for (int i = 0; i < num_items; ++i) {
    item_views_[i]->SetPositionAndSetSize(i + 1, num_items);
    if (num_items <= kMinItems) {
      item_views_[i]->RemoveCloseButton();
    }
  }

  on_item_removed_.Run();
}

void TabAppSelectionView::OnItemTapped(TabAppSelectionItemView* sender) {
  // Toggle selection for `sender`; clear selection otherwise.
  for (TabAppSelectionItemView* item : item_views_) {
    item->SetSelected(item == sender && !item->selected());
  }
}

views::View* TabAppSelectionView::GetItemViewAtForTesting(int i) {
  return item_views_[i];
}

BEGIN_METADATA(TabAppSelectionView)
END_METADATA

}  // namespace ash
