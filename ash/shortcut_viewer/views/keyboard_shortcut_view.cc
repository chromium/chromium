// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/keyboard_shortcut_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/app_types.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_view_base.h"
#include "ash/shell.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ash/shortcut_viewer/views/keyboard_shortcut_item_list_view.h"
#include "ash/shortcut_viewer/views/keyboard_shortcut_item_view.h"
#include "ash/shortcut_viewer/views/ksv_search_box_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/functional/bind.h"
#include "base/i18n/string_search.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/ash/keyboard_layout_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

KeyboardShortcutView* g_ksv_view = nullptr;

constexpr std::nullopt_t kAllCategories = std::nullopt;

// Light mode colors:
constexpr SkColor kSearchIllustrationIconColorLight =
    SkColorSetARGB(0xFF, 0xDA, 0xDC, 0xE0);

constexpr SkColor kSearchIllustrationIconColorDark =
    SkColorSetARGB(0xFF, 0x3C, 0x40, 0x43);

// Custom No Results image view to handle color theme changes.
class KSVNoResultsImageView : public views::ImageView {
  METADATA_HEADER(KSVNoResultsImageView, views::ImageView)

 public:
  KSVNoResultsImageView()
      : dark_light_mode_controller_(ash::DarkLightModeControllerImpl::Get()) {}

  KSVNoResultsImageView(const KSVNoResultsImageView&) = delete;
  KSVNoResultsImageView operator=(const KSVNoResultsImageView&) = delete;

  ~KSVNoResultsImageView() override = default;

 protected:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();

    if (dark_light_mode_controller_->IsDarkModeEnabled()) {
      SetImage(gfx::CreateVectorIcon(ash::kKsvSearchNoResultDarkIcon,
                                     kSearchIllustrationIconColorDark));
    } else {
      SetImage(gfx::CreateVectorIcon(ash::kKsvSearchNoResultLightIcon,
                                     kSearchIllustrationIconColorLight));
    }
  }

 private:
  const raw_ptr<ash::DarkLightModeControllerImpl, ExperimentalAsh>
      dark_light_mode_controller_;
};

BEGIN_METADATA(KSVNoResultsImageView)
END_METADATA

// Creates the no search result view.
std::unique_ptr<views::View> CreateNoSearchResultView() {
  constexpr int kSearchIllustrationIconSize = 150;
  auto* color_provider = ash::ColorProvider::Get();

  auto illustration_view = std::make_unique<views::View>();
  constexpr int kTopPadding = 98;
  views::BoxLayout* layout =
      illustration_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(kTopPadding, 0, 0, 0)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  auto image_view = std::make_unique<KSVNoResultsImageView>();
  image_view->SetImage(gfx::CreateVectorIcon(
      ash::kKsvSearchNoResultLightIcon,
      color_provider->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kIconColorPrimary)));
  image_view->SetImageSize(
      gfx::Size(kSearchIllustrationIconSize, kSearchIllustrationIconSize));
  illustration_view->AddChildView(std::move(image_view));

  auto text = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_NO_RESULT));
  text->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
  constexpr int kLabelFontSizeDelta = 1;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  text->SetFontList(rb.GetFontListWithDelta(kLabelFontSizeDelta));
  illustration_view->AddChildView(std::move(text));
  return illustration_view;
}

class ShortcutsListScrollView : public views::ScrollView {
 public:
  ShortcutsListScrollView() {
    SetAccessibilityProperties(
        ax::mojom::Role::kScrollView,
        l10n_util::GetStringUTF16(IDS_KSV_SCROLL_VIEW_ACCESSIBILITY_NAME));
  }

  ShortcutsListScrollView(const ShortcutsListScrollView&) = delete;
  ShortcutsListScrollView& operator=(const ShortcutsListScrollView&) = delete;

  ~ShortcutsListScrollView() override = default;

  // views::View:
  void OnFocus() override {
    SetHasFocusIndicator(true);
    views::ScrollView::OnFocus();
  }

  void OnThemeChanged() override {
    views::ScrollView::OnThemeChanged();

    SetBackgroundColor(GetColorProvider()->GetColor(cros_tokens::kBgColor));
  }

  void OnBlur() override { SetHasFocusIndicator(false); }
};

std::unique_ptr<ShortcutsListScrollView> CreateScrollView(
    std::unique_ptr<views::View> content_view) {
  auto scroller = std::make_unique<ShortcutsListScrollView>();
  scroller->SetDrawOverflowIndicator(false);
  scroller->ClipHeightTo(0, 0);
  scroller->SetContents(std::move(content_view));
  scroller->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  return scroller;
}

void UpdateAXNodeDataPosition(
    std::vector<KeyboardShortcutItemView*>& shortcut_items) {
  // Update list item AXNodeData position for assistive tool.
  const int number_shortcut_items = shortcut_items.size();
  for (int i = 0; i < number_shortcut_items; ++i) {
    shortcut_items.at(i)->GetViewAccessibility().OverridePosInSet(
        i + 1, number_shortcut_items);
  }
}

// Returns true if the given |item| should be excluded from the view, since
// certain shortcuts can be associated with a disabled feature behind a flag,
// or specific device property, e.g. keyboard layout.
bool ShouldExcludeItem(const ash::KeyboardShortcutItem& item) {
  switch (item.description_message_id) {
    case IDS_KSV_DESCRIPTION_OPEN_GOOGLE_ASSISTANT:
      return ui::DeviceKeyboardHasAssistantKey();
    case IDS_KSV_DESCRIPTION_PRIVACY_SCREEN_TOGGLE:
      return !ash::Shell::Get()->privacy_screen_controller()->IsSupported();
  }

  return false;
}

}  // namespace

KeyboardShortcutView::~KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, this);
  g_ksv_view = nullptr;
}

// static
views::Widget* KeyboardShortcutView::Toggle(aura::Window* context) {
  if (g_ksv_view) {
    if (g_ksv_view->GetWidget()->IsActive())
      g_ksv_view->GetWidget()->Close();
    else
      g_ksv_view->GetWidget()->Activate();
  } else {
    TRACE_EVENT0("shortcut_viewer", "CreateWidget");
    base::RecordAction(
        base::UserMetricsAction("KeyboardShortcutViewer.CreateWindow"));

    views::Widget::InitParams params;
    params.delegate = new KeyboardShortcutView;
    params.name = "KeyboardShortcutWidget";
    // Intentionally don't set bounds. The window will be sized and centered
    // based on CalculatePreferredSize().
    views::Widget* widget = new views::Widget;
    params.context = context;
    params.init_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::SYSTEM_APP));
    widget->Init(std::move(params));

    // Set frame view Active and Inactive colors, both are SK_ColorWHITE.
    aura::Window* window = g_ksv_view->GetWidget()->GetNativeWindow();
    g_ksv_view->UpdateActiveAndInactiveFrameColor();

    // Set shelf icon.
    const ash::ShelfID shelf_id(ash::kInternalAppIdKeyboardShortcutViewer);
    window->SetProperty(
        ash::kAppIDKey,
        new std::string(ash::kInternalAppIdKeyboardShortcutViewer));
    window->SetProperty(ash::kShelfIDKey,
                        new std::string(shelf_id.Serialize()));
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

    // We don't want the KSV window to have a title (per design), however the
    // shelf uses the window title to set the shelf item's tooltip text. The
    // shelf observes changes to the |kWindowIconKey| property and handles that
    // by initializing the shelf item including its tooltip text.
    // TODO(wutao): we can remove resource id IDS_KSV_TITLE after implementing
    // internal app shelf launcher.
    window->SetTitle(l10n_util::GetStringUTF16(IDS_KSV_TITLE));
    gfx::ImageSkia* icon =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_SHORTCUT_VIEWER_LOGO_192);
    // The new gfx::ImageSkia instance is owned by the window itself.
    window->SetProperty(aura::client::kWindowIconKey,
                        new gfx::ImageSkia(*icon));

    g_ksv_view->AddAccelerator(
        ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
    g_ksv_view->AddAccelerator(
        ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));

    g_ksv_view->needs_init_all_categories_ = false;
    g_ksv_view->did_first_paint_ = false;
    g_ksv_view->GetWidget()->Show();
    g_ksv_view->search_box_view_->search_box()->RequestFocus();
  }
  return g_ksv_view->GetWidget();
}

std::u16string KeyboardShortcutView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_KSV_TITLE);
}

bool KeyboardShortcutView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const bool is_valid_modifier =
      accelerator.modifiers() == ui::EF_CONTROL_DOWN ||
      accelerator.modifiers() == (ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  DCHECK(is_valid_modifier);
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());

  GetWidget()->Close();
  return true;
}

void KeyboardShortcutView::Layout() {
  gfx::Rect content_bounds(GetContentsBounds());
  if (content_bounds.IsEmpty()) {
    return;
  }

  constexpr int kSearchBoxTopPadding = 8;
  constexpr int kSearchBoxBottomPadding = 16;
  constexpr int kSearchBoxHorizontalPadding = 30;
  const int left = content_bounds.x();
  const int top = content_bounds.y();
  gfx::Rect search_box_bounds(search_box_view_->GetPreferredSize());
  search_box_bounds.set_width(
      std::min(search_box_bounds.width(),
               content_bounds.width() - 2 * kSearchBoxHorizontalPadding));
  search_box_bounds.set_x(
      left + (content_bounds.width() - search_box_bounds.width()) / 2);
  search_box_bounds.set_y(top + kSearchBoxTopPadding);
  search_box_view_->SetBoundsRect(search_box_bounds);

  views::View* content_view = categories_tabbed_pane_->GetVisible()
                                  ? categories_tabbed_pane_.get()
                                  : search_results_container_.get();
  const int search_box_used_height = search_box_bounds.height() +
                                     kSearchBoxTopPadding +
                                     kSearchBoxBottomPadding;
  content_view->SetBounds(left, top + search_box_used_height,
                          content_bounds.width(),
                          content_bounds.height() - search_box_used_height);
}

gfx::Size KeyboardShortcutView::CalculatePreferredSize() const {
  return gfx::Size(800, 512);
}

void KeyboardShortcutView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Skip if it is the first OnPaint event.
  if (!did_first_paint_) {
    did_first_paint_ = true;
    needs_init_all_categories_ = true;
    return;
  }

  if (!needs_init_all_categories_) {
    return;
  }

  needs_init_all_categories_ = false;
  // Cannot post a task right after initializing the first category, it will
  // have a chance to end up in the same group of drawing commands sent to
  // compositor. We can wait for the second OnPaint, which means previous
  // drawing commands have been sent to compositor for the next frame and new
  // coming commands will be sent for the next-next frame.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&KeyboardShortcutView::InitCategoriesTabbedPane,
                                weak_factory_.GetWeakPtr(), kAllCategories));
}

void KeyboardShortcutView::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();

  UpdateBackgroundColor();
  UpdateActiveAndInactiveFrameColor();
}

void KeyboardShortcutView::QueryChanged(const std::u16string& query) {
  std::u16string trimmed_query;
  base::TrimWhitespace(query, base::TRIM_ALL, &trimmed_query);

  const bool query_empty = trimmed_query.empty();
  if (is_search_box_empty_ != query_empty) {
    is_search_box_empty_ = query_empty;
    UpdateViewsLayout();
  }

  debounce_timer_.Stop();
  // If search box is empty, do not show |search_results_container_|.
  if (query_empty) {
    return;
  }

  // TODO(wutao): This timeout value is chosen based on subjective search
  // latency tests on Minnie. Objective method or UMA is desired.
  constexpr base::TimeDelta kTimeOut(base::Milliseconds(250));
  debounce_timer_.Start(FROM_HERE, kTimeOut,
                        base::BindOnce(&KeyboardShortcutView::ShowSearchResults,
                                       base::Unretained(this), query));
}

KeyboardShortcutView::KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, nullptr);
  g_ksv_view = this;

  SetCanMinimize(true);
  SetShowTitle(false);

  InitViews();
}

void KeyboardShortcutView::InitViews() {
  TRACE_EVENT0("shortcut_viewer", "InitViews");
  // Init search box view.
  auto search_box_view = std::make_unique<KSVSearchBoxView>(base::BindRepeating(
      &KeyboardShortcutView::QueryChanged, base::Unretained(this)));
  search_box_view->Initialize();
  search_box_view_ = AddChildView(std::move(search_box_view));

  // Init no search result illustration view.
  search_no_result_view_ = CreateNoSearchResultView();
  search_no_result_view_->set_owned_by_client();

  // Init search results container view.
  search_results_container_ = AddChildView(std::make_unique<views::View>());
  search_results_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  search_results_container_->SetVisible(false);

  // Init views of KeyboardShortcutItemView.
  // TODO(https://crbug.com/843394): Observe changes in keyboard layout and
  // clear the cache.
  KeyboardShortcutItemView::ClearKeycodeToString16Cache();
  for (const auto& item : GetKeyboardShortcutItemList()) {
    if (ShouldExcludeItem(item)) {
      continue;
    }

    for (auto category : item.categories) {
      shortcut_views_.push_back(
          std::make_unique<KeyboardShortcutItemView>(item, category));
      shortcut_views_.back()->set_owned_by_client();
    }
  }
  std::sort(shortcut_views_.begin(), shortcut_views_.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs->category() != rhs->category())
                return lhs->category() < rhs->category();
              return lhs->description_label_view()->GetText() <
                     rhs->description_label_view()->GetText();
            });

  // Init views of |categories_tabbed_pane_| and KeyboardShortcutItemListViews.
  categories_tabbed_pane_ = AddChildView(std::make_unique<views::TabbedPane>(
      views::TabbedPane::Orientation::kVertical,
      views::TabbedPane::TabStripStyle::kHighlight));

  // Initial Layout of KeyboardShortcutItemView is time consuming. To speed up
  // the startup time, we only initialize the first category pane, which is
  // visible to user, and defer initialization of other categories in the
  // background.
  InitCategoriesTabbedPane(ash::ShortcutCategory::kPopular);
}

void KeyboardShortcutView::InitCategoriesTabbedPane(
    std::optional<ash::ShortcutCategory> initial_category) {
  active_tab_index_ = categories_tabbed_pane_->GetSelectedTabIndex();
  // If the tab count is 0, GetSelectedTabIndex() will return kNoSelectedTab,
  // which we do not want to cache.
  if (active_tab_index_ == views::TabbedPaneTabStrip::kNoSelectedTab) {
    active_tab_index_ = 0;
  }

  ash::ShortcutCategory current_category = ash::ShortcutCategory::kUnknown;
  KeyboardShortcutItemListView* item_list_view = nullptr;
  std::vector<KeyboardShortcutItemView*> shortcut_items;
  const bool already_has_tabs = categories_tabbed_pane_->GetTabCount() > 0;
  size_t tab_index = 0;
  views::View* const tab_contents = categories_tabbed_pane_->children()[1];
  for (const auto& item_view : shortcut_views_) {
    const ash::ShortcutCategory category = item_view->category();
    DCHECK_NE(ash::ShortcutCategory::kUnknown, category);
    if (current_category != category) {
      current_category = category;
      std::unique_ptr<views::View> content_view;
      // Delay constructing a KeyboardShortcutItemListView until it is needed.
      if (initial_category.value_or(category) == category) {
        auto list_view = std::make_unique<KeyboardShortcutItemListView>();
        item_list_view = list_view.get();

        // When in a new category, update the node data of the shortcut items in
        // previous category and clear the vector in order to store items in
        // current category.
        UpdateAXNodeDataPosition(shortcut_items);
        shortcut_items.clear();

        content_view = std::move(list_view);
      } else {
        content_view = std::make_unique<views::View>();
      }

      // Create new tabs or update the existing tabs' contents.
      if (already_has_tabs) {
        auto* scroll_view = static_cast<views::ScrollView*>(
            tab_contents->children()[tab_index]);
        scroll_view->SetContents(std::move(content_view));
      } else {
        categories_tabbed_pane_->AddTab(
            GetStringForCategory(current_category),
            CreateScrollView(std::move(content_view)));
      }

      ++tab_index;
    }

    // If |initial_category| has a value, we only initialize the pane with the
    // KeyboardShortcutItemView in the specific category in |initial_category|.
    // Otherwise, we will initialize all the panes.
    if (initial_category.value_or(category) != category) {
      continue;
    }

    // Add the item to the category contents container.
    if (!item_list_view->children().empty())
      item_list_view->AddHorizontalSeparator();
    views::StyledLabel* description_label_view =
        item_view->description_label_view();
    // Clear any styles used to highlight matched search query in search mode.
    description_label_view->ClearStyleRanges();
    item_list_view->AddChildView(item_view.get());
    shortcut_items.push_back(item_view.get());
    // Remove the search query highlight.
    description_label_view->InvalidateLayout();
  }
  // Update node data for the last category.
  UpdateAXNodeDataPosition(shortcut_items);

  tab_contents->InvalidateLayout();
}

void KeyboardShortcutView::UpdateViewsLayout() {
  // 1. Search box is empty: show |categories_tabbed_pane_| and focus on
  //    active tab.
  // 2. Search box is not empty, show |search_results_container_|.
  const bool should_show_search_results = !is_search_box_empty_;
  if (!should_show_search_results) {
    // Remove all child views, including horizontal separator lines, to prepare
    // for showing search results next time.
    search_results_container_->RemoveAllChildViews();
    if (!categories_tabbed_pane_->GetVisible()) {
      // Repopulate |categories_tabbed_pane_| child views, which were removed
      // when they were added to |search_results_container_|.
      InitCategoriesTabbedPane(kAllCategories);
      // Select the category that was active before entering search mode.
      categories_tabbed_pane_->SelectTabAt(active_tab_index_);
    }
  }
  categories_tabbed_pane_->SetVisible(!should_show_search_results);
  search_results_container_->SetVisible(should_show_search_results);
  InvalidateLayout();
}

void KeyboardShortcutView::ShowSearchResults(
    const std::u16string& search_query) {
  search_results_container_->RemoveAllChildViews();
  auto* search_container_content_view = search_no_result_view_.get();
  auto found_items_list_view = std::make_unique<KeyboardShortcutItemListView>();
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents finder(
      search_query);
  ash::ShortcutCategory current_category = ash::ShortcutCategory::kUnknown;
  bool has_category_item = false;
  found_shortcut_items_.clear();

  for (const auto& item_view : shortcut_views_) {
    std::u16string description_text =
        item_view->description_label_view()->GetText();
    std::u16string shortcut_text = item_view->shortcut_label_view()->GetText();
    size_t match_index = -1;
    size_t match_length = 0;
    // Only highlight |description_label_view_| in KeyboardShortcutItemView.
    // |shortcut_label_view_| has customized style ranges for bubble views
    // so it may have overlappings with the searched ranges. The highlighted
    // behaviors are not defined so we don't highlight
    // |shortcut_label_view_|.
    if (finder.Search(description_text, &match_index, &match_length) ||
        finder.Search(shortcut_text, nullptr, nullptr)) {
      const ash::ShortcutCategory category = item_view->category();
      if (current_category != category) {
        current_category = category;
        has_category_item = false;
        found_items_list_view->AddCategoryLabel(GetStringForCategory(category));
      }
      if (has_category_item)
        found_items_list_view->AddHorizontalSeparator();
      else
        has_category_item = true;
      // Highlight matched query in |description_label_view_|.
      if (match_length > 0) {
        views::StyledLabel::RangeStyleInfo style;
        views::StyledLabel* description_label_view =
            item_view->description_label_view();
        // Clear previous styles.
        description_label_view->ClearStyleRanges();
        style.text_style = views::style::STYLE_EMPHASIZED;
        description_label_view->AddStyleRange(
            gfx::Range(match_index, match_index + match_length), style);
        // Apply new styles to highlight matched search query.
        description_label_view->InvalidateLayout();
      }

      found_items_list_view->AddChildView(item_view.get());
      found_shortcut_items_.push_back(item_view.get());
    }
  }

  std::vector<std::u16string> replacement_strings;
  const int number_search_results = found_shortcut_items_.size();
  if (!found_items_list_view->children().empty()) {
    UpdateAXNodeDataPosition(found_shortcut_items_);
    replacement_strings.push_back(
        base::NumberToString16(number_search_results));

    // To offset the padding between the bottom of the |search_box_view_| and
    // the top of the |search_results_container_|.
    constexpr int kTopPadding = -16;
    constexpr int kHorizontalPadding = 128;
    found_items_list_view->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kTopPadding, kHorizontalPadding, 0, kHorizontalPadding)));
    search_container_content_view =
        CreateScrollView(std::move(found_items_list_view)).release();
  }
  replacement_strings.push_back(search_query);
  search_box_view_->SetAccessibleValue(l10n_util::GetStringFUTF16(
      number_search_results == 0
          ? IDS_KSV_SEARCH_BOX_ACCESSIBILITY_VALUE_WITHOUT_RESULTS
          : IDS_KSV_SEARCH_BOX_ACCESSIBILITY_VALUE_WITH_RESULTS,
      replacement_strings, nullptr));
  search_results_container_->AddChildView(search_container_content_view);
  InvalidateLayout();
}

views::ClientView* KeyboardShortcutView::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, this);
}

KeyboardShortcutView* KeyboardShortcutView::GetInstanceForTesting() {
  return g_ksv_view;
}

size_t KeyboardShortcutView::GetTabCountForTesting() const {
  return categories_tabbed_pane_->GetTabCount();
}

const std::vector<std::unique_ptr<KeyboardShortcutItemView>>&
KeyboardShortcutView::GetShortcutViewsForTesting() const {
  return shortcut_views_;
}

KSVSearchBoxView* KeyboardShortcutView::GetSearchBoxViewForTesting() {
  return search_box_view_;
}

const std::vector<KeyboardShortcutItemView*>&
KeyboardShortcutView::GetFoundShortcutItemsForTesting() const {
  return found_shortcut_items_;
}

void KeyboardShortcutView::UpdateBackgroundColor() {
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(cros_tokens::kBgColor)));
}

void KeyboardShortcutView::UpdateActiveAndInactiveFrameColor() {
  aura::Window* window = g_ksv_view->GetWidget()->GetNativeWindow();
  window->SetProperty(chromeos::kTrackDefaultFrameColors,
                      /*value=*/false);
  const SkColor background_color =
      GetColorProvider()->GetColor(cros_tokens::kBgColor);
  window->SetProperty(chromeos::kFrameActiveColorKey, background_color);
  window->SetProperty(chromeos::kFrameInactiveColorKey, background_color);
}

BEGIN_METADATA(KeyboardShortcutView, views::WidgetDelegateView)
END_METADATA

}  // namespace keyboard_shortcut_viewer

namespace ash {

void ToggleKeyboardShortcutViewer() {
  keyboard_shortcut_viewer::KeyboardShortcutView::Toggle(nullptr);
}

}  // namespace ash
