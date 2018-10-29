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
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_controller.h"
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

using ash::ColorProfileType;

namespace app_list {

namespace {

constexpr int kPaddingSearchResult = 16;
constexpr int kSearchBoxBorderWidth = 4;

constexpr SkColor kSearchBoxBorderColor =
    SkColorSetARGB(0x3D, 0xFF, 0xFF, 0xFF);

constexpr int kSearchBoxBorderCornerRadiusSearchResult = 4;
constexpr int kAssistantIconSize = 24;
constexpr int kCloseIconSize = 24;
constexpr int kSearchBoxFocusBorderCornerRadius = 28;

// Range of the fraction of app list from collapsed to peeking that search box
// should change opacity.
constexpr float kOpacityStartFraction = 0.11f;
constexpr float kOpacityEndFraction = 1.0f;

// Minimum amount of characters required to enable autocomplete.
constexpr int kMinimumLengthToAutocomplete = 2;

// Gets the box layout inset horizontal padding for the state of AppListModel.
int GetBoxLayoutPaddingForState(ash::AppListState state) {
  if (state == ash::AppListState::kStateSearchResults)
    return kPaddingSearchResult;
  return search_box::kPadding;
}

float GetAssistantButtonOpacityForState(ash::AppListState state) {
  if (state == ash::AppListState::kStateSearchResults)
    return .0f;
  return 1.f;
}

}  // namespace

SearchBoxView::SearchBoxView(search_box::SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListView* app_list_view)
    : search_box::SearchBoxViewBase(delegate),
      view_delegate_(view_delegate),
      app_list_view_(app_list_view),
      is_new_style_launcher_enabled_(
          app_list_features::IsNewStyleLauncherEnabled()),
      is_app_list_search_autocomplete_enabled_(
          app_list_features::IsAppListSearchAutocompleteEnabled()),
      weak_ptr_factory_(this) {
  set_is_tablet_mode(app_list_view->is_tablet_mode());
  if (app_list_features::IsZeroStateSuggestionsEnabled())
    set_show_close_button_when_active(true);
}

SearchBoxView::~SearchBoxView() {
  search_model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::ClearSearch() {
  search_box::SearchBoxViewBase::ClearSearch();
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
            located_event->AsMouseWheelEvent()->offset().y(),
            ui::ET_MOUSEWHEEL)) {
      return;
    }
  }
  search_box::SearchBoxViewBase::HandleSearchBoxEvent(located_event);
}

void SearchBoxView::ModelChanged() {
  if (search_model_)
    search_model_->search_box()->RemoveObserver(this);

  search_model_ = view_delegate_->GetSearchModel();
  DCHECK(search_model_);
  UpdateSearchIcon();
  search_model_->search_box()->AddObserver(this);

  HintTextChanged();
  OnWallpaperColorsChanged();
  ShowAssistantChanged();
}

void SearchBoxView::UpdateKeyboardVisibility() {
  if (!keyboard::KeyboardController::HasInstance())
    return;
  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  if (!keyboard_controller->IsEnabled() ||
      is_search_box_active() == keyboard_controller->IsKeyboardVisible()) {
    return;
  }

  if (is_search_box_active()) {
    keyboard_controller->ShowKeyboard(false);
    return;
  }

  keyboard_controller->HideKeyboardByUser();
}

void SearchBoxView::UpdateModel(bool initiated_by_user) {
  // Temporarily remove from observer to ignore notifications caused by us.
  search_model_->search_box()->RemoveObserver(this);
  search_model_->search_box()->Update(search_box()->text(), initiated_by_user);
  search_model_->search_box()->SetSelectionModel(
      search_box()->GetSelectionModel());
  search_model_->search_box()->AddObserver(this);
}

void SearchBoxView::UpdateSearchIcon() {
  const gfx::VectorIcon& google_icon =
      is_search_box_active() ? kIcGoogleColorIcon : kIcGoogleBlackIcon;
  const gfx::VectorIcon& icon = search_model_->search_engine_is_google()
                                    ? google_icon
                                    : kIcSearchEngineNotGoogleIcon;
  SetSearchIconImage(gfx::CreateVectorIcon(icon, search_box::kSearchIconSize,
                                           search_box_color()));
}

void SearchBoxView::UpdateSearchBoxBorder() {
  if (search_box()->HasFocus() && !is_search_box_active() &&
      !is_tablet_mode()) {
    // Show a gray ring around search box to indicate that the search box is
    // selected. Do not show it when search box is active, because blinking
    // cursor already indicates that.
    SetBorder(views::CreateRoundedRectBorder(kSearchBoxBorderWidth,
                                             kSearchBoxFocusBorderCornerRadius,
                                             kSearchBoxBorderColor));
    return;
  }

  // Creates an empty border as a placeholder for colored border so that
  // re-layout won't move views below the search box.
  SetBorder(
      views::CreateEmptyBorder(kSearchBoxBorderWidth, kSearchBoxBorderWidth,
                               kSearchBoxBorderWidth, kSearchBoxBorderWidth));
}

void SearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetImage(views::ImageButton::STATE_NORMAL,
                  gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseIconSize,
                                        search_box_color()));
  close->SetVisible(false);
  close->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_LIST_CLEAR_SEARCHBOX));
}

void SearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  back->SetImage(views::ImageButton::STATE_NORMAL,
                 rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
  back->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                          views::ImageButton::ALIGN_MIDDLE);
  back->SetVisible(false);
  base::string16 back_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_BACK));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
}

void SearchBoxView::RecordSearchBoxActivationHistogram(
    ui::EventType event_type) {
  search_box::ActivationSource activation_type;
  switch (event_type) {
    case ui::ET_GESTURE_TAP:
      activation_type = search_box::ActivationSource::kGestureTap;
      break;
    case ui::ET_MOUSE_PRESSED:
      activation_type = search_box::ActivationSource::kMousePress;
      break;
    case ui::ET_KEY_PRESSED:
      activation_type = search_box::ActivationSource::kKeyPress;
      break;
    default:
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchBoxActivated", activation_type);
}

void SearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  app_list_view_->RedirectKeyEventToSearchBox(event);

  if (!CanProcessUpDownKeyTraversal(*event))
    return;

  // If focus is in search box view, up key moves focus to the last element of
  // contents view if new style launcher is not enabled while it moves focus to
  // expand arrow if the feature is enabled. Down key moves focus to the first
  // element of contents view.
  AppListPage* page =
      contents_view_->GetPageView(contents_view_->GetActivePageIndex());
  views::View* arrow_view = contents_view_->expand_arrow_view();
  views::View* v = event->key_code() == ui::VKEY_UP
                       ? (arrow_view && arrow_view->IsFocusable()
                              ? arrow_view
                              : page->GetLastFocusableView())
                       : page->GetFirstFocusableView();

  if (v)
    v->RequestFocus();
  event->SetHandled();
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);
  return false;
}

void SearchBoxView::UpdateBackground(double progress,
                                     ash::AppListState current_state,
                                     ash::AppListState target_state) {
  SetSearchBoxBackgroundCornerRadius(gfx::Tween::LinearIntValueBetween(
      progress, GetSearchBoxBorderCornerRadiusForState(current_state),
      GetSearchBoxBorderCornerRadiusForState(target_state)));
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(current_state),
      GetBackgroundColorForState(target_state));
  UpdateBackgroundColor(color);
}

void SearchBoxView::UpdateLayout(double progress,
                                 ash::AppListState current_state,
                                 ash::AppListState target_state) {
  box_layout()->set_inside_border_insets(
      gfx::Insets(0,
                  gfx::Tween::LinearIntValueBetween(
                      progress, GetBoxLayoutPaddingForState(current_state),
                      GetBoxLayoutPaddingForState(target_state)),
                  0, 0));
  if (show_assistant_button()) {
    assistant_button()->layer()->SetOpacity(gfx::Tween::LinearIntValueBetween(
        progress, GetAssistantButtonOpacityForState(current_state),
        GetAssistantButtonOpacityForState(target_state)));
  }
  InvalidateLayout();
}

int SearchBoxView::GetSearchBoxBorderCornerRadiusForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults &&
      !app_list_view_->is_in_drag()) {
    return kSearchBoxBorderCornerRadiusSearchResult;
  }
  return search_box::kSearchBoxBorderCornerRadius;
}

SkColor SearchBoxView::GetBackgroundColorForState(
    ash::AppListState state) const {
  if (state == ash::AppListState::kStateSearchResults)
    return kCardBackgroundColor;
  return background_color();
}

void SearchBoxView::UpdateOpacity() {
  // The opacity of searchbox is a function of the fractional displacement of
  // the app list from collapsed(0) to peeking(1) state. When the fraction
  // changes from |kOpacityStartFraction| to |kOpaticyEndFraction|, the opacity
  // of searchbox changes from 0.f to 1.0f.
  if (!contents_view_->GetPageView(contents_view_->GetActivePageIndex())
           ->ShouldShowSearchBox()) {
    return;
  }
  const int shelf_height = AppListConfig::instance().shelf_height();
  float fraction =
      std::max<float>(
          0, contents_view_->app_list_view()->GetCurrentAppListHeight() -
                 shelf_height) /
      (AppListConfig::instance().peeking_app_list_height() - shelf_height);

  float opacity =
      std::min(std::max((fraction - kOpacityStartFraction) /
                            (kOpacityEndFraction - kOpacityStartFraction),
                        0.f),
               1.0f);

  AppListView* app_list_view = contents_view_->app_list_view();
  bool should_restore_opacity =
      !app_list_view->is_in_drag() &&
      (app_list_view->app_list_state() != AppListViewState::CLOSED);
  // Restores the opacity of searchbox if the gesture dragging ends.
  this->layer()->SetOpacity(should_restore_opacity ? 1.0f : opacity);
  contents_view_->search_results_page_view()->layer()->SetOpacity(
      should_restore_opacity ? 1.0f : opacity);
}

void SearchBoxView::ShowZeroStateSuggestions() {
  base::string16 empty_query;
  ContentsChanged(search_box(), empty_query);
}

void SearchBoxView::OnWallpaperColorsChanged() {
  GetWallpaperProminentColors(
      base::BindOnce(&SearchBoxView::OnWallpaperProminentColorsReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchBoxView::ProcessAutocomplete() {
  if (!is_app_list_search_autocomplete_enabled_)
    return;

  // Current non-autocompleted text.
  const base::string16& user_typed_text =
      search_box()->text().substr(0, highlight_range_.start());
  if (last_key_pressed_ == ui::VKEY_BACK || last_key_pressed_ == ui::VKEY_UP ||
      last_key_pressed_ == ui::VKEY_DOWN ||
      last_key_pressed_ == ui::VKEY_LEFT ||
      last_key_pressed_ == ui::VKEY_RIGHT ||
      search_model_->results()->item_count() == 0 ||
      user_typed_text.length() < kMinimumLengthToAutocomplete) {
    // Backspace or arrow keys were pressed, no results exist, or current text
    // is too short for a confident autocomplete suggestion.
    return;
  }

  const base::string16& details =
      search_model_->results()->GetItemAt(0)->details();
  const base::string16& search_text =
      search_model_->results()->GetItemAt(0)->title();
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

void SearchBoxView::GetWallpaperProminentColors(
    AppListViewDelegate::GetWallpaperProminentColorsCallback callback) {
  view_delegate_->GetWallpaperProminentColors(std::move(callback));
}

void SearchBoxView::OnWallpaperProminentColorsReceived(
    const std::vector<SkColor>& prominent_colors) {
  if (prominent_colors.empty())
    return;
  DCHECK_EQ(static_cast<size_t>(ColorProfileType::NUM_OF_COLOR_PROFILES),
            prominent_colors.size());

  SetSearchBoxColor(
      prominent_colors[static_cast<int>(ColorProfileType::DARK_MUTED)]);
  UpdateSearchIcon();
  close_button()->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon, kCloseIconSize,
                            search_box_color()));
  search_box()->set_placeholder_text_color(search_box_color());
  UpdateBackgroundColor(background_color());
  SchedulePaint();
}

void SearchBoxView::AcceptAutocompleteText() {
  if (!is_app_list_search_autocomplete_enabled_)
    return;

  if (HasAutocompleteText())
    ContentsChanged(search_box(), search_box()->text());
}

void SearchBoxView::AcceptOneCharInAutocompleteText() {
  if (!is_app_list_search_autocomplete_enabled_)
    return;

  highlight_range_.set_start(highlight_range_.start() + 1);
  highlight_range_.set_end(search_box()->text().length());
  const base::string16 original_text = search_box()->text();
  search_box()->SetText(
      search_box()->text().substr(0, highlight_range_.start()));
  ContentsChanged(search_box(), search_box()->text());
  search_box()->SetText(original_text);
  search_box()->SetSelectionRange(highlight_range_);
}

bool SearchBoxView::HasAutocompleteText() {
  // If the selected range is non-empty, it will either be suggested by
  // autocomplete or selected by the user. If the recorded autocomplete
  // |highlight_range_| matches the selection range, this text is suggested by
  // autocomplete.
  return search_box()->GetSelectedRange() == highlight_range_ &&
         highlight_range_.length() > 0;
}

void SearchBoxView::ClearAutocompleteText() {
  if (!is_app_list_search_autocomplete_enabled_)
    return;

  search_box()->SetText(
      search_box()->text().substr(0, highlight_range_.start()));
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  // Update autocomplete text highlight range to track user typed text.
  if (is_app_list_search_autocomplete_enabled_)
    highlight_range_.set_start(search_box()->text().length());
  search_box::SearchBoxViewBase::ContentsChanged(sender, new_contents);
  app_list_view_->SetStateFromSearchBoxView(
      IsSearchBoxTrimmedQueryEmpty(), true /*triggered_by_contents_change*/);
}

void SearchBoxView::SetAutocompleteText(
    const base::string16& autocomplete_text) {
  if (!is_app_list_search_autocomplete_enabled_)
    return;

  const base::string16& current_text = search_box()->text();
  // Currrent text is a prefix of autocomplete text.
  DCHECK(base::StartsWith(autocomplete_text, current_text,
                          base::CompareCase::INSENSITIVE_ASCII));
  // Don't set autocomplete text if it's the same as current search box text.
  if (autocomplete_text.length() == current_text.length())
    return;

  const base::string16& user_typed_text =
      current_text.substr(0, highlight_range_.start());
  const base::string16& highlighted_text =
      autocomplete_text.substr(highlight_range_.start());
  search_box()->SetText(user_typed_text + highlighted_text);
  highlight_range_.set_end(autocomplete_text.length());
  search_box()->SelectRange(highlight_range_);
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  if (search_box()->HasFocus() && is_search_box_active() &&
      !search_box()->text().empty() &&
      is_app_list_search_autocomplete_enabled_) {
    // If the search box has no text in it currently, autocomplete should not
    // work.
    last_key_pressed_ = key_event.key_code();
    if (key_event.type() == ui::ET_KEY_PRESSED &&
        key_event.key_code() != ui::VKEY_BACK) {
      if (key_event.key_code() == ui::VKEY_TAB) {
        AcceptAutocompleteText();
      } else if ((key_event.key_code() == ui::VKEY_UP ||
                  key_event.key_code() == ui::VKEY_DOWN ||
                  key_event.key_code() == ui::VKEY_LEFT ||
                  key_event.key_code() == ui::VKEY_RIGHT) &&
                 HasAutocompleteText()) {
        ClearAutocompleteText();
        return true;
      } else {
        const base::string16 pending_text = search_box()->GetSelectedText();
        // Hitting the next key in the autocompete suggestion continues
        // autocomplete suggestion. If the selected range doesn't match the
        // recorded highlight range, the selection should be overwritten.
        if (!pending_text.empty() &&
            key_event.GetCharacter() == pending_text[0] &&
            pending_text.length() == highlight_range_.length()) {
          AcceptOneCharInAutocompleteText();
          return true;
        }
      }
    }
  }
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      key_event.key_code() == ui::VKEY_RETURN) {
    if (!IsSearchBoxTrimmedQueryEmpty()) {
      // Hitting Enter when focus is on search box opens the first result.
      ui::KeyEvent event(key_event);
      views::View* first_result_view =
          contents_view_->search_results_page_view()->first_result_view();
      if (first_result_view)
        first_result_view->OnKeyEvent(&event);
      return true;
    }

    if (!is_search_box_active()) {
      SetSearchBoxActive(true, key_event.type());
      return true;
    }
    return false;
  }

  if (CanProcessLeftRightKeyTraversal(key_event))
    return ProcessLeftRightKeyTraversalForTextfield(search_box(), key_event);
  return false;
}

bool SearchBoxView::HandleMouseEvent(views::Textfield* sender,
                                     const ui::MouseEvent& mouse_event) {
  if (mouse_event.type() == ui::ET_MOUSEWHEEL) {
    return app_list_view_->HandleScroll(
        (&mouse_event)->AsMouseWheelEvent()->offset().y(), ui::ET_MOUSEWHEEL);
  }
  if (mouse_event.type() == ui::ET_MOUSE_PRESSED)
    AcceptAutocompleteText();
  return search_box::SearchBoxViewBase::HandleMouseEvent(sender, mouse_event);
}

bool SearchBoxView::HandleGestureEvent(views::Textfield* sender,
                                       const ui::GestureEvent& gesture_event) {
  if (gesture_event.type() == ui::ET_GESTURE_TAP)
    AcceptAutocompleteText();
  return search_box::SearchBoxViewBase::HandleGestureEvent(sender,
                                                           gesture_event);
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (close_button() && sender == close_button()) {
    SetSearchBoxActive(false, ui::ET_UNKNOWN);
  }
  search_box::SearchBoxViewBase::ButtonPressed(sender, event);
}

void SearchBoxView::HintTextChanged() {
  const app_list::SearchBoxModel* search_box_model =
      search_model_->search_box();
  search_box()->set_placeholder_text(search_box_model->hint_text());
  search_box()->SetAccessibleName(search_box_model->accessible_name());
  SchedulePaint();
}

void SearchBoxView::SelectionModelChanged() {
  search_box()->SelectSelectionModel(
      search_model_->search_box()->selection_model());
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

void SearchBoxView::SetupAssistantButton() {
  if (search_model_ && !search_model_->search_box()->show_assistant_button()) {
    return;
  }

  views::ImageButton* assistant = assistant_button();
  assistant->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kAssistantIcon, kAssistantIconSize,
                            search_box_color()));
  assistant->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_LIST_START_ASSISTANT));
}

}  // namespace app_list
