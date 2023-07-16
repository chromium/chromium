// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/launcher_search_iph_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view_delegate.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/user_education/user_education_class_properties.h"
#include "base/containers/contains.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kSearchBoxFocusRingWidth = 2;

// Padding between the focus ring and the search box view
constexpr int kSearchBoxFocusRingPadding = 4;

constexpr int kSearchBoxFocusRingCornerRadius = 28;

// Minimum amount of characters required to enable autocomplete.
constexpr int kMinimumLengthToAutocomplete = 2;

// Border insets for SearchBoxView in bubble launcher.
constexpr auto kBorderInsetsForAppListBubble = gfx::Insets::TLBR(4, 4, 4, 0);

// Margins for the search box text field in bubble launcher.
constexpr auto kTextFieldMarginsForAppListBubble =
    gfx::Insets::TLBR(8, 0, 0, 0);

// The default PlaceholderTextTypes used for productivity launcher. Randomly
// selected when placeholder text would be shown.
constexpr SearchBoxView::PlaceholderTextType kDefaultPlaceholders[] = {
    SearchBoxView::PlaceholderTextType::kShortcuts,
    SearchBoxView::PlaceholderTextType::kTabs,
    SearchBoxView::PlaceholderTextType::kSettings,
    SearchBoxView::PlaceholderTextType::kImages,
};

// PlaceholderTextTypes used for productivity launcher for cloud gaming devices.
// Randomly selected when placeholder text would be shown.
constexpr SearchBoxView::PlaceholderTextType kGamingPlaceholders[4] = {
    SearchBoxView::PlaceholderTextType::kShortcuts,
    SearchBoxView::PlaceholderTextType::kTabs,
    SearchBoxView::PlaceholderTextType::kSettings,
    SearchBoxView::PlaceholderTextType::kGames,
};

constexpr gfx::RoundedCornersF kAssistantButtonBackgroundRadiiLTR = {
    18,
    18,
    4,
    18,
};

constexpr gfx::RoundedCornersF kAssistantButtonBackgroundRadiiRTL = {
    18,
    18,
    18,
    4,
};

bool IsTrimmedQueryEmpty(const std::u16string& query) {
  std::u16string trimmed_query;
  base::TrimWhitespace(query, base::TrimPositions::TRIM_ALL, &trimmed_query);
  return trimmed_query.empty();
}

std::u16string GetCategoryName(SearchResult* search_result) {
  switch (search_result->category()) {
    case ash::AppListSearchResultCategory::kApps:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_APPS);
    case ash::AppListSearchResultCategory::kAppShortcuts:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_APP_SHORTCUTS);
    case ash::AppListSearchResultCategory::kWeb:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_WEB);
    case ash::AppListSearchResultCategory::kFiles:
      return (l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_FILES));
    case ash::AppListSearchResultCategory::kSettings:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_SETTINGS);
    case ash::AppListSearchResultCategory::kHelp:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_HELP);
    case ash::AppListSearchResultCategory::kPlayStore:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_PLAY_STORE);
    case ash::AppListSearchResultCategory::kSearchAndAssistant:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_SEARCH_AND_ASSISTANT);
    case ash::AppListSearchResultCategory::kGames:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_GAMES);
    case ash::AppListSearchResultCategory::kUnknown:
      return std::u16string();
  }
}

bool IsSubstringCaseInsensitive(std::u16string haystack_expr,
                                std::u16string needle_expr) {
  // Convert complete given String to lower case
  std::u16string haystack = base::i18n::ToLower(haystack_expr);
  // Convert complete given Sub String to lower case
  std::u16string needle = base::i18n::ToLower(needle_expr);

  // Find substring in the given string
  return base::Contains(haystack, needle);
}

void RecordAutocompleteMatchMetric(SearchBoxTextMatch match_type) {
  base::UmaHistogramEnumeration("Apps.AppListSearchAutocomplete", match_type);
}

ui::ColorId GetFocusColorId(bool use_jelly_colors) {
  return use_jelly_colors
             ? static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing)
             : ui::kColorAshFocusRing;
}

}  // namespace

class SearchBoxView::FocusRingLayer : public ui::LayerOwner, ui::LayerDelegate {
 public:
  FocusRingLayer()
      : LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)) {
    layer()->SetName("search_box/FocusRing");
    layer()->SetFillsBoundsOpaquely(false);
    layer()->set_delegate(this);
  }
  FocusRingLayer(const FocusRingLayer&) = delete;
  FocusRingLayer& operator=(const FocusRingLayer&) = delete;
  ~FocusRingLayer() override = default;

  void SetColor(SkColor color) {
    if (color == color_) {
      return;
    }
    color_ = color;
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

 private:
  // views::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer()->size());
    gfx::Canvas* canvas = recorder.canvas();

    // When using strokes to draw a rect, the bounds set is the center of the
    // rect, which means that setting draw bounds to `bounds()` will leave half
    // of the border outside the layer that may not be painted. Shrink the draw
    // bounds by half of the width to solve this problem.
    gfx::Rect draw_bounds(layer()->size());
    draw_bounds.Inset(kSearchBoxFocusRingWidth / 2);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    flags.setStrokeWidth(kSearchBoxFocusRingWidth);
    canvas->DrawRoundRect(draw_bounds, kSearchBoxFocusRingCornerRadius, flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

  SkColor color_ = gfx::kPlaceholderColor;
};

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             bool is_app_list_bubble)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      is_app_list_bubble_(is_app_list_bubble),
      is_jelly_enabled_(chromeos::features::IsJellyEnabled()) {
  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  model_provider->AddObserver(this);
  SearchBoxModel* const search_box_model =
      model_provider->search_model()->search_box();
  search_box_model_observer_.Observe(search_box_model);

  if (features::IsUserEducationEnabled()) {
    // NOTE: Set `kHelpBubbleContextKey` before `views::kElementIdentifierKey`
    // in case registration causes a help bubble to be created synchronously.
    SetProperty(kHelpBubbleContextKey, HelpBubbleContext::kAsh);
  }
  SetProperty(views::kElementIdentifierKey, kSearchBoxViewElementId);

  if (is_jelly_enabled_) {
    auto font_list = TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosBody1);
    SetPreferredStyleForSearchboxText(font_list,
                                      cros_tokens::kCrosSysOnSurface);
    SetPreferredStyleForAutocompleteText(font_list,
                                         cros_tokens::kCrosSysOnSurfaceVariant);
  }

  views::ImageButton* close_button = CreateCloseButton(base::BindRepeating(
      &SearchBoxView::CloseButtonPressed, base::Unretained(this)));
  std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
  close_button->SetAccessibleName(close_button_label);
  close_button->SetTooltipText(close_button_label);

  views::ImageButton* assistant_button =
      CreateAssistantButton(base::BindRepeating(
          &SearchBoxView::AssistantButtonPressed, base::Unretained(this)));
  assistant_button->SetFlipCanvasOnPaintForRTLUI(false);
  std::u16string assistant_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_START_ASSISTANT));
  assistant_button->SetAccessibleName(assistant_button_label);
  assistant_button->SetTooltipText(assistant_button_label);
  SetShowAssistantButton(search_box_model->show_assistant_button());

  UpdateIphViewVisibility();
}

SearchBoxView::~SearchBoxView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void SearchBoxView::InitializeForBubbleLauncher() {
  SearchBoxViewBase::InitParams params;
  params.show_close_button_when_active = false;
  params.create_background = false;
  params.animate_changing_search_icon = false;
  params.increase_child_view_padding = true;
  // Add margins to the text field because the BoxLayout vertical centering
  // does not properly align the text baseline with the icons.
  params.textfield_margins = kTextFieldMarginsForAppListBubble;

  SearchBoxViewBase::Init(params);

  UpdatePlaceholderTextAndAccessibleName();
}

void SearchBoxView::InitializeForFullscreenLauncher() {
  SearchBoxViewBase::InitParams params;
  params.show_close_button_when_active = true;
  params.create_background = true;
  params.animate_changing_search_icon = true;

  SearchBoxViewBase::Init(params);

  UpdatePlaceholderTextAndAccessibleName();
}

void SearchBoxView::SetResultSelectionController(
    ResultSelectionController* controller) {
  DCHECK(controller);
  result_selection_controller_ = controller;
}

void SearchBoxView::ResetForShow() {
  if (!is_search_box_active())
    return;
  ClearSearchAndDeactivateSearchBox();
}

void SearchBoxView::UpdateSearchTextfieldAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (a11y_active_descendant_) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                               *a11y_active_descendant_);
  }
}

void SearchBoxView::OnActiveAppListModelsChanged(AppListModel* model,
                                                 SearchModel* search_model) {
  search_box_model_observer_.Reset();
  search_box_model_observer_.Observe(search_model->search_box());

  ResetForShow();
  UpdateSearchIcon();
  ShowAssistantChanged();

  // `UpdateIphViewVisibility` expect that `AppListModelProvider` returns the
  // new model.
  CHECK(search_model == AppListModelProvider::Get()->search_model());
  UpdateIphViewVisibility();
}

void SearchBoxView::UpdateKeyboardVisibility() {
  if (!keyboard::KeyboardUIController::HasInstance())
    return;
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  bool should_show_keyboard =
      is_search_box_active() && search_box()->HasFocus();
  if (!keyboard_controller->IsEnabled() ||
      should_show_keyboard == keyboard_controller->IsKeyboardVisible()) {
    return;
  }

  if (should_show_keyboard) {
    keyboard_controller->ShowKeyboard(false);
    return;
  }

  keyboard_controller->HideKeyboardByUser();
}

void SearchBoxView::HandleQueryChange(const std::u16string& query,
                                      bool initiated_by_user) {
  // Randomly select a new placeholder text when we get an empty new query.
  if (query.empty()) {
    UpdatePlaceholderTextAndAccessibleName();
  }

  MaybeSetAutocompleteGhostText(std::u16string(), std::u16string());

  // Update autocomplete text highlight range to track user typed text.
  if (ShouldProcessAutocomplete())
    ResetHighlightRange();

  if (initiated_by_user) {
    const base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_query_.empty() && !query.empty()) {
      base::RecordAction(base::UserMetricsAction("AppList_SearchQueryStarted"));
      // Set 'user_initiated_model_update_time_' when initiating a new query.
      user_initiated_model_update_time_ = current_time;
    } else if (!current_query_.empty() && query.empty()) {
      base::RecordAction(base::UserMetricsAction("AppList_LeaveSearch"));
      // Reset 'user_initiated_model_update_time_' when clearing the search_box.
      user_initiated_model_update_time_ = base::TimeTicks();
    } else if (query != current_query_ &&
               !user_initiated_model_update_time_.is_null()) {
      if (is_app_list_bubble_) {
        UMA_HISTOGRAM_TIMES("Ash.SearchModelUpdateTime.ClamshellMode",
                            current_time - user_initiated_model_update_time_);
      } else {
        UMA_HISTOGRAM_TIMES("Ash.SearchModelUpdateTime.TabletMode",
                            current_time - user_initiated_model_update_time_);
      }
      user_initiated_model_update_time_ = current_time;
    }
  }

  std::u16string trimmed_query;
  base::TrimWhitespace(query, base::TrimPositions::TRIM_ALL, &trimmed_query);
  const bool query_empty_changed =
      trimmed_query.empty() != IsTrimmedQueryEmpty(current_query_);

  current_query_ = query;

  UpdateIphViewVisibility();

  // The search box background depens on whether the query is empty, so schedule
  // repaint when this changes.
  if (query_empty_changed)
    SchedulePaint();

  delegate_->QueryChanged(trimmed_query, initiated_by_user);

  // Don't reinitiate zero state search if the previous query was already empty
  // (to avoid issuing zero state search twice in a row while clearing up search
  // - see http://crbug.com/979594).
  if (initiated_by_user || !trimmed_query.empty() || query_empty_changed)
    view_delegate_->StartSearch(query);
}

void SearchBoxView::UpdatePlaceholderTextStyle() {
  SkColor primary_color =
      is_jelly_enabled_
          ? GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface)
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary);
  SkColor secondary_color =
      is_jelly_enabled_
          ? GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant)
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorSecondary);
  if (is_app_list_bubble_) {
    // The bubble launcher text is always side-aligned.
    search_box()->set_placeholder_text_draw_flags(
        base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                            : gfx::Canvas::TEXT_ALIGN_LEFT);
    // Bubble launcher uses standard text colors (light-on-dark by default).
    search_box()->set_placeholder_text_color(secondary_color);
    return;
  }
  // Fullscreen launcher centers the text when inactive.
  search_box()->set_placeholder_text_draw_flags(
      is_search_box_active()
          ? (base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                                 : gfx::Canvas::TEXT_ALIGN_LEFT)
          : gfx::Canvas::TEXT_ALIGN_CENTER);
  // Fullscreen launcher uses custom colors (dark-on-light by default).
  search_box()->set_placeholder_text_color(
      is_search_box_active() ? secondary_color : primary_color);
}

void SearchBoxView::UpdateSearchBoxBorder() {
  gfx::Insets border_insets;
  if (!is_app_list_bubble_) {
    // Creates an empty border to create a region for the focus ring to appear.
    border_insets = gfx::Insets(GetFocusRingSpacing());
  } else {
    // Bubble search box does not use a focus ring.
    border_insets = kBorderInsetsForAppListBubble;
  }
  SetBorder(views::CreateEmptyBorder(border_insets));
}

void SearchBoxView::OnPaintBackground(gfx::Canvas* canvas) {
  // Paint the SearchBoxBackground defined in SearchBoxViewBase first.
  views::View::OnPaintBackground(canvas);

  if (is_app_list_bubble_) {
    // When the search box is focused, paint a vertical focus bar along the left
    // edge, vertically aligned with the search icon.
    if (search_box()->HasFocus() && IsTrimmedQueryEmpty(current_query_)) {
      gfx::Point icon_origin;
      views::View::ConvertPointToTarget(search_icon(), this, &icon_origin);
      PaintFocusBar(
          canvas, gfx::Point(0, icon_origin.y()),
          /*height=*/GetSearchBoxIconSize(),
          GetColorProvider()->GetColor(GetFocusColorId(is_jelly_enabled_)));
    }
  }
}

void SearchBoxView::OnPaintBorder(gfx::Canvas* canvas) {
  if (should_paint_highlight_border_) {
    views::HighlightBorder::PaintBorderToCanvas(
        canvas, *this, GetContentsBounds(),
        gfx::RoundedCornersF(corner_radius_),
        chromeos::features::IsJellyrollEnabled()
            ? views::HighlightBorder::Type::kHighlightBorderNoShadow
            : views::HighlightBorder::Type::kHighlightBorder1);
  }
}

const char* SearchBoxView::GetClassName() const {
  return "SearchBoxView";
}

void SearchBoxView::OnThemeChanged() {
  SearchBoxViewBase::OnThemeChanged();

  const SkColor button_icon_color =
      GetColorProvider()->GetColor(kColorAshButtonIconColor);
  close_button()->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, GetSearchBoxIconSize(),
                            button_icon_color));
  assistant_button()->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(chromeos::kAssistantIcon, GetSearchBoxIconSize(),
                            button_icon_color));
  auto* focus_ring = views::FocusRing::Get(assistant_button());
  focus_ring->SetColorId(GetFocusColorId(is_jelly_enabled_));

  if (focus_ring_layer_) {
    focus_ring_layer_->SetColor(
        GetColorProvider()->GetColor(GetFocusColorId(is_jelly_enabled_)));
  }

  UpdateSearchIcon();
  UpdatePlaceholderTextStyle();
  UpdateTextColor();

  UpdateBackgroundColor(GetBackgroundColorForState(current_app_list_state_));
  SchedulePaint();
}

void SearchBoxView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (focus_ring_layer_)
    focus_ring_layer_->layer()->SetBounds(bounds());
}

void SearchBoxView::AddedToWidget() {
  // Creating the search box focus ring relies on its parent layer which only
  // exists after widget initialization.
  if (!is_app_list_bubble_) {
    focus_ring_layer_ = std::make_unique<FocusRingLayer>();
    focus_ring_layer_->SetColor(
        GetColorProvider()->GetColor(GetFocusColorId(is_jelly_enabled_)));
    layer()->parent()->Add(focus_ring_layer_->layer());
    layer()->parent()->StackAtBottom(focus_ring_layer_->layer());
    UpdateSearchBoxFocusPaint();
  }
}

void SearchBoxView::RunLauncherSearchQuery(const std::u16string& query) {
  UpdateQuery(query);
}

void SearchBoxView::OpenAssistantPage() {
  delegate_->AssistantButtonPressed();
}

void SearchBoxView::OpenSearchBoxIphUrl() {
  view_delegate_->OpenSearchBoxIphUrl();
}

// static
int SearchBoxView::GetFocusRingSpacing() {
  return kSearchBoxFocusRingWidth + kSearchBoxFocusRingPadding;
}

void SearchBoxView::OnSearchBoxActiveChanged(bool active) {
  UpdateSearchIcon();

  // Clear ghost text when toggling search box active state.
  MaybeSetAutocompleteGhostText(std::u16string(), std::u16string());

  if (active) {
    result_selection_controller_->ResetSelection(nullptr,
                                                 true /* default_selection */);
  } else {
    result_selection_controller_->ClearSelection();
  }

  delegate_->ActiveChanged(this);
}

void SearchBoxView::UpdateSearchBoxFocusPaint() {
  if (!focus_ring_layer_)
    return;

  // Paints the focus ring if the search box is focused.
  if (search_box()->HasFocus() && !is_search_box_active() &&
      view_delegate_->KeyboardTraversalEngaged()) {
    focus_ring_layer_->layer()->SetVisible(true);
  } else {
    focus_ring_layer_->layer()->SetVisible(false);
  }
}

void SearchBoxView::OnKeyEvent(ui::KeyEvent* evt) {
  // Handle keyboard navigation keys when close button is focused - move the
  // focus to the search box text field, and ensure result selection gets
  // updated according to the navigation key. The latter is the reason
  // navigation is handled here instead of the focus manager - intended result
  // selection depends on the key event that triggered the focus change.
  if (close_button()->HasFocus() && evt->type() == ui::ET_KEY_PRESSED &&
      (IsUnhandledArrowKeyEvent(*evt) || evt->key_code() == ui::VKEY_TAB)) {
    search_box()->RequestFocus();

    if (delegate_->CanSelectSearchResults() &&
        result_selection_controller_->MoveSelection(*evt) ==
            ResultSelectionController::MoveResult::kResultChanged) {
      UpdateSearchBoxForSelectedResult(
          result_selection_controller_->selected_result()->result());
    }

    evt->SetHandled();
    return;
  }

  delegate_->OnSearchBoxKeyEvent(evt);
}

void SearchBoxView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (HasAutocompleteText()) {
    node_data->role = ax::mojom::Role::kTextField;
    node_data->SetValue(l10n_util::GetStringFUTF16(
        IDS_APP_LIST_SEARCH_BOX_AUTOCOMPLETE, search_box()->GetText()));
  }
}

void SearchBoxView::UpdateBackground(AppListState target_state) {
  int corner_radius = GetSearchBoxBorderCornerRadiusForState(target_state);
  SetSearchBoxBackgroundCornerRadius(corner_radius);
  const bool is_corner_radius_changed = corner_radius_ != corner_radius;
  corner_radius_ = corner_radius;

  bool highlight_border_changed = false;

  // The background layer is only painted for the search box in tablet mode.
  // Also the layer is not painted when the search result page is visible.
  if (!is_app_list_bubble_ && (!search_result_page_visible_ ||
                               target_state == AppListState::kStateApps)) {
    layer()->SetClipRect(GetContentsBounds());
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
    highlight_border_changed = !should_paint_highlight_border_;
    should_paint_highlight_border_ = true;
  } else {
    layer()->SetBackgroundBlur(0);
    layer()->SetBackdropFilterQuality(0);
    highlight_border_changed = should_paint_highlight_border_;
    should_paint_highlight_border_ = false;
  }

  if (is_corner_radius_changed || highlight_border_changed)
    SchedulePaint();
  UpdateBackgroundColor(GetBackgroundColorForState(target_state));
  UpdateTextColor();
  current_app_list_state_ = target_state;
}

void SearchBoxView::UpdateLayout(AppListState target_state,
                                 int target_state_height) {
  // Horizontal margins are selected to match search box icon's vertical
  // margins. Space used for iph should be ignored.
  const int iph_height =
      iph_view() ? iph_view()->GetPreferredSize().height() : 0;
  const int horizontal_spacing =
      (target_state_height - iph_height - GetSearchBoxIconSize()) / 2;
  const int horizontal_right_padding =
      horizontal_spacing -
      (GetSearchBoxButtonSize() - GetSearchBoxIconSize()) / 2;
  box_layout_view()->SetInsideBorderInsets(
      gfx::Insets::TLBR(0, horizontal_spacing, 0, horizontal_right_padding));
  box_layout_view()->SetBetweenChildSpacing(horizontal_spacing);
  InvalidateLayout();
  // Avoid setting background when animating to kStateApps, background will be
  // set when the animation ends.
  if (target_state != AppListState::kStateApps)
    UpdateBackground(target_state);
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    AppListState state) const {
  return state == AppListState::kStateSearchResults
             ? kExpandedSearchBoxCornerRadius
             : kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(AppListState state) const {
  const auto* app_list_widget = GetWidget();

  if (is_app_list_bubble_) {
    return app_list_widget->GetColorProvider()->GetColor(
        is_jelly_enabled_
            ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)
            : kColorAshControlBackgroundColorInactive);
  }

  if (search_result_page_visible_)
    return SK_ColorTRANSPARENT;

  return app_list_widget->GetColorProvider()->GetColor(
      is_jelly_enabled_
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)
          : kColorAshShieldAndBase80);
}

void SearchBoxView::ProcessAutocomplete(
    SearchResultBaseView* first_result_view) {
  if (!ShouldProcessAutocomplete())
    return;

  if (!first_result_view || !first_result_view->selected())
    return;

  SearchResult* const first_visible_result = first_result_view->result();

  // Do not autocomplete on answer cards.
  if (!first_visible_result || first_visible_result->display_type() ==
                                   SearchResultDisplayType::kAnswerCard) {
    return;
  }

  if (first_result_view->is_default_result() &&
      current_query_ != search_box()->GetText()) {
    // Search box text has been set to the previous selected result. Reset
    // it back to the current query. This could happen due to the racing
    // between results update and user press key to select a result.
    // See crbug.com/1065454.
    search_box()->SetText(current_query_);
  }

  // Current non-autocompleted text.
  const std::u16string& user_typed_text =
      search_box()->GetText().substr(0, highlight_range_.start());
  if (last_key_pressed_ == ui::VKEY_BACK ||
      last_key_pressed_ == ui::VKEY_DELETE || IsArrowKey(last_key_pressed_) ||
      !first_visible_result ||
      user_typed_text.length() < kMinimumLengthToAutocomplete) {
    // If the suggestion was rejected, no results exist, or current text
    // is too short for a confident autocomplete suggestion.
    return;
  }

  const std::u16string& details = first_visible_result->details();
  const std::u16string& search_text = first_visible_result->title();

  // Don't set autocomplete text if it's the same as user typed text.
  if (user_typed_text == details || user_typed_text == search_text)
    return;

  if (ProcessPrefixMatchAutocomplete(first_visible_result, user_typed_text)) {
    RecordAutocompleteMatchMetric(SearchBoxTextMatch::kPrefixMatch);
    return;
  }

  // Clear autocomplete since we don't have a prefix match.
  ClearAutocompleteText();

  if (IsValidAutocompleteText(search_text)) {
    // Setup autocomplete ghost text for eligible search_text.
    MaybeSetAutocompleteGhostText(first_result_view->result()->title(),
                                  GetCategoryName(first_result_view->result()));

    if (IsSubstringCaseInsensitive(search_text, user_typed_text)) {
      // user_typed_text is a substring of search_text and is eligible for
      // autocompletion.
      RecordAutocompleteMatchMetric(SearchBoxTextMatch::kSubstringMatch);
    } else {
      // user_typed_text does not match search_text but is eligible for
      // autocompletion.
      RecordAutocompleteMatchMetric(
          SearchBoxTextMatch::kAutocompletedWithoutMatch);
    }
  } else {
    // search_text is not eligible for autocompletion.
    RecordAutocompleteMatchMetric(SearchBoxTextMatch::kNoMatch);
  }
}

bool SearchBoxView::ProcessPrefixMatchAutocomplete(
    SearchResult* search_result,
    const std::u16string& user_typed_text) {
  const std::u16string& details = search_result->details();
  const std::u16string& search_text = search_result->title();

  if (base::StartsWith(details, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      IsValidAutocompleteText(details)) {
    // Current text in the search_box matches the first result's url.
    SetAutocompleteText(details);
    MaybeSetAutocompleteGhostText(std::u16string(),
                                  GetCategoryName(search_result));
    return true;
  }

  if (base::StartsWith(search_text, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      IsValidAutocompleteText(search_text)) {
    // Current text in the search_box matches the first result's search result
    // text.
    SetAutocompleteText(search_text);
    MaybeSetAutocompleteGhostText(std::u16string(),
                                  GetCategoryName(search_result));
    return true;
  }
  return false;
}

void SearchBoxView::ClearAutocompleteText() {
  if (!ShouldProcessAutocomplete())
    return;

  // Clear ghost text.
  MaybeSetAutocompleteGhostText(std::u16string(), std::u16string());

  // Avoid triggering subsequent query by temporarily setting controller to
  // nullptr.
  search_box()->set_controller(nullptr);
  // search_box()->ClearCompositionText() does not work here because
  // SetAutocompleteText() calls SelectRange(), which comfirms the active
  // composition text (so there is nothing to clear here). Set empty composition
  // text to clear the selected range.
  search_box()->SetCompositionText(ui::CompositionText());
  search_box()->set_controller(this);
  ResetHighlightRange();
}

void SearchBoxView::OnResultContainerVisibilityChanged(bool visible) {
  if (search_result_page_visible_ == visible)
    return;
  search_result_page_visible_ = visible;
  UpdateBackground(current_app_list_state_);
  SchedulePaint();
}

bool SearchBoxView::HasValidQuery() {
  return !IsTrimmedQueryEmpty(current_query_);
}

int SearchBoxView::GetSearchBoxIconSize() {
  return kBubbleLauncherSearchBoxIconSize;
}

int SearchBoxView::GetSearchBoxButtonSize() {
  return kBubbleLauncherSearchBoxButtonSizeDip;
}

void SearchBoxView::SetIsIphAllowed(bool iph_allowed) {
  if (is_iph_allowed_ == iph_allowed) {
    return;
  }

  is_iph_allowed_ = iph_allowed;

  UpdateIphViewVisibility();
}

void SearchBoxView::CloseButtonPressed() {
  delegate_->CloseButtonPressed();
}

void SearchBoxView::AssistantButtonPressed() {
  delegate_->AssistantButtonPressed();
}

void SearchBoxView::UpdateSearchIcon() {
  const bool search_engine_is_google =
      AppListModelProvider::Get()->search_model()->search_engine_is_google();
  const gfx::VectorIcon& google_icon = is_search_box_active()
                                           ? vector_icons::kGoogleColorIcon
                                           : kGoogleBlackIcon;
  const gfx::VectorIcon& icon =
      search_engine_is_google ? google_icon : kSearchEngineNotGoogleIcon;
  SetSearchIconImage(gfx::CreateVectorIcon(
      icon, GetSearchBoxIconSize(),
      GetColorProvider()->GetColor(kColorAshButtonIconColor)));
}

bool SearchBoxView::IsValidAutocompleteText(
    const std::u16string& autocomplete_text) {
  // Don't set autocomplete text if it's the same as current search box
  // text.
  if (autocomplete_text == search_box()->GetText())
    return false;
  // Don't set autocomplete text if the highlighted text is the same as
  // before.
  if (autocomplete_text.length() > highlight_range_.start() &&
      autocomplete_text.substr(highlight_range_.start()) ==
          search_box()->GetSelectedText()) {
    return false;
  }
  return true;
}

void SearchBoxView::UpdateTextColor() {
  ui::ColorId color_id =
      is_jelly_enabled_
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshTextColorPrimary;
  search_box()->SetTextColor(GetColorProvider()->GetColor(color_id));
}

void SearchBoxView::UpdatePlaceholderTextAndAccessibleName() {
  const int a11y_name_template =
      is_app_list_bubble_
          ? IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL
          : IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_TABLET;
  switch (SelectPlaceholderText()) {
    case PlaceholderTextType::kShortcuts:
      search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SHORTCUTS)));
      search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
          a11y_name_template,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SHORTCUTS)));
      break;
    case PlaceholderTextType::kTabs:
      search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
          l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS)));
      search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
          a11y_name_template,
          l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS)));
      break;
    case PlaceholderTextType::kSettings:
      search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SETTINGS)));
      search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
          a11y_name_template,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SETTINGS)));
      break;
    case PlaceholderTextType::kGames:
      search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_GAMES)));
      search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
          a11y_name_template, l10n_util::GetStringUTF16(
                                  IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_GAMES)));
      break;
    case PlaceholderTextType::kImages:
      search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
          l10n_util::GetStringUTF16(
              IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_IMAGES)));
      search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
          a11y_name_template, l10n_util::GetStringUTF16(
                                  IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_IMAGES)));
      break;
  }
}

void SearchBoxView::AcceptAutocompleteText() {
  if (!ShouldProcessAutocomplete())
    return;

  // Do not trigger another search here in case the user is left clicking to
  // select existing autocomplete text. (This also matches omnibox behavior.)
  DCHECK(HasAutocompleteText());
  search_box()->ClearSelection();
  ResetHighlightRange();
}

bool SearchBoxView::HasAutocompleteText() {
  // If the selected range is non-empty, it will either be suggested by
  // autocomplete or selected by the user. If the recorded autocomplete
  // |highlight_range_| matches the selection range, this text is suggested by
  // autocomplete.
  return search_box()->GetSelectedRange().EqualsIgnoringDirection(
             highlight_range_) &&
         highlight_range_.length() > 0;
}

void SearchBoxView::OnBeforeUserAction(views::Textfield* sender) {
  if (a11y_active_descendant_)
    SetA11yActiveDescendant(absl::nullopt);
}

void SearchBoxView::SetAutocompleteText(
    const std::u16string& autocomplete_text) {
  if (!ShouldProcessAutocomplete())
    return;

  // Clear existing autocomplete text and reset the highlight range.
  ClearAutocompleteText();

  const std::u16string& current_text = search_box()->GetText();
  // Currrent text is a prefix of autocomplete text.
  DCHECK(base::StartsWith(autocomplete_text, current_text,
                          base::CompareCase::INSENSITIVE_ASCII));
  // Autocomplete text should not be the same as current search box text.
  DCHECK(autocomplete_text != current_text);
  // Autocomplete text should not be the same as highlighted text.

  const std::u16string& highlighted_text =
      autocomplete_text.substr(highlight_range_.start());
  DCHECK(highlighted_text != current_text);

  highlight_range_.set_end(autocomplete_text.length());
  ui::CompositionText composition_text;
  composition_text.text = highlighted_text;
  composition_text.selection = gfx::Range(0, highlighted_text.length());

  // Avoid triggering subsequent query by temporarily setting controller to
  // nullptr.
  search_box()->set_controller(nullptr);
  search_box()->SetCompositionText(composition_text);
  search_box()->set_controller(this);

  // The controller was null briefly, so it was unaware of a highlight change.
  // As a result, we need to manually declare the range to allow for proper
  // selection behavior.
  search_box()->SetSelectedRange(highlight_range_);

  // Send an event to alert ChromeVox that an autocomplete has occurred.
  // The |kValueChanged| type lets ChromeVox know that it should scan
  // |node_data| for "Value".
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);

  MaybeSetAutocompleteGhostText(std::u16string(), std::u16string());
}

SearchBoxView::PlaceholderTextType SearchBoxView::SelectPlaceholderText()
    const {
  if (use_fixed_placeholder_text_for_test_)
    return kDefaultPlaceholders[0];

  if (chromeos::features::IsCloudGamingDeviceEnabled())
    return kGamingPlaceholders[rand() % std::size(kGamingPlaceholders)];

  return kDefaultPlaceholders[rand() % std::size(kDefaultPlaceholders)];
}

void SearchBoxView::UpdateQuery(const std::u16string& new_query) {
  search_box()->SetText(new_query);
  ContentsChanged(search_box(), new_query);
}

void SearchBoxView::ClearSearchAndDeactivateSearchBox() {
  if (!is_search_box_active())
    return;

  SetA11yActiveDescendant(absl::nullopt);
  // Set search box as inactive first, because ClearSearch() eventually calls
  // into AppListMainView::QueryChanged() which will hide search results based
  // on `is_search_box_active_`.
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
  ClearSearch();
  MaybeSetAutocompleteGhostText(std::u16string(), std::u16string());
}

void SearchBoxView::SetA11yActiveDescendant(
    const absl::optional<int32_t>& active_descendant) {
  a11y_active_descendant_ = active_descendant;
  search_box()->NotifyAccessibilityEvent(
      ax::mojom::Event::kActiveDescendantChanged, true);
}

void SearchBoxView::UseFixedPlaceholderTextForTest() {
  if (use_fixed_placeholder_text_for_test_)
    return;

  use_fixed_placeholder_text_for_test_ = true;
  UpdatePlaceholderTextAndAccessibleName();
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  DCHECK(result_selection_controller_);
  if (key_event.type() == ui::ET_KEY_RELEASED)
    return false;

  // Events occurring over an inactive search box are handled elsewhere, with
  // the exception of left/right arrow key events, and return.
  if (!is_search_box_active()) {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      SetSearchBoxActive(true, key_event.type());
      return true;
    }

    if (IsUnhandledLeftRightKeyEvent(key_event)) {
      return ProcessLeftRightKeyTraversalForTextfield(search_box(), key_event);
    }

    return false;
  }

  // Nothing to do if no results are available (the rest of the method handles
  // result actions and result traversal). This might happen if zero state
  // suggestions are not enabled, and search box textfield is empty.
  if (!delegate_->CanSelectSearchResults())
    return false;

  // When search box is active, the focus cycles between close button and the
  // search_box - when close button is focused, traversal keys (arrows and
  // tab) should move the focus to the search box, and reset the selection
  // (which might have been cleared when focus moved to the close button).
  if (!search_box()->HasFocus()) {
    // Only handle result traversal keys.
    if (!IsUnhandledArrowKeyEvent(key_event) &&
        key_event.key_code() != ui::VKEY_TAB) {
      return false;
    }

    search_box()->RequestFocus();
    if (result_selection_controller_->MoveSelection(key_event) ==
        ResultSelectionController::MoveResult::kResultChanged) {
      UpdateSearchBoxForSelectedResult(
          result_selection_controller_->selected_result()->result());
    }
    return true;
  }

  // Handle return - opens the selected result.
  if (key_event.key_code() == ui::VKEY_RETURN) {
    // Hitting Enter when focus is on search box opens the selected result.
    ui::KeyEvent event(key_event);
    SearchResultBaseView* selected_result =
        result_selection_controller_->selected_result();
    if (selected_result && selected_result->result())
      selected_result->OnKeyEvent(&event);
    return true;
  }

  // Allows alt+back and alt+delete as a shortcut for the 'remove result'
  // dialog
  if (key_event.IsAltDown() &&
      ((key_event.key_code() == ui::VKEY_BROWSER_BACK) ||
       (key_event.key_code() == ui::VKEY_DELETE))) {
    ui::KeyEvent event(key_event);
    SearchResultBaseView* selected_result =
        result_selection_controller_->selected_result();
    if (selected_result && selected_result->result())
      selected_result->OnKeyEvent(&event);
    // Reset the selected result to the default result.
    result_selection_controller_->ResetSelection(nullptr,
                                                 true /* default_selection */);
    search_box()->SetText(std::u16string());
    return true;
  }

  // Do not handle keys intended for result selection traversal here - these
  // should be handled elsewhere, for example by the search box text field.
  // Keys used for result selection traversal:
  // *   TAB
  // *   up/down key
  // *   left/right, if the selected container is horizontal. For vertical
  //     containers, left and right key should be handled by the text field
  //     (to move cursor, and clear or accept autocomplete suggestion).
  const bool result_selection_traversal_key_event =
      key_event.key_code() == ui::VKEY_TAB ||
      IsUnhandledUpDownKeyEvent(key_event) ||
      (IsUnhandledLeftRightKeyEvent(key_event) &&
       result_selection_controller_->selected_location_details() &&
       result_selection_controller_->selected_location_details()
           ->container_is_horizontal);
  if (!result_selection_traversal_key_event) {
    // Record the |last_key_pressed_| for autocomplete.
    if (!search_box()->GetText().empty() && ShouldProcessAutocomplete())
      last_key_pressed_ = key_event.key_code();
    return false;
  }

  // Clear non-auto-complete generated selection to prevent navigation keys from
  // deleting selected text.
  if (search_box()->HasSelection() && !HasAutocompleteText())
    search_box()->ClearSelection();

  ResultSelectionController::MoveResult move_result =
      result_selection_controller_->MoveSelection(key_event);
  switch (move_result) {
    case ResultSelectionController::MoveResult::kNone:
      // If the |ResultSelectionController| decided not to change selection,
      // return early, as what follows is actions for updating based on
      // change.
      break;
    case ResultSelectionController::MoveResult::kSelectionCycleRejected:
      // If move was about to cycle, clear the selection and move the focus to
      // the next element in the SearchBoxView - close_button() (only
      // close_button() and search_box() are expected to be in the focus cycle
      // while the search box is active).
      if (HasAutocompleteText())
        ClearAutocompleteText();
      result_selection_controller_->ClearSelection();

      DCHECK(close_button()->GetVisible());
      close_button()->RequestFocus();
      SetA11yActiveDescendant(absl::nullopt);
      break;
    case ResultSelectionController::MoveResult::kResultChanged:
      UpdateSearchBoxForSelectedResult(
          result_selection_controller_->selected_result()->result());
      break;
  }

  return true;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSE_PRESSED && HasAutocompleteText())
    AcceptAutocompleteText();

  // Don't activate search box for context menu click.
  if (mouse_event.type() == ui::ET_MOUSE_PRESSED &&
      mouse_event.IsOnlyRightMouseButton()) {
    return false;
  }

  return SearchBoxViewBase::HandleMouseEvent(sender, mouse_event);
}

bool SearchBoxView::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  if (gesture_event.type() == ui::ET_GESTURE_TAP && HasAutocompleteText())
    AcceptAutocompleteText();
  return SearchBoxViewBase::HandleGestureEvent(sender, gesture_event);
}

void SearchBoxView::UpdateSearchBoxForSelectedResult(
    SearchResult* selected_result) {
  if (!selected_result)
    return;

  if (selected_result->result_type() ==
          AppListSearchResultType::kInternalPrivacyInfo ||
      selected_result->display_type() == SearchResultDisplayType::kAnswerCard) {
    // Privacy and answer card views should not change the search box text.
    return;
  }

  if (features::IsAutocompleteExtendedSuggestionsEnabled()) {
    ClearAutocompleteText();

    const std::u16string& details = selected_result->details();
    const std::u16string& search_text = selected_result->title();

    // Don't set autocomplete text if it's the same as user typed text.
    if (current_query_ == details || current_query_ == search_text)
      return;

    if (!ProcessPrefixMatchAutocomplete(selected_result, current_query_)) {
      MaybeSetAutocompleteGhostText(selected_result->title(),
                                    GetCategoryName(selected_result));
    }
  } else {
    if (selected_result->result_type() == AppListSearchResultType::kOmnibox &&
        !selected_result->is_omnibox_search() &&
        !selected_result->details().empty()) {
      // For url (non-search) results, use details to ensure that the url is
      // displayed.
      search_box()->SetText(selected_result->details());
    } else {
      search_box()->SetText(selected_result->title());
    }
  }
}

void SearchBoxView::SearchEngineChanged() {
  UpdateSearchIcon();
}

void SearchBoxView::ShowAssistantChanged() {
  SetShowAssistantButton(AppListModelProvider::Get()
                             ->search_model()
                             ->search_box()
                             ->show_assistant_button());

  // `LauncherSearchIphView` and an Assistant button have synchronized
  // backgrounds. The IPH UI is integrated with the Assistant button. We don't
  // show an IPH if Assistant is disabled. Both `LauncherSearchIphView` and the
  // Assistant button are hosted by `SearchBoxViewBase`.
  UpdateIphViewVisibility();
}

void SearchBoxView::UpdateIphViewVisibility() {
  const bool show_assistant_button = AppListModelProvider::Get()
                                         ->search_model()
                                         ->search_box()
                                         ->show_assistant_button();
  const bool would_trigger_iph =
      AppListModelProvider::Get()->search_model()->would_trigger_iph();
  const bool is_iph_showing = iph_view() != nullptr;

  const bool should_show_iph = show_assistant_button && is_iph_allowed_ &&
                               !HasValidQuery() &&
                               (would_trigger_iph || is_iph_showing);

  if (should_show_iph == is_iph_showing) {
    return;
  }

  if (should_show_iph) {
    std::unique_ptr<ScopedIphSession> scoped_iph_session =
        view_delegate_->CreateLauncherSearchIphSession();
    if (!scoped_iph_session) {
      return;
    }

    SetIphView(std::make_unique<LauncherSearchIphView>(
        std::move(scoped_iph_session), /*delegate=*/this,
        /*is_in_tablet_mode=*/!is_app_list_bubble_));

    assistant_button()->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive,
        base::i18n::IsRTL() ? kAssistantButtonBackgroundRadiiRTL
                            : kAssistantButtonBackgroundRadiiLTR,
        /*for_border_thickness=*/0));
  } else {
    DeleteIphView();
    assistant_button()->SetBackground(nullptr);
  }

  // Adding or removing IPH view can change `SearchBoxView` bounds largely.
  // Re-layout can be necessary on parent views as well. Explicitly call
  // `InvalidateLayout` to trigger re-layouts on all parent views. Without this,
  // we can have unnecessary spaces in `SearchBoxView` for an IPH dismiss under
  // some conditions.
  InvalidateLayout();
}

void SearchBoxView::OnWouldTriggerIphChanged() {
  UpdateIphViewVisibility();
}

bool SearchBoxView::ShouldProcessAutocomplete() {
  // IME sets composition text while the user is typing, so avoid handling
  // autocomplete in this case to avoid conflicts.
  // The user's cursor may not be at the end of the the current query, so avoid
  // handling autocomplete in this case to avoid moving the user's cursor.
  return search_box()->GetCursorPosition() == search_box()->GetText().size() &&
         (!(search_box()->IsIMEComposing() && highlight_range_.is_empty()));
}

void SearchBoxView::ResetHighlightRange() {
  DCHECK(ShouldProcessAutocomplete());
  const uint32_t text_length = search_box()->GetText().length();
  highlight_range_.set_start(text_length);
  highlight_range_.set_end(text_length);
}

}  // namespace ash
