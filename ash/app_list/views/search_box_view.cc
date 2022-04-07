// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_box_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/resources/grit/app_list_resources.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/highlight_border.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
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

bool IsTrimmedQueryEmpty(const std::u16string& query) {
  std::u16string trimmed_query;
  base::TrimWhitespace(query, base::TrimPositions::TRIM_ALL, &trimmed_query);
  return trimmed_query.empty();
}

}  // namespace

class SearchBoxView::FocusRingLayer : public ui::Layer, ui::LayerDelegate {
 public:
  FocusRingLayer() : Layer(ui::LAYER_TEXTURED) {
    SetName("search_box/FocusRing");
    SetFillsBoundsOpaquely(false);
    set_delegate(this);
  }
  FocusRingLayer(const FocusRingLayer&) = delete;
  FocusRingLayer& operator=(const FocusRingLayer&) = delete;
  ~FocusRingLayer() override {}

 private:
  // views::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, bounds().size());
    gfx::Canvas* canvas = recorder.canvas();

    // When using strokes to draw a rect, the bounds set is the center of the
    // rect, which means that setting draw bounds to `bounds()` will leave half
    // of the border outside the layer that may not be painted. Shrink the draw
    // bounds by half of the width to solve this problem.
    gfx::Rect draw_bounds(bounds().size());
    draw_bounds.Inset(kSearchBoxFocusRingWidth / 2);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(AppListColorProvider::Get()->GetFocusRingColor());
    flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    flags.setStrokeWidth(kSearchBoxFocusRingWidth);
    canvas->DrawRoundRect(draw_bounds, kSearchBoxFocusRingCornerRadius, flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    SchedulePaint(gfx::Rect(size()));
  }
};

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : SearchBoxViewBase(delegate),
      view_delegate_(view_delegate),
      app_list_view_(app_list_view),
      is_app_list_bubble_(!app_list_view_),
      is_tablet_mode_(view_delegate_->IsInTabletMode()) {
  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  model_provider->AddObserver(this);
  search_box_model_observer_.Observe(
      model_provider->search_model()->search_box());
}

SearchBoxView::~SearchBoxView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void SearchBoxView::Init(const InitParams& params) {
  SearchBoxViewBase::Init(params);
  UpdatePlaceholderTextAndAccessibleName();
  current_query_ = search_box()->GetText();
  if (is_app_list_bubble_) {
    // Add margins to the text field because the BoxLayout vertical centering
    // does not properly align the text baseline with the icons.
    search_box()->SetProperty(views::kMarginsKey,
                              kTextFieldMarginsForAppListBubble);
  }
  ShowAssistantChanged();
}

void SearchBoxView::SetResultSelectionController(
    ResultSelectionController* controller) {
  DCHECK(controller);
  result_selection_controller_ = controller;
}

void SearchBoxView::OnTabletModeChanged(bool started) {
  // Reset timer metrics when tablet mode changes.
  if (is_tablet_mode_ != started)
    user_initiated_model_update_time_ = base::TimeTicks();

  is_tablet_mode_ = started;

  UpdateKeyboardVisibility();
  // Search box accessible name may change depending on tablet mode state.
  UpdatePlaceholderTextAndAccessibleName();
  UpdateSearchBoxBorder();
  UpdateBackgroundColor(GetBackgroundColorForState(current_app_list_state_));
}

void SearchBoxView::ResetForShow() {
  // Avoid clearing an already inactive SearchBox in tablet mode because this
  // causes suggested chips to flash (http://crbug.com/979594).
  if (!is_search_box_active() && is_tablet_mode_)
    return;

  ClearSearchAndDeactivateSearchBox();
  if (contents_view_) {
    SetSearchBoxBackgroundCornerRadius(GetSearchBoxBorderCornerRadiusForState(
        contents_view_->GetActiveState()));
  }
}

void SearchBoxView::UpdateSearchTextfieldAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (a11y_active_descendant_) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                               *a11y_active_descendant_);
  }
}

void SearchBoxView::ClearSearch() {
  SearchBoxViewBase::ClearSearch();
  current_query_.clear();
  // Peeking/fullscreen launcher needs explicit state changes.
  if (app_list_view_) {
    app_list_view_->SetStateFromSearchBoxView(
        /*search_box_is_empty=*/true, /*triggered_by_contents_change=*/false);
  }
  NotifyQueryChanged();
}

void SearchBoxView::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::ET_MOUSEWHEEL) {
    // TODO(crbug.com/1216082): Forward scroll events for bubble launcher.
    if (app_list_view_ &&
        !app_list_view_->HandleScroll(
            located_event->location(),
            located_event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL)) {
      return;
    }
  }
  SearchBoxViewBase::HandleSearchBoxEvent(located_event);
}

void SearchBoxView::OnActiveAppListModelsChanged(AppListModel* model,
                                                 SearchModel* search_model) {
  search_box_model_observer_.Reset();
  search_box_model_observer_.Observe(search_model->search_box());

  ResetForShow();
  UpdateSearchIcon();
  OnWallpaperColorsChanged();
  ShowAssistantChanged();
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

void SearchBoxView::UpdateModel(bool initiated_by_user) {
  auto new_query = search_box()->GetText();
  SearchBoxModel* const search_box_model =
      AppListModelProvider::Get()->search_model()->search_box();

  // Randomly select a new placeholder text when we get an empty new query.
  if (new_query.empty() && features::IsProductivityLauncherEnabled())
    UpdatePlaceholderTextAndAccessibleName();

  if (initiated_by_user) {
    const std::u16string& previous_query = search_box_model->text();
    const base::TimeTicks current_time = base::TimeTicks::Now();
    if (previous_query.empty() && !new_query.empty()) {
      // Set 'user_initiated_model_update_time_' when initiating a new query.
      user_initiated_model_update_time_ = current_time;
    } else if (!previous_query.empty() && new_query.empty()) {
      // Reset 'user_initiated_model_update_time_' when clearing the search_box.
      user_initiated_model_update_time_ = base::TimeTicks();
    } else if (new_query != previous_query &&
               !user_initiated_model_update_time_.is_null()) {
      if (is_tablet_mode_) {
        UMA_HISTOGRAM_TIMES("Ash.SearchModelUpdateTime.TabletMode",
                            current_time - user_initiated_model_update_time_);
      } else {
        UMA_HISTOGRAM_TIMES("Ash.SearchModelUpdateTime.ClamshellMode",
                            current_time - user_initiated_model_update_time_);
      }
      user_initiated_model_update_time_ = current_time;
    }
  }
  // Temporarily remove from observer to ignore notifications caused by us.
  search_box_model_observer_.Reset();

  search_box_model->Update(new_query, initiated_by_user);
  search_box_model_observer_.Observe(search_box_model);
}

void SearchBoxView::UpdateSearchIcon() {
  const bool search_engine_is_google =
      AppListModelProvider::Get()->search_model()->search_engine_is_google();
  const gfx::VectorIcon& google_icon = is_search_box_active()
                                           ? vector_icons::kGoogleColorIcon
                                           : kGoogleBlackIcon;
  const gfx::VectorIcon& icon =
      search_engine_is_google ? google_icon : kSearchEngineNotGoogleIcon;
  SetSearchIconImage(
      gfx::CreateVectorIcon(icon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                SkColorSetARGB(0xDE, 0x00, 0x00, 0x00))));
}

void SearchBoxView::UpdatePlaceholderTextStyle() {
  if (is_app_list_bubble_) {
    // The bubble launcher text is always side-aligned.
    search_box()->set_placeholder_text_draw_flags(
        base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                            : gfx::Canvas::TEXT_ALIGN_LEFT);
    // Bubble launcher uses standard text colors (light-on-dark by default).
    search_box()->set_placeholder_text_color(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kTextColorSecondary));
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
      is_search_box_active()
          ? AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
                kZeroQuerySearchboxColor)
          : AppListColorProvider::Get()->GetSearchBoxTextColor(
                kDefaultSearchboxPlaceholderTextColor));
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
      PaintFocusBar(canvas, gfx::Point(0, icon_origin.y()),
                    /*height=*/kSearchBoxIconSize);
    }
  }
}

void SearchBoxView::OnPaintBorder(gfx::Canvas* canvas) {
  if (should_paint_highlight_border_) {
    HighlightBorder::PaintBorderToCanvas(
        canvas, GetContentsBounds(), corner_radius_,
        HighlightBorder::Type::kHighlightBorder1, false);
  }
}

const char* SearchBoxView::GetClassName() const {
  return "SearchBoxView";
}

void SearchBoxView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetupAssistantButton();
  SetupCloseButton();
  OnWallpaperColorsChanged();
}

void SearchBoxView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (focus_ring_layer_)
    focus_ring_layer_->SetBounds(bounds());
}

// static
int SearchBoxView::GetFocusRingSpacing() {
  return kSearchBoxFocusRingWidth + kSearchBoxFocusRingPadding;
}

void SearchBoxView::MaybeCreateFocusRing() {
  if (!is_app_list_bubble_) {
    focus_ring_layer_ = std::make_unique<FocusRingLayer>();
    layer()->parent()->Add(focus_ring_layer_.get());
    layer()->parent()->StackAtBottom(focus_ring_layer_.get());
  }
}

void SearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                gfx::kGoogleGrey700)));
  close->SetVisible(false);
  std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
  close->SetAccessibleName(close_button_label);
  close->SetTooltipText(close_button_label);
}

void SearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  back->SetImage(views::ImageButton::STATE_NORMAL,
                 rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  back->SetVisible(false);
  std::u16string back_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_BACK));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
}

void SearchBoxView::RecordSearchBoxActivationHistogram(
    ui::EventType event_type) {
  ActivationSource activation_type;
  switch (event_type) {
    case ui::ET_GESTURE_TAP:
      activation_type = ActivationSource::kGestureTap;
      break;
    case ui::ET_MOUSE_PRESSED:
      activation_type = ActivationSource::kMousePress;
      break;
    case ui::ET_KEY_PRESSED:
      activation_type = ActivationSource::kKeyPress;
      break;
    default:
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchBoxActivated", activation_type);
  if (is_tablet_mode_) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchBoxActivated.TabletMode",
                              activation_type);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchBoxActivated.ClamshellMode",
                              activation_type);
  }
}

void SearchBoxView::OnSearchBoxActiveChanged(bool active) {
  if (active) {
    result_selection_controller_->ResetSelection(nullptr,
                                                 true /* default_selection */);
  } else {
    result_selection_controller_->ClearSelection();
  }

  // Remove accessibility hint for classic launcher when search box is active
  // because there are no apps to navigate to.
  if (!features::IsProductivityLauncherEnabled()) {
    if (active) {
      search_box()->SetAccessibleName(std::u16string());
    } else {
      UpdatePlaceholderTextAndAccessibleName();
    }
  }
}

void SearchBoxView::UpdateSearchBoxFocusPaint() {
  if (!focus_ring_layer_)
    return;

  // Paints the focus ring if the search box is focused.
  if (search_box()->HasFocus() && !is_search_box_active() &&
      view_delegate_->KeyboardTraversalEngaged()) {
    focus_ring_layer_->SetVisible(true);
  } else {
    focus_ring_layer_->SetVisible(false);
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

    if (delegate()->CanSelectSearchResults() &&
        result_selection_controller_->MoveSelection(*evt) ==
            ResultSelectionController::MoveResult::kResultChanged) {
      UpdateSearchBoxTextForSelectedResult(
          result_selection_controller_->selected_result()->result());
    }

    evt->SetHandled();
    return;
  }

  SearchBoxViewBase::OnKeyEvent(evt);
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);
  return false;
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
  if (is_tablet_mode_ && (!search_result_page_visible_ ||
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
  // margins.
  const int horizontal_spacing = (target_state_height - kSearchBoxIconSize) / 2;
  const int horizontal_right_padding =
      horizontal_spacing - (kSearchBoxButtonSizeDip - kSearchBoxIconSize) / 2;
  box_layout()->set_inside_border_insets(
      gfx::Insets::TLBR(0, horizontal_spacing, 0, horizontal_right_padding));
  box_layout()->set_between_child_spacing(horizontal_spacing);
  InvalidateLayout();
  UpdateBackground(target_state);
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    AppListState state) const {
  if (state == AppListState::kStateSearchResults && app_list_view_ &&
      !app_list_view_->is_in_drag()) {
    return features::IsProductivityLauncherEnabled()
               ? kExpandedSearchBoxCornerRadiusForProductivityLauncher
               : kSearchBoxBorderCornerRadiusSearchResult;
  }
  return kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(AppListState state) const {
  if (state == AppListState::kStateSearchResults) {
    if ((features::IsDarkLightModeEnabled() ||
         features::IsProductivityLauncherEnabled()) &&
        search_result_page_visible_) {
      return SK_ColorTRANSPARENT;
    }
    return AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor();
  }
  return AppListColorProvider::Get()->GetSearchBoxBackgroundColor();
}

void SearchBoxView::ShowZeroStateSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("AppList_ShowZeroStateSuggestions"));
  std::u16string empty_query;
  ContentsChanged(search_box(), empty_query);
}

void SearchBoxView::OnWallpaperColorsChanged() {
  UpdateSearchIcon();
  UpdatePlaceholderTextStyle();
  UpdateTextColor();

  if (features::IsDarkLightModeEnabled()) {
    UpdateBackgroundColor(
        AppListColorProvider::Get()->GetSearchBoxBackgroundColor());
  }
  SchedulePaint();
}

void SearchBoxView::ProcessAutocomplete(
    SearchResultBaseView* first_result_view) {
  if (!ShouldProcessAutocomplete())
    return;

  if (!first_result_view || !first_result_view->selected())
    return;

  SearchResult* const first_visible_result = first_result_view->result();

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
  if (base::StartsWith(details, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // Current text in the search_box matches the first result's url.
    SetAutocompleteText(details);
    return;
  }
  if (base::StartsWith(search_text, user_typed_text,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    // Current text in the search_box matches the first result's search result
    // text.
    SetAutocompleteText(search_text);
    return;
  }
  // Current text in the search_box does not match the first result's url or
  // search result text.
  ClearAutocompleteText();
}

void SearchBoxView::ClearAutocompleteText() {
  if (!ShouldProcessAutocomplete())
    return;

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

void SearchBoxView::UpdateTextColor() {
  if (is_app_list_bubble_) {
    // Bubble launcher uses standard text colors (light-on-dark by default).
    search_box()->SetTextColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  } else {
    // Fullscreen launcher uses dark-on-light text by default.
    search_box()->SetTextColor(
        AppListColorProvider::Get()->GetSearchBoxTextColor(
            kDeprecatedSearchBoxTextDefaultColor));
  }
}

void SearchBoxView::UpdatePlaceholderTextAndAccessibleName() {
  if (features::IsProductivityLauncherEnabled()) {
    // Randomly select a placeholder text.
    const PlaceholderTextType placeholder_type = PlaceholderTextType(
        base::RandInt(0, static_cast<int>(PlaceholderTextType::kMaxValue)));

    switch (placeholder_type) {
      case PlaceholderTextType::kShortcuts:
        search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
            IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SHORTCUTS)));
        search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
            is_tablet_mode_
                ? IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_TABLET
                : IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SHORTCUTS)));
        break;
      case PlaceholderTextType::kTabs:
        search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
            IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS)));
        search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
            is_tablet_mode_
                ? IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_TABLET
                : IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TABS)));
        break;
      case PlaceholderTextType::kSettings:
        search_box()->SetPlaceholderText(l10n_util::GetStringFUTF16(
            IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SETTINGS)));
        search_box()->SetAccessibleName(l10n_util::GetStringFUTF16(
            is_tablet_mode_
                ? IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_TABLET
                : IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_TEMPLATE_ACCESSIBILITY_NAME_CLAMSHELL,
            l10n_util::GetStringUTF16(
                IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER_SETTINGS)));
        break;
    }
  } else {
    search_box()->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER));
    search_box()->SetAccessibleName(l10n_util::GetStringUTF16(
        is_tablet_mode_
            ? IDS_APP_LIST_SEARCH_BOX_ACCESSIBILITY_NAME_TABLET
            : IDS_APP_LIST_SEARCH_BOX_ACCESSIBILITY_NAME_CLAMSHELL));
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

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const std::u16string& new_contents) {
  bool current_query_empty = IsTrimmedQueryEmpty(current_query_);
  bool new_contents_empty = IsTrimmedQueryEmpty(new_contents);
  if (current_query_empty && !new_contents_empty) {
    // User enters a new search query. Record the action.
    base::RecordAction(base::UserMetricsAction("AppList_SearchQueryStarted"));
  }

  // Schedule paint to update the focus bar, a part of the search box background
  // that is dependent on whether the query is empty.
  if (current_query_empty != new_contents_empty)
    SchedulePaint();

  current_query_ = new_contents;

  // Update autocomplete text highlight range to track user typed text.
  if (ShouldProcessAutocomplete())
    ResetHighlightRange();
  SearchBoxViewBase::ContentsChanged(sender, new_contents);
  if (app_list_view_) {
    app_list_view_->SetStateFromSearchBoxView(
        IsSearchBoxTrimmedQueryEmpty(), true /*triggered_by_contents_change*/);
  }
}

void SearchBoxView::SetAutocompleteText(
    const std::u16string& autocomplete_text) {
  if (!ShouldProcessAutocomplete())
    return;

  const std::u16string& current_text = search_box()->GetText();
  // Currrent text is a prefix of autocomplete text.
  DCHECK(base::StartsWith(autocomplete_text, current_text,
                          base::CompareCase::INSENSITIVE_ASCII));
  // Don't set autocomplete text if it's the same as current search box text.
  if (autocomplete_text == current_text)
    return;

  const std::u16string& highlighted_text =
      autocomplete_text.substr(highlight_range_.start());

  // Don't set autocomplete text if the highlighted text is the same as before.
  if (highlighted_text == search_box()->GetSelectedText())
    return;

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
}

void SearchBoxView::UpdateQuery(const std::u16string& new_query) {
  search_box()->SetText(new_query);
  ContentsChanged(search_box(), new_query);
}

void SearchBoxView::ClearSearchAndDeactivateSearchBox() {
  if (!is_search_box_active())
    return;

  SetA11yActiveDescendant(absl::nullopt);
  ClearSearch();
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
}

void SearchBoxView::SetA11yActiveDescendant(
    const absl::optional<int32_t>& active_descendant) {
  a11y_active_descendant_ = active_descendant;
  search_box()->NotifyAccessibilityEvent(
      ax::mojom::Event::kActiveDescendantChanged, true);
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
  if (!delegate()->CanSelectSearchResults())
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
      UpdateSearchBoxTextForSelectedResult(
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
      UpdateSearchBoxTextForSelectedResult(
          result_selection_controller_->selected_result()->result());
      break;
  }

  return true;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    // TODO(crbug.com/1216082): Forward scroll events for bubble launcher.
    if (!app_list_view_) {
      NOTIMPLEMENTED_LOG_ONCE();
      return false;
    }
    return app_list_view_->HandleScroll(
        mouse_event.location(), (&mouse_event)->AsMouseWheelEvent()->offset(),
        ui::ET_MOUSEWHEEL);
  }
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

void SearchBoxView::UpdateSearchBoxTextForSelectedResult(
    SearchResult* selected_result) {
  if (selected_result->result_type() ==
      AppListSearchResultType::kInternalPrivacyInfo) {
    // Privacy view should not change the search box text.
    return;
  }

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

void SearchBoxView::Update() {
  search_box()->SetText(
      AppListModelProvider::Get()->search_model()->search_box()->text());
  UpdateButtonsVisibility();
  NotifyQueryChanged();
}

void SearchBoxView::SearchEngineChanged() {
  UpdateSearchIcon();
}

void SearchBoxView::ShowAssistantChanged() {
  SetShowAssistantButton(AppListModelProvider::Get()
                             ->search_model()
                             ->search_box()
                             ->show_assistant_button());
}

bool SearchBoxView::ShouldProcessAutocomplete() {
  // IME sets composition text while the user is typing, so avoid handle
  // autocomplete in this case to avoid conflicts.
  return !(search_box()->IsIMEComposing() && highlight_range_.is_empty());
}

void SearchBoxView::ResetHighlightRange() {
  DCHECK(ShouldProcessAutocomplete());
  const uint32_t text_length = search_box()->GetText().length();
  highlight_range_.set_start(text_length);
  highlight_range_.set_end(text_length);
}

void SearchBoxView::SetupAssistantButton() {
  views::ImageButton* assistant = assistant_button();
  assistant->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(chromeos::kAssistantIcon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                gfx::kGoogleGrey700)));
  std::u16string assistant_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_START_ASSISTANT));
  assistant->SetAccessibleName(assistant_button_label);
  assistant->SetTooltipText(assistant_button_label);
}

}  // namespace ash
