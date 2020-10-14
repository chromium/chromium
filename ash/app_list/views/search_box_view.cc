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
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kSearchBoxFocusRingWidth = 2;

// Padding between the focus ring and the search box view
constexpr int kSearchBoxFocusRingPadding = 4;

constexpr SkColor kSearchBoxFocusRingColor = gfx::kGoogleBlue300;

constexpr int kSearchBoxFocusRingCornerRadius = 28;

// Minimum amount of characters required to enable autocomplete.
constexpr int kMinimumLengthToAutocomplete = 2;

float GetAssistantButtonOpacityForState(AppListState state) {
  if (state == AppListState::kStateSearchResults)
    return .0f;
  return 1.f;
}

bool IsTrimmedQueryEmpty(const base::string16& query) {
  base::string16 trimmed_query;
  base::TrimWhitespace(query, base::TrimPositions::TRIM_ALL, &trimmed_query);
  return trimmed_query.empty();
}

}  // namespace

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : SearchBoxViewBase(delegate),
      view_delegate_(view_delegate),
      app_list_view_(app_list_view),
      is_app_list_search_autocomplete_enabled_(
          app_list_features::IsAppListSearchAutocompleteEnabled()) {}

SearchBoxView::~SearchBoxView() {
  search_model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::Init(bool is_tablet_mode) {
  is_tablet_mode_ = is_tablet_mode;
  if (app_list_features::IsZeroStateSuggestionsEnabled())
    set_show_close_button_when_active(true);
  SearchBoxViewBase::Init();
  UpdatePlaceholderTextAndAccessibleName();
  current_query_ = search_box()->GetText();
}

void SearchBoxView::OnTabletModeChanged(bool started) {
  is_tablet_mode_ = started;

  UpdateKeyboardVisibility();
  // Search box accessible name may change depending on tablet mode state.
  UpdatePlaceholderTextAndAccessibleName();
  UpdateSearchBoxBorder();
}

void SearchBoxView::ResetForShow() {
  // Avoid clearing an already inactive SearchBox in tablet mode because this
  // causes suggested chips to flash (http://crbug.com/979594).
  if (!is_search_box_active() && is_tablet_mode_)
    return;

  ClearSearchAndDeactivateSearchBox();
  SetSearchBoxBackgroundCornerRadius(
      GetSearchBoxBorderCornerRadiusForState(contents_view_->GetActiveState()));
}

void SearchBoxView::ClearSearch() {
  SearchBoxViewBase::ClearSearch();
  current_query_.clear();
  app_list_view_->SetStateFromSearchBoxView(
      true, false /*triggered_by_contents_change*/);
}

views::View* SearchBoxView::GetSelectedViewInContentsView() {
  if (!contents_view_)
    return nullptr;
  return contents_view_->GetSelectedView();
}

void SearchBoxView::HandleSearchBoxEvent(ui::LocatedEvent* located_event) {
  if (located_event->type() == ui::ET_MOUSEWHEEL) {
    if (!app_list_view_->HandleScroll(
            located_event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL)) {
      return;
    }
  }
  SearchBoxViewBase::HandleSearchBoxEvent(located_event);
}

void SearchBoxView::ModelChanged() {
  if (search_model_)
    search_model_->search_box()->RemoveObserver(this);

  search_model_ = view_delegate_->GetSearchModel();
  DCHECK(search_model_);
  UpdateSearchIcon();
  search_model_->search_box()->AddObserver(this);

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
  // Temporarily remove from observer to ignore notifications caused by us.
  search_model_->search_box()->RemoveObserver(this);
  search_model_->search_box()->Update(search_box()->GetText(),
                                      initiated_by_user);
  search_model_->search_box()->AddObserver(this);
}

void SearchBoxView::UpdateSearchIcon() {
  const gfx::VectorIcon& google_icon =
      is_search_box_active() ? kGoogleColorIcon : kGoogleBlackIcon;
  const gfx::VectorIcon& icon = search_model_->search_engine_is_google()
                                    ? google_icon
                                    : kSearchEngineNotGoogleIcon;
  SetSearchIconImage(
      gfx::CreateVectorIcon(icon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                SkColorSetARGB(0xDE, 0x00, 0x00, 0x00))));
}

void SearchBoxView::UpdateSearchBoxBorder() {
  // Creates an empty border to create a region for the focus ring to appear.
  SetBorder(views::CreateEmptyBorder(gfx::Insets(GetFocusRingSpacing())));
}

void SearchBoxView::OnPaintBackground(gfx::Canvas* canvas) {
  // Paints the focus ring if the search box is focused.
  if (search_box()->HasFocus() && !is_search_box_active() &&
      view_delegate_->KeyboardTraversalEngaged()) {
    gfx::Rect bounds = GetContentsBounds();
    bounds.Inset(-kSearchBoxFocusRingPadding, -kSearchBoxFocusRingPadding);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kSearchBoxFocusRingColor);
    flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    flags.setStrokeWidth(kSearchBoxFocusRingWidth);
    canvas->DrawRoundRect(bounds, kSearchBoxFocusRingCornerRadius, flags);
  }
}

const char* SearchBoxView::GetClassName() const {
  return "SearchBoxView";
}

// static
int SearchBoxView::GetFocusRingSpacing() {
  return kSearchBoxFocusRingWidth + kSearchBoxFocusRingPadding;
}

void SearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                gfx::kGoogleGrey700)));
  close->SetVisible(false);
  base::string16 close_button_label(
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
  base::string16 back_button_label(
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

void SearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  app_list_view_->RedirectKeyEventToSearchBox(event);

  if (!IsUnhandledUpDownKeyEvent(*event))
    return;

  // Handles arrow key events from the search box while the search box is
  // inactive. This covers both folder traversal and apps grid traversal. Search
  // result traversal is handled in |HandleKeyEvent|
  AppListPage* page =
      contents_view_->GetPageView(contents_view_->GetActivePageIndex());
  views::View* arrow_view = contents_view_->expand_arrow_view();
  views::View* next_view = nullptr;

  if (event->key_code() == ui::VKEY_UP) {
    if (arrow_view && arrow_view->IsFocusable())
      next_view = arrow_view;
    else
      next_view = page->GetLastFocusableView();
  } else {
    next_view = page->GetFirstFocusableView();
  }

  if (next_view)
    next_view->RequestFocus();
  event->SetHandled();
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

void SearchBoxView::UpdateBackground(double progress,
                                     AppListState current_state,
                                     AppListState target_state) {
  SetSearchBoxBackgroundCornerRadius(gfx::Tween::LinearIntValueBetween(
      progress, GetSearchBoxBorderCornerRadiusForState(current_state),
      GetSearchBoxBorderCornerRadiusForState(target_state)));
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(current_state),
      GetBackgroundColorForState(target_state));
  UpdateBackgroundColor(color);
  search_box()->SetTextColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
}

void SearchBoxView::UpdateLayout(double progress,
                                 AppListState current_state,
                                 int current_state_height,
                                 AppListState target_state,
                                 int target_state_height) {
  // Horizontal margins are selected to match search box icon's vertical
  // margins.
  const int horizontal_spacing = gfx::Tween::LinearIntValueBetween(
      progress, (current_state_height - kSearchBoxIconSize) / 2,
      (target_state_height - kSearchBoxIconSize) / 2);
  const int horizontal_right_padding =
      horizontal_spacing - (kSearchBoxButtonSizeDip - kSearchBoxIconSize) / 2;
  box_layout()->set_inside_border_insets(
      gfx::Insets(0, horizontal_spacing, 0, horizontal_right_padding));
  box_layout()->set_between_child_spacing(horizontal_spacing);
  if (show_assistant_button()) {
    assistant_button()->layer()->SetOpacity(gfx::Tween::LinearIntValueBetween(
        progress, GetAssistantButtonOpacityForState(current_state),
        GetAssistantButtonOpacityForState(target_state)));
  }
  InvalidateLayout();
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    AppListState state) const {
  if (state == AppListState::kStateSearchResults &&
      !app_list_view_->is_in_drag()) {
    return kSearchBoxBorderCornerRadiusSearchResult;
  }
  return kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(AppListState state) const {
  if (state == AppListState::kStateSearchResults)
    return AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor();
  return AppListColorProvider::Get()->GetSearchBoxBackgroundColor();
}

void SearchBoxView::ShowZeroStateSuggestions() {
  base::RecordAction(
      base::UserMetricsAction("AppList_ShowZeroStateSuggestions"));
  base::string16 empty_query;
  ContentsChanged(search_box(), empty_query);
}

void SearchBoxView::OnWallpaperColorsChanged() {
  UpdateSearchIcon();
  AppListColorProvider* app_list_color_provider = AppListColorProvider::Get();
  search_box()->set_placeholder_text_color(
      app_list_color_provider->GetSearchBoxPlaceholderTextColor());
  search_box()->SetTextColor(app_list_color_provider->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
  if (features::IsDarkLightModeEnabled()) {
    UpdateBackgroundColor(
        app_list_color_provider->GetSearchBoxBackgroundColor());
  }
  SchedulePaint();
}

void SearchBoxView::ProcessAutocomplete() {
  if (!ShouldProcessAutocomplete())
    return;

  SearchResultBaseView* const first_result_view =
      contents_view_->search_results_page_view()->first_result_view();
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
  const base::string16& user_typed_text =
      search_box()->GetText().substr(0, highlight_range_.start());
  if (last_key_pressed_ == ui::VKEY_BACK ||
      last_key_pressed_ == ui::VKEY_DELETE || IsArrowKey(last_key_pressed_) ||
      !first_visible_result ||
      user_typed_text.length() < kMinimumLengthToAutocomplete) {
    // If the suggestion was rejected, no results exist, or current text
    // is too short for a confident autocomplete suggestion.
    return;
  }

  const base::string16& details = first_visible_result->details();
  const base::string16& search_text = first_visible_result->title();
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

void SearchBoxView::UpdatePlaceholderTextAndAccessibleName() {
  search_box()->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_BOX_PLACEHOLDER));
  search_box()->SetAccessibleName(l10n_util::GetStringUTF16(
      is_tablet_mode_ ? IDS_APP_LIST_SEARCH_BOX_ACCESSIBILITY_NAME_TABLET
                      : IDS_APP_LIST_SEARCH_BOX_ACCESSIBILITY_NAME_CLAMSHELL));
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

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  if (IsTrimmedQueryEmpty(current_query_) && !IsSearchBoxTrimmedQueryEmpty()) {
    // User enters a new search query. Record the action.
    base::RecordAction(base::UserMetricsAction("AppList_SearchQueryStarted"));
  }

  current_query_ = new_contents;

  // Update autocomplete text highlight range to track user typed text.
  if (ShouldProcessAutocomplete())
    ResetHighlightRange();
  SearchBoxViewBase::ContentsChanged(sender, new_contents);
  app_list_view_->SetStateFromSearchBoxView(
      IsSearchBoxTrimmedQueryEmpty(), true /*triggered_by_contents_change*/);
}

void SearchBoxView::SetAutocompleteText(
    const base::string16& autocomplete_text) {
  if (!ShouldProcessAutocomplete())
    return;

  const base::string16& current_text = search_box()->GetText();
  // Currrent text is a prefix of autocomplete text.
  DCHECK(base::StartsWith(autocomplete_text, current_text,
                          base::CompareCase::INSENSITIVE_ASCII));
  // Don't set autocomplete text if it's the same as current search box text.
  if (autocomplete_text == current_text)
    return;

  const base::string16& highlighted_text =
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

void SearchBoxView::UpdateQuery(const base::string16& new_query) {
  search_box()->SetText(new_query);
  ContentsChanged(search_box(), new_query);
}

void SearchBoxView::ClearSearchAndDeactivateSearchBox() {
  if (!is_search_box_active())
    return;

  view_delegate_->LogSearchAbandonHistogram();

  contents_view_->search_results_page_view()
      ->result_selection_controller()
      ->ClearSelection();
  ClearSearch();
  SetSearchBoxActive(false, ui::ET_UNKNOWN);
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
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
  if (!contents_view_->search_results_page_view()->first_result_view())
    return false;

  ResultSelectionController* selection_controller =
      contents_view_->search_results_page_view()->result_selection_controller();

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
    if (selection_controller->MoveSelection(key_event) ==
        ResultSelectionController::MoveResult::kResultChanged) {
      UpdateSearchBoxTextForSelectedResult(
          selection_controller->selected_result()->result());
    }
    return true;
  }

  // Handle return - opens the selected result.
  if (key_event.key_code() == ui::VKEY_RETURN) {
    // Hitting Enter when focus is on search box opens the selected result.
    ui::KeyEvent event(key_event);
    SearchResultBaseView* selected_result =
        selection_controller->selected_result();
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
        selection_controller->selected_result();
    if (selected_result && selected_result->result())
      selected_result->OnKeyEvent(&event);
    // Reset the selected result to the default result.
    selection_controller->ResetSelection(nullptr, true /* default_selection */);
    search_box()->SetText(base::string16());
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
       selection_controller->selected_location_details() &&
       selection_controller->selected_location_details()
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
      selection_controller->MoveSelection(key_event);
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
      selection_controller->ClearSelection();

      DCHECK(close_button()->GetVisible());
      close_button()->RequestFocus();
      break;
    case ResultSelectionController::MoveResult::kResultChanged:
      UpdateSearchBoxTextForSelectedResult(
          selection_controller->selected_result()->result());
      break;
  }

  return true;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    return app_list_view_->HandleScroll(
        (&mouse_event)->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL);
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

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (close_button() && sender == close_button()) {
    view_delegate_->LogSearchAbandonHistogram();
    SetSearchBoxActive(false, ui::ET_UNKNOWN);
  }
  SearchBoxViewBase::ButtonPressed(sender, event);
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
    // If set, use details to ensure url results fill url.
    search_box()->SetText(selected_result->details());
  } else {
    search_box()->SetText(selected_result->title());
  }
}

void SearchBoxView::Update() {
  search_box()->SetText(search_model_->search_box()->text());
  UpdateButtonsVisisbility();
  NotifyQueryChanged();
}

void SearchBoxView::SearchEngineChanged() {
  UpdateSearchIcon();
}

void SearchBoxView::ShowAssistantChanged() {
  if (search_model_) {
    SetShowAssistantButton(
        search_model_->search_box()->show_assistant_button());
  }
}

bool SearchBoxView::ShouldProcessAutocomplete() {
  // IME sets composition text while the user is typing, so avoid handle
  // autocomplete in this case to avoid conflicts.
  return is_app_list_search_autocomplete_enabled_ &&
         !(search_box()->IsIMEComposing() && highlight_range_.is_empty());
}

void SearchBoxView::ResetHighlightRange() {
  DCHECK(ShouldProcessAutocomplete());
  const uint32_t text_length = search_box()->GetText().length();
  highlight_range_.set_start(text_length);
  highlight_range_.set_end(text_length);
}

void SearchBoxView::SetupAssistantButton() {
  if (search_model_ && !search_model_->search_box()->show_assistant_button()) {
    return;
  }

  views::ImageButton* assistant = assistant_button();
  assistant->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(chromeos::kAssistantIcon, kSearchBoxIconSize,
                            AppListColorProvider::Get()->GetSearchBoxIconColor(
                                gfx::kGoogleGrey700)));
  base::string16 assistant_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_START_ASSISTANT));
  assistant->SetAccessibleName(assistant_button_label);
  assistant->SetTooltipText(assistant_button_label);
}

}  // namespace ash
