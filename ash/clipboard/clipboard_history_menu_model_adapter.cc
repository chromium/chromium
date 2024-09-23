// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/clipboard/views/clipboard_history_label.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Returns a font list resolved from the specified `typography_token`.
gfx::FontList Resolve(TypographyToken typography_token) {
  return TypographyProvider::Get()->ResolveTypographyToken(typography_token);
}

// Returns the elapsed time since the specified `time`.
base::TimeDelta TimeSince(const base::Time& time) {
  return base::Time::Now() - time;
}

// Returns whether the clipboard history menu requires a header.
bool IsHeaderRequired() {
  return chromeos::features::IsClipboardHistoryRefreshEnabled();
}

// Returns whether the clipboard history menu requires a footer.
bool IsFooterRequired(
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
    const std::optional<base::Time>& menu_last_time_shown,
    const std::optional<base::Time>& nudge_last_time_shown) {
  // A footer is always required when the menu is shown via Ctrl+V long press.
  using crosapi::mojom::ClipboardHistoryControllerShowSource;
  if (show_source == ClipboardHistoryControllerShowSource::kControlVLongpress) {
    return true;
  }

  // If the menu is not shown via Ctrl+V long press, footers require that the
  // clipboard history refresh feature be enabled.
  if (!chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    return false;
  }

  // A footer is required if the menu hasn't been shown in the past 60 days.
  if (TimeSince(menu_last_time_shown.value_or(base::Time())) >=
      base::Days(60)) {
    return true;
  }

  // A footer is required if a nudge has been shown in the past 60 seconds.
  return TimeSince(nudge_last_time_shown.value_or(base::Time())) <=
         base::Seconds(60);
}

// Populates `container` with a menu title to appear at the top of the clipboard
// history menu.
void InsertHeaderContent(views::MenuItemView* container) {
  container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBorder(
              views::CreateEmptyBorder(ClipboardHistoryViews::kContentsInsets))
          .AfterBuild(base::BindOnce([](views::BoxLayoutView* header) {
            const int width =
                clipboard_history_util::GetPreferredItemViewWidth();
            header->SetPreferredSize(
                gfx::Size(width, header->GetHeightForWidth(width)));
          }))
          .AddChild(views::Builder<views::Label>(
                        bubble_utils::CreateLabel(
                            TypographyToken::kCrosButton1,
                            l10n_util::GetStringUTF16(
                                IDS_ASH_CLIPBOARD_HISTORY_HEADER_TITLE),
                            cros_tokens::kCrosSysOnSurface))
                        .SetAccessibleRole(ax::mojom::Role::kHeading)
                        .SetHorizontalAlignment(gfx::ALIGN_LEFT))
          .Build());
}

// TODO(http://b/267694412): Add pixel test.
// Populates `styled_label` with educational text to appear at the bottom of the
// clipboard history menu. This method may only be called when clipboard history
// refresh is enabled.
void InsertFooterContentV2LabelStyledText(
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
    views::StyledLabel* styled_label) {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());

  // Create text style.
  views::StyledLabel::RangeStyleInfo text_style;
  text_style.custom_font = Resolve(TypographyToken::kCrosAnnotation1);
  text_style.override_color_id = cros_tokens::kCrosSysOnSurfaceVariant;

  // When the clipboard history menu is shown from a Ctrl+V long press event, a
  // specific educational text is used which does not require inline icons.
  using crosapi::mojom::ClipboardHistoryControllerShowSource;
  if (show_source == ClipboardHistoryControllerShowSource::kControlVLongpress) {
    styled_label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_CLIPBOARD_HISTORY_CONTROL_V_LONGPRESS_FOOTER));
    styled_label->AddStyleRange(gfx::Range(0u, styled_label->GetText().size()),
                                std::move(text_style));
    return;
  }

  // When the clipboard history menu is *not* shown from a Ctrl+V long press
  // event, set text based on keyboard layout, caching the offset where an
  // inline icon should be inserted.
  size_t inline_icon_offset;
  const auto& shortcut_key = clipboard_history_util::GetShortcutKeyName();
  styled_label->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_CLIPBOARD_HISTORY_FOOTER, shortcut_key, &inline_icon_offset));
  inline_icon_offset += shortcut_key.size();

  // Set text styles.
  styled_label->AddStyleRange(gfx::Range(0u, inline_icon_offset), text_style);
  styled_label->AddStyleRange(
      gfx::Range(inline_icon_offset + 1u, styled_label->GetText().size()),
      std::move(text_style));

  // Create inline icon.
  auto inline_icon =
      views::Builder<views::ImageView>()
          .SetBorder(views::CreateEmptyBorder(
              ClipboardHistoryViews::kInlineIconMargins))
          .SetImage(ui::ImageModel::FromVectorIcon(
              clipboard_history_util::GetShortcutKeyIcon(),
              cros_tokens::kCrosSysOnSurfaceVariant,
              ClipboardHistoryViews::kFooterContentV2InlineIconSize))
          .Build();

  // Insert inline icon.
  views::StyledLabel::RangeStyleInfo inline_icon_style;
  inline_icon_style.custom_view = inline_icon.get();
  styled_label->AddStyleRange(
      gfx::Range(inline_icon_offset, inline_icon_offset + 1u),
      std::move(inline_icon_style));

  // Transfer inline icon ownership.
  styled_label->AddCustomView(std::move(inline_icon));
}

// TODO(http://b/267694412): Add pixel test.
// Populates `container` with educational content to appear at the bottom of the
// clipboard history menu. This method may only be called when clipboard history
// refresh is enabled.
void InsertFooterContentV2(
    views::MenuItemView* container,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());

  // Cache `menu_padding`.
  const int menu_padding =
      views::MenuConfig::instance().vertical_touchable_menu_item_padding;

  // Compute `footer_margins`, accounting for `menu_padding`.
  gfx::Insets footer_margins = ClipboardHistoryViews::kFooterContentV2Margins;
  footer_margins -= gfx::Insets::TLBR(0, 0, menu_padding, 0);

  // Compute `footer_width`.
  const int footer_width = clipboard_history_util::GetPreferredItemViewWidth() -
                           footer_margins.width();

  container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysSystemOnBase1,
              ClipboardHistoryViews::kFooterContentV2BackgroundCornerRadius))
          .SetBetweenChildSpacing(
              ClipboardHistoryViews::kFooterContentV2ChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetID(clipboard_history_util::kFooterContentV2ViewID)
          .SetInsideBorderInsets(ClipboardHistoryViews::kFooterContentV2Insets)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetProperty(views::kMarginsKey, footer_margins)
          .AddChildren(
              views::Builder<views::ImageView>().SetImage(
                  ui::ImageModel::FromVectorIcon(
                      vector_icons::kHelpOutlineIcon,
                      cros_tokens::kCrosSysOnSurfaceVariant,
                      ClipboardHistoryViews::kFooterContentV2IconSize)),
              views::Builder<views::StyledLabel>()
                  .SetAutoColorReadabilityEnabled(false)
                  .SetID(clipboard_history_util::kFooterContentV2LabelID)
                  .SizeToFit(
                      footer_width -
                      ClipboardHistoryViews::kFooterContentV2Insets.width() -
                      ClipboardHistoryViews::kFooterContentV2IconSize -
                      ClipboardHistoryViews::kFooterContentV2ChildSpacing)
                  .CustomConfigure(base::BindOnce(
                      &InsertFooterContentV2LabelStyledText, show_source)))
          .Build());
}

// TODO(http://b/267694412): Add pixel test.
// Populates `container` with educational content to appear at the bottom of the
// clipboard history menu.
void InsertFooterContent(
    views::MenuItemView* container,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    InsertFooterContentV2(container, show_source);
    return;
  }

  const int content_width =
      clipboard_history_util::GetPreferredItemViewWidth() -
      ClipboardHistoryViews::kContentsInsets.width();

  // Introduce a layout view between `container` and the desired separator and
  // label to circumvent `container` manually laying out its children.
  container->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetID(clipboard_history_util::kFooterContentViewID)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              views::Builder<views::Separator>()
                  .SetBorder(views::CreateEmptyBorder(
                      ClipboardHistoryViews::kContentsInsets))
                  .SetColorId(cros_tokens::kCrosSysSeparator)
                  .SetOrientation(views::Separator::Orientation::kHorizontal)
                  .SetPreferredLength(content_width),
              views::Builder<views::Label>(
                  bubble_utils::CreateLabel(
                      TypographyToken::kCrosAnnotation1,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_CLIPBOARD_HISTORY_CONTROL_V_LONGPRESS_FOOTER),
                      cros_tokens::kCrosSysSecondary))
                  .SetBorder(views::CreateEmptyBorder(
                      ClipboardHistoryViews::kContentsInsets))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SizeToFit(/*fixed_width=*/content_width))
          .Build());
}

}  // namespace

// ClipboardHistoryMenuModelAdapter::MenuModelWithWillCloseCallback ------------

// Utility class that allows `ClipboardHistoryMenuModelAdapter` to run a task
// before its menu closes.
class ClipboardHistoryMenuModelAdapter::MenuModelWithWillCloseCallback
    : public ui::SimpleMenuModel {
 public:
  MenuModelWithWillCloseCallback(
      ui::SimpleMenuModel::Delegate* delegate,
      ClipboardHistoryController::OnMenuClosingCallback callback)
      : ui::SimpleMenuModel(delegate), callback_(std::move(callback)) {}

  // ui::SimpleMenuModel:
  void MenuWillClose() override {
    if (callback_) {
      std::move(callback_).Run(will_paste_item_);
    }

    ui::SimpleMenuModel::MenuWillClose();
  }

  void set_will_paste_item(bool will_paste_item) {
    will_paste_item_ = will_paste_item;
  }

 private:
  ClipboardHistoryController::OnMenuClosingCallback callback_;
  bool will_paste_item_ = false;
};

// ClipboardHistoryMenuModelAdapter::ScopedA11yIgnore --------------------------

// The scoped class to disable a11y for all items views.
class ClipboardHistoryMenuModelAdapter::ScopedA11yIgnore {
 public:
  explicit ScopedA11yIgnore(
      ClipboardHistoryMenuModelAdapter* menu_model_adapter)
      : menu_model_adapter_(menu_model_adapter) {
    SetIgnoreA11yForAllItemViews(true);
  }
  ~ScopedA11yIgnore() { SetIgnoreA11yForAllItemViews(false); }

 private:
  void SetIgnoreA11yForAllItemViews(bool ignore) {
    for (auto& item_view_command_id_pair :
         menu_model_adapter_->item_views_by_command_id_) {
      views::View* item_view = item_view_command_id_pair.second;
      item_view->GetViewAccessibility().SetIsIgnored(ignore);
    }
  }

  const raw_ptr<ClipboardHistoryMenuModelAdapter> menu_model_adapter_;
};

// ClipboardHistoryMenuModelAdapter --------------------------------------------

// static
std::unique_ptr<ClipboardHistoryMenuModelAdapter>
ClipboardHistoryMenuModelAdapter::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    ClipboardHistoryController::OnMenuClosingCallback on_menu_closing_callback,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history) {
  return base::WrapUnique(new ClipboardHistoryMenuModelAdapter(
      std::make_unique<MenuModelWithWillCloseCallback>(
          delegate, std::move(on_menu_closing_callback)),
      std::move(menu_closed_callback), clipboard_history));
}

ClipboardHistoryMenuModelAdapter::~ClipboardHistoryMenuModelAdapter() = default;

void ClipboardHistoryMenuModelAdapter::Run(
    const gfx::Rect& anchor_rect,
    ui::MenuSourceType source_type,
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source,
    const std::optional<base::Time>& menu_last_time_shown,
    const std::optional<base::Time>& nudge_last_time_shown) {
  DCHECK(!root_view_);
  DCHECK(model_);
  DCHECK(item_snapshots_.empty());
  DCHECK(item_views_by_command_id_.empty());

  // `Run()` should be called at most once for an instance.
  DCHECK(!run_before_);
  run_before_ = true;

  menu_open_time_ = base::TimeTicks::Now();
  menu_show_source_ = show_source;

  int command_id = clipboard_history_util::kFirstItemCommandId;
  const auto& items = clipboard_history_->GetItems();
  // Do not include the final kDeleteCommandId item in histograms, because it
  // is not shown.
  UMA_HISTOGRAM_COUNTS_100(
      "Ash.ClipboardHistory.ContextMenu.NumberOfItemsShown", items.size());

  size_t index = 0u;
  if (IsHeaderRequired()) {
    // Add a placeholder non-interactive item that will contain the clipboard
    // history menu's header.
    model_->AddTitle(std::u16string());
    header_index_ = index++;
  }

  for (const auto& item : items) {
    model_->AddItem(command_id, std::u16string());
    item_snapshots_.emplace(command_id, item);
    ++command_id;
    ++index;
  }

  if (IsFooterRequired(show_source, menu_last_time_shown,
                       nudge_last_time_shown)) {
    // Add a placeholder non-interactive item that will contain the clipboard
    // history menu's footer, consisting of a separator (styled differently from
    // the context menu separators) and educational text.
    model_->AddTitle(std::u16string());
    footer_index_ = index++;
  }

  // Start async rendering of HTML, if any exists.
  // This factory may be nullptr in tests.
  if (auto* clipboard_image_factory = ClipboardImageModelFactory::Get()) {
    clipboard_image_factory->Activate();
  }

  std::unique_ptr<views::MenuItemView> root_view = CreateMenu();
  root_view_ = root_view.get();
  root_view_->SetTitle(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_TITLE));
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root_view), views::MenuRunner::CONTEXT_MENU |
                                views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                                views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(
      /*parent=*/nullptr, /*button_controller=*/nullptr, anchor_rect,
      views::MenuAnchorPosition::kBubbleBottomRight, source_type);
}

bool ClipboardHistoryMenuModelAdapter::IsRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ClipboardHistoryMenuModelAdapter::Cancel(bool will_paste_item) {
  model_->set_will_paste_item(will_paste_item);
  DCHECK(menu_runner_);
  menu_runner_->Cancel();
}

std::optional<int> ClipboardHistoryMenuModelAdapter::GetFirstMenuItemCommand() {
  if (item_views_by_command_id_.empty()) {
    return std::nullopt;
  }

  return base::ranges::min(item_views_by_command_id_, /*comp=*/{},
                           /*proj=*/[](const auto& kv) { return kv.first; })
      .first;
}

std::optional<int>
ClipboardHistoryMenuModelAdapter::GetSelectedMenuItemCommand() const {
  DCHECK(root_view_);

  // `root_view_` may be selected if no menu item is under selection.
  auto* menu_item = root_view_->GetMenuController()->GetSelectedMenuItem();
  return menu_item && menu_item != root_view_
             ? std::make_optional(menu_item->GetCommand())
             : std::nullopt;
}

const ClipboardHistoryItem&
ClipboardHistoryMenuModelAdapter::GetItemFromCommandId(int command_id) const {
  auto iter = item_snapshots_.find(command_id);
  DCHECK(iter != item_snapshots_.cend());
  return iter->second;
}

size_t ClipboardHistoryMenuModelAdapter::GetMenuItemsCount() const {
  // We should not use `root_view_` to retrieve the item count. Because the
  // menu item view is removed from `root_view_` asynchronously.
  return item_views_by_command_id_.size();
}

void ClipboardHistoryMenuModelAdapter::SelectMenuItemWithCommandId(
    int command_id) {
  views::MenuItemView* selected_menu_item =
      root_view_->GetMenuItemByID(command_id);
  DCHECK(IsRunning());
  views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
      selected_menu_item);
}

void ClipboardHistoryMenuModelAdapter::SelectMenuItemHoveredByMouse() {
  // Find the menu item hovered by mouse.
  auto iter = base::ranges::find_if(item_views_by_command_id_,
                                    &views::View::IsMouseHovered,
                                    &ItemViewsByCommandId::value_type::second);

  if (iter == item_views_by_command_id_.cend()) {
    // If no item is hovered by mouse, cancel the selection on the child menu
    // item by selecting the root menu item.
    views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
        root_view_);
  } else {
    SelectMenuItemWithCommandId(iter->first);
  }
}

void ClipboardHistoryMenuModelAdapter::RemoveMenuItemWithCommandId(
    int command_id) {
  // Calculate `new_selected_command_id` before removing the item specified by
  // `command_id` from data structures because the item to be removed is
  // needed in calculation.
  std::optional<int> new_selected_command_id =
      CalculateSelectedCommandIdAfterDeletion(command_id);

  // Disable a11y for all item views. It ensures that when deleting multiple
  // item views, only the one finally selected is announced.
  if (!item_deletion_in_progress_count_) {
    DCHECK(!scoped_ignore_);
    scoped_ignore_ = std::make_unique<ScopedA11yIgnore>(this);
  }

  // Update the menu item selection.
  if (new_selected_command_id.has_value()) {
    SelectMenuItemWithCommandId(*new_selected_command_id);
  } else {
    views::MenuController::GetActiveInstance()->SelectItemAndOpenSubmenu(
        root_view_);
  }

  auto item_view_to_delete_iter = item_views_by_command_id_.find(command_id);
  DCHECK(item_view_to_delete_iter != item_views_by_command_id_.cend());

  views::View* item_view_to_delete = item_view_to_delete_iter->second;

  // Configure `item_view_to_delete` to serve a11y features.
  views::ViewAccessibility& view_accessibility =
      item_view_to_delete->GetViewAccessibility();

  // Polish the a11y announcement for deletion operation.
  view_accessibility.SetDescription(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_ITEM_DELETION));

  // Enable a11y announcement for the view to be deleted.
  view_accessibility.SetIsIgnored(false);

  // Disabling `item_view_to_delete` is more like implementation details.
  // So do not expose it to users.
  view_accessibility.SetIsEnabled(true);

  // Specify `item_view_to_delete`'s position in the set. Without updating the
  // position in set and set size, the menu's size after deletion may be
  // announced.
  const int pos_in_set = std::distance(item_views_by_command_id_.begin(),
                                       item_view_to_delete_iter) +
                         1;
  view_accessibility.SetPosInSet(pos_in_set);
  view_accessibility.SetSetSize(item_views_by_command_id_.size());

  // Disable views to be removed in order to prevent them from handling
  // events.
  root_view_->GetMenuItemByID(command_id)->SetEnabled(false);
  item_view_to_delete->SetEnabled(false);

  item_views_by_command_id_.erase(item_view_to_delete_iter);

  auto item_to_delete = item_snapshots_.find(command_id);
  DCHECK(item_to_delete != item_snapshots_.end());
  item_snapshots_.erase(item_to_delete);

  // The current selected menu item may be accessed after item deletion. So
  // postpone the menu item deletion.
  ++item_deletion_in_progress_count_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHistoryMenuModelAdapter::RemoveItemView,
                     weak_ptr_factory_.GetWeakPtr(), command_id));
}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocus(bool reverse) {
  std::optional<int> selected_command = GetSelectedMenuItemCommand();

  // If no item is selected, select the topmost or bottom menu item depending
  // on the focus move direction.
  if (!selected_command.has_value()) {
    SelectMenuItemWithCommandId(
        reverse ? item_views_by_command_id_.rbegin()->first
                : clipboard_history_util::kFirstItemCommandId);
    return;
  }

  AdvancePseudoFocusFromSelectedItem(reverse);
}

clipboard_history_util::Action
ClipboardHistoryMenuModelAdapter::GetActionForCommandId(int command_id) const {
  auto selected_item_iter = item_views_by_command_id_.find(command_id);
  DCHECK(selected_item_iter != item_views_by_command_id_.cend());

  return selected_item_iter->second->action();
}

gfx::Rect ClipboardHistoryMenuModelAdapter::GetMenuBoundsInScreenForTest()
    const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetBoundsInScreen();
}

const views::MenuItemView*
ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(size_t index) const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetMenuItemAt(index);
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::GetMenuItemViewAtForTest(
    size_t index) {
  return const_cast<views::MenuItemView*>(
      const_cast<const ClipboardHistoryMenuModelAdapter*>(this)
          ->GetMenuItemViewAtForTest(index));
}

const ui::SimpleMenuModel* ClipboardHistoryMenuModelAdapter::GetModelForTest()
    const {
  return model_.get();
}

ClipboardHistoryMenuModelAdapter::ClipboardHistoryMenuModelAdapter(
    std::unique_ptr<MenuModelWithWillCloseCallback> model,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history)
    : views::MenuModelAdapter(model.get(), std::move(menu_closed_callback)),
      model_(std::move(model)),
      clipboard_history_(clipboard_history) {}

void ClipboardHistoryMenuModelAdapter::AdvancePseudoFocusFromSelectedItem(
    bool reverse) {
  std::optional<int> selected_item_command = GetSelectedMenuItemCommand();
  DCHECK(selected_item_command.has_value());
  auto selected_item_iter =
      item_views_by_command_id_.find(*selected_item_command);
  DCHECK(selected_item_iter != item_views_by_command_id_.end());
  ClipboardHistoryItemView* selected_item_view = selected_item_iter->second;

  // Move the pseudo focus on the selected item view. Return early if the
  // focused view does not change.
  const bool selected_item_has_focus =
      selected_item_view->AdvancePseudoFocus(reverse);
  if (selected_item_has_focus)
    return;

  int next_selected_item_command = -1;
  ClipboardHistoryItemView* next_focused_view = nullptr;

  if (reverse) {
    auto next_focused_item_iter =
        selected_item_iter == item_views_by_command_id_.begin()
            ? item_views_by_command_id_.rbegin()
            : std::make_reverse_iterator(selected_item_iter);
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  } else {
    auto next_focused_item_iter = std::next(selected_item_iter, 1);
    if (next_focused_item_iter == item_views_by_command_id_.end())
      next_focused_item_iter = item_views_by_command_id_.begin();
    next_selected_item_command = next_focused_item_iter->first;
    next_focused_view = next_focused_item_iter->second;
  }

  // Advancing pseudo focus should precede the item selection. Because when an
  // item view is selected, the selected view does not overwrite its pseudo
  // focus if its pseudo focus is non-empty. It can ensure that the pseudo
  // focus and the corresponding UI appearance update only once.
  next_focused_view->AdvancePseudoFocus(reverse);
  SelectMenuItemWithCommandId(next_selected_item_command);
}

int ClipboardHistoryMenuModelAdapter::CalculateSelectedCommandIdAfterDeletion(
    int command_id) const {
  // If the menu item view to be deleted is the last one, Cancel()
  // should be called so this function should not be hit.
  DCHECK_GT(item_snapshots_.size(), 1u);

  auto item_to_delete = item_snapshots_.find(command_id);
  DCHECK(item_to_delete != item_snapshots_.cend());

  // Use the menu item right after the one to be deleted if any. Otherwise,
  // select the previous one.

  auto next_item_iter = item_to_delete;
  ++next_item_iter;
  if (next_item_iter != item_snapshots_.cend())
    return next_item_iter->first;

  auto previous_item_iter = item_to_delete;
  --previous_item_iter;
  return previous_item_iter->first;
}

void ClipboardHistoryMenuModelAdapter::RemoveItemView(int command_id) {
  std::optional<int> original_selected_command_id =
      GetSelectedMenuItemCommand();

  // The menu item view and its corresponding command should be removed at the
  // same time. Otherwise, it may run into check errors.
  model_->RemoveItemAt(model_->GetIndexOfCommandId(command_id).value());
  root_view_->RemoveMenuItem(root_view_->GetMenuItemByID(command_id));
  if (const auto first_item_command_id = GetFirstMenuItemCommand();
      first_item_command_id &&
      chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    item_views_by_command_id_[*first_item_command_id]->ShowCtrlVLabel();
  }
  root_view_->ChildrenChanged();

  --item_deletion_in_progress_count_;
  // Re-enable a11y for all item views when item deletion finally completes.
  if (!item_deletion_in_progress_count_) {
    DCHECK(scoped_ignore_);
    scoped_ignore_.reset();
  }

  // `ChildrenChanged()` clears the selection. So restore the selection.
  if (original_selected_command_id.has_value())
    SelectMenuItemWithCommandId(*original_selected_command_id);
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    size_t index) {
  const int command_id = model->GetCommandIdAt(index);

  views::MenuItemView* container = menu->AppendMenuItem(command_id);

  // Ignore `container` in accessibility events handling. Let `item_view`
  // handle.
  container->GetViewAccessibility().SetIsIgnored(true);

  // Margins are managed by `ClipboardHistoryItemView`.
  container->set_vertical_margin(0);

  if (header_index_ == index) {
    CHECK_EQ(model->GetTypeAt(index), ui::MenuModel::ItemType::TYPE_TITLE);
    InsertHeaderContent(container);
  } else if (footer_index_ == index) {
    CHECK_EQ(model->GetTypeAt(index), ui::MenuModel::ItemType::TYPE_TITLE);
    InsertFooterContent(container, menu_show_source_.value());
  } else {
    CHECK_EQ(model->GetTypeAt(index), ui::MenuModel::ItemType::TYPE_COMMAND);
    std::unique_ptr<ClipboardHistoryItemView> item_view =
        ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
            GetItemFromCommandId(command_id).id(), clipboard_history_,
            container);
    if (chromeos::features::IsClipboardHistoryRefreshEnabled() &&
        command_id == clipboard_history_util::kFirstItemCommandId) {
      item_view->ShowCtrlVLabel();
    }
    item_views_by_command_id_.insert(
        std::make_pair(command_id, item_view.get()));
    container->AddChildView(std::move(item_view));
  }

  return container;
}

void ClipboardHistoryMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  // Terminate alive asynchronous calls on `RemoveItemView()`. It is pointless
  // to update views when the menu is closed.
  // Note that data members related to the asynchronous calls, such as
  // `item_deletion_in_progress_count_` and `scoped_ignore_`, are not reset.
  // Because when hitting here, this instance is going to be destructed soon.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // This factory may be nullptr in tests.
  if (auto* clipboard_image_factory = ClipboardImageModelFactory::Get()) {
    clipboard_image_factory->Deactivate();
  }
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - menu_open_time_;
  UMA_HISTOGRAM_TIMES("Ash.ClipboardHistory.ContextMenu.UserJourneyTime",
                      user_journey_time);
  views::MenuModelAdapter::OnMenuClosed(menu);
  item_views_by_command_id_.clear();

  // This implementation of MenuModelAdapter does not have a widget so we need
  // to manually notify the accessibility side of the closed menu.
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* active_widget =
      views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(active_widget);
  views::View* focused_view =
      active_widget->GetFocusManager()->GetFocusedView();
  if (focused_view) {
    focused_view->NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd,
                                           /*send_native_event=*/true);
  }
}

}  // namespace ash
