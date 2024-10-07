// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_caps_lock_position.h"
#include "ash/picker/model/picker_emoji_history_model.h"
#include "ash/picker/model/picker_emoji_suggester.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_action_on_next_focus_request.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl.h"
#include "ash/picker/picker_caps_lock_bubble_controller.h"
#include "ash/picker/picker_client.h"
#include "ash/picker/picker_copy_media.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/picker_paste_request.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_suggestions_controller.h"
#include "ash/picker/picker_transform_case.h"
#include "ash/picker/search/picker_search_controller.h"
#include "ash/picker/views/picker_caps_lock_state_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_positioning.h"
#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_widget.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/sha1.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

bool g_feature_tour_enabled = true;

// When spoken feedback is enabled, closing the widget after an insert is
// delayed by this amount.
constexpr base::TimeDelta kCloseWidgetDelay = base::Milliseconds(200);

// Time to wait for a focus event after triggering caps lock.
constexpr base::TimeDelta kCapsLockRequestTimeout = base::Seconds(1);

constexpr int kCapsLockMinimumTopDisplayCount = 5;
constexpr float kCapsLockRatioThresholdForTop = 0.8;
constexpr float kCapsLockRatioThresholdForBottom = 0.2;

constexpr std::string_view kSupportUrl =
    "https://support.google.com/chromebook?p=dugong";

ui::TextInputClient* GetFocusedTextInputClient() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method || !input_method->GetTextInputClient()) {
    return nullptr;
  }
  return input_method->GetTextInputClient();
}

// Gets the current caret bounds in universal screen coordinates in DIP. Returns
// an empty rect if there is no active caret or the caret bounds can't be
// determined (e.g. no focused input field).
gfx::Rect GetCaretBounds() {
  if (ui::TextInputClient* client = GetFocusedTextInputClient()) {
    return client->GetCaretBounds();
  }
  return gfx::Rect();
}

// Gets the current cursor point in universal screen coordinates in DIP.
gfx::Point GetCursorPoint() {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

// Gets the bounds of the current focused window in universal screen coordinates
// in DIP. Returns an empty rect if there is no currently focused window.
gfx::Rect GetFocusedWindowBounds() {
  return window_util::GetFocusedWindow()
             ? window_util::GetFocusedWindow()->GetBoundsInScreen()
             : gfx::Rect();
}

input_method::ImeKeyboard& GetImeKeyboard() {
  auto* input_method_manager = input_method::InputMethodManager::Get();
  CHECK(input_method_manager);
  input_method::ImeKeyboard* ime_keyboard =
      input_method_manager->GetImeKeyboard();
  CHECK(ime_keyboard);
  return *ime_keyboard;
}

// The user can ask to insert rich media, a clipboard item, or insert nothing.
using InsertionContent =
    std::variant<PickerRichMedia, PickerClipboardResult, std::monostate>;

InsertionContent GetInsertionContentForResult(
    const PickerSearchResult& result) {
  using ReturnType = InsertionContent;
  return std::visit(
      base::Overloaded{
          [](const PickerTextResult& data) -> ReturnType {
            return PickerTextMedia(data.primary_text);
          },
          [](const PickerEmojiResult& data) -> ReturnType {
            return PickerTextMedia(data.text);
          },
          [](const PickerClipboardResult& data) -> ReturnType { return data; },
          [](const PickerBrowsingHistoryResult& data) -> ReturnType {
            return PickerLinkMedia(data.url, base::UTF16ToUTF8(data.title));
          },
          [](const PickerGifResult& data) -> ReturnType {
            return PickerImageMedia(data.full_url, data.full_dimensions,
                                    data.content_description);
          },
          [](const PickerLocalFileResult& data) -> ReturnType {
            return PickerLocalFileMedia(data.file_path);
          },
          [](const PickerDriveFileResult& data) -> ReturnType {
            return PickerLinkMedia(data.url, base::UTF16ToUTF8(data.title));
          },
          [](const PickerCategoryResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerSearchRequestResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerEditorResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerLobsterResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerNewWindowResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerCapsLockResult& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerCaseTransformResult& data) -> ReturnType {
            return std::monostate();
          },
      },
      result);
}

std::vector<PickerSearchResultsSection> CreateSingleSectionForCategoryResults(
    PickerSectionType section_type,
    std::vector<PickerSearchResult> results) {
  if (results.empty()) {
    return {};
  }
  return {PickerSearchResultsSection(section_type, std::move(results),
                                     /*has_more_results=*/false)};
}

std::u16string TransformText(std::u16string_view text,
                             PickerCaseTransformResult::Type type) {
  switch (type) {
    case PickerCaseTransformResult::Type::kUpperCase:
      return PickerTransformToUpperCase(text);
    case PickerCaseTransformResult::Type::kLowerCase:
      return PickerTransformToLowerCase(text);
    case PickerCaseTransformResult::Type::kTitleCase:
      return PickerTransformToTitleCase(text);
  }
  NOTREACHED();
}

void OpenLink(const GURL& url) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void OpenFile(const base::FilePath& path) {
  ash::NewWindowDelegate::GetPrimary()->OpenFile(path);
}

GURL GetUrlForNewWindow(PickerNewWindowResult::Type type) {
  switch (type) {
    case PickerNewWindowResult::Type::kDoc:
      return GURL("https://docs.new");
    case PickerNewWindowResult::Type::kSheet:
      return GURL("https://sheets.new");
    case PickerNewWindowResult::Type::kSlide:
      return GURL("https://slides.new");
    case PickerNewWindowResult::Type::kChrome:
      return GURL("chrome://newtab");
  }
}

ui::EmojiPickerCategory EmojiResultTypeToCategory(
    PickerEmojiResult::Type type) {
  switch (type) {
    case PickerEmojiResult::Type::kEmoji:
      return ui::EmojiPickerCategory::kEmojis;
    case PickerEmojiResult::Type::kSymbol:
      return ui::EmojiPickerCategory::kSymbols;
    case PickerEmojiResult::Type::kEmoticon:
      return ui::EmojiPickerCategory::kEmoticons;
  }
}

}  // namespace

PickerController::PickerController()
    : caps_lock_bubble_controller_(&GetImeKeyboard()),
      asset_fetcher_(std::make_unique<PickerAssetFetcherImpl>(this)),
      search_controller_(kBurnInPeriod) {}

PickerController::~PickerController() {
  // `widget_` depends on `this`. Destroy the widget synchronously to avoid a
  // dangling pointer.
  if (widget_) {
    widget_->CloseNow();
  }
}

bool PickerController::IsFeatureEnabled() {
  return features::IsPickerUpdateEnabled();
}

void PickerController::DisableFeatureTourForTesting() {
  CHECK_IS_TEST();
  g_feature_tour_enabled = false;
}

void PickerController::SetClient(PickerClient* client) {
  // `PickerSearchController` may depend on the current client via
  // `StartSearch`. Stop the search before changing the `client`. This may send
  // a `StopSearch` call to the current `client_`.
  search_controller_.StopSearch();
  client_ = client;
}

void PickerController::OnClientPrefsSet(PrefService* prefs) {
  if (client_ == nullptr) {
    return;
  }

  search_controller_.LoadEmojiLanguagesFromPrefs(prefs);
}

void PickerController::ToggleWidget(
    const base::TimeTicks trigger_event_timestamp) {
  if (!IsFeatureEnabled()) {
    return;
  }

  // Show the feature tour if it's the first time this feature is used.
  if (PrefService* prefs = GetPrefs();
      g_feature_tour_enabled && prefs &&
      feature_tour_.MaybeShowForFirstUse(
          prefs,
          client_->IsEligibleForEditor()
              ? PickerFeatureTour::EditorStatus::kEligible
              : PickerFeatureTour::EditorStatus::kNotEligible,
          base::BindRepeating(OpenLink, GURL(kSupportUrl)),
          base::BindRepeating(&PickerController::ShowWidgetPostFeatureTour,
                              weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }

  if (widget_) {
    CloseWidget();
  } else {
    ShowWidget(trigger_event_timestamp, WidgetTriggerSource::kDefault);
  }
}

std::vector<PickerCategory> PickerController::GetAvailableCategories() {
  return session_ == nullptr ? std::vector<PickerCategory>{}
                             : session_->model.GetAvailableCategories();
}

void PickerController::GetZeroStateSuggestedResults(
    SuggestedResultsCallback callback) {
  CHECK(client_);
  suggestions_controller_.GetSuggestions(*client_, session_->model,
                                         std::move(callback));
}

void PickerController::GetResultsForCategory(PickerCategory category,
                                             SearchResultsCallback callback) {
  const PickerSectionType section_type =
      (category == PickerCategory::kUnitsMaths ||
       category == PickerCategory::kDatesTimes)
          ? PickerSectionType::kExamples
          : PickerSectionType::kNone;

  CHECK(client_);
  suggestions_controller_.GetSuggestionsForCategory(
      *client_, category,
      base::BindRepeating(CreateSingleSectionForCategoryResults, section_type)
          .Then(std::move(callback)));
}

void PickerController::StartSearch(std::u16string_view query,
                                   std::optional<PickerCategory> category,
                                   SearchResultsCallback callback) {
  CHECK(session_);
  CHECK(client_);
  search_controller_.StartSearch(
      client_, query, std::move(category),
      {
          .available_categories = GetAvailableCategories(),
          .caps_lock_state_to_search = !session_->model.is_caps_lock_enabled(),
          .search_case_transforms =
              session_->model.GetMode() == PickerModeType::kHasSelection,
      },
      std::move(callback));
}

void PickerController::StopSearch() {
  search_controller_.StopSearch();
}

void PickerController::StartEmojiSearch(std::u16string_view query,
                                        EmojiSearchResultsCallback callback) {
  search_controller_.StartEmojiSearch(GetPrefs(), query, std::move(callback));
}

void PickerController::CloseWidgetThenInsertResultOnNextFocus(
    const PickerSearchResult& result) {
  InsertResultOnNextFocus(result);

  client_->Announce(
      l10n_util::GetStringUTF16(IDS_PICKER_INSERTION_ANNOUNCEMENT_TEXT));

  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    close_widget_delay_timer_.Start(
        FROM_HERE, kCloseWidgetDelay,
        base::BindOnce(&PickerController::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    CloseWidget();
  }
}

void PickerController::OpenResult(const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [](const PickerTextResult& data) { NOTREACHED(); },
          [](const PickerEmojiResult& data) { NOTREACHED(); },
          [](const PickerGifResult& data) { NOTREACHED(); },
          [](const PickerClipboardResult& data) { NOTREACHED(); },
          [&](const PickerBrowsingHistoryResult& data) {
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kOpenLink);
            OpenLink(data.url);
          },
          [&](const PickerLocalFileResult& data) {
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kOpenFile);
            OpenFile(data.file_path);
          },
          [&](const PickerDriveFileResult& data) {
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kOpenLink);
            OpenLink(data.url);
          },
          [](const PickerCategoryResult& data) { NOTREACHED(); },
          [](const PickerSearchRequestResult& data) { NOTREACHED(); },
          [](const PickerEditorResult& data) { NOTREACHED(); },
          [](const PickerLobsterResult& data) { NOTREACHED(); },
          [&](const PickerNewWindowResult& data) {
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kCreate);
            OpenLink(GetUrlForNewWindow(data.type));
          },
          [&](const PickerCapsLockResult& data) {
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kFormat);
            caps_lock_request_ =
                std::make_unique<PickerActionOnNextFocusRequest>(
                    widget_->GetInputMethod(), kCapsLockRequestTimeout,
                    base::BindOnce(
                        [](bool enabled) {
                          GetImeKeyboard().SetCapsLockEnabled(enabled);
                        },
                        data.enabled),
                    base::BindOnce(
                        [](bool enabled) {
                          GetImeKeyboard().SetCapsLockEnabled(enabled);
                        },
                        data.enabled));
          },
          [&](const PickerCaseTransformResult& data) {
            if (!session_) {
              return;
            }
            session_->session_metrics.SetOutcome(
                PickerSessionMetrics::SessionOutcome::kFormat);
            std::u16string_view selected_text = session_->model.selected_text();
            InsertResultOnNextFocus(
                PickerTextResult(TransformText(selected_text, data.type),
                                 PickerTextResult::Source::kCaseTransform));
          },
      },
      result);
}

void PickerController::ShowEmojiPicker(ui::EmojiPickerCategory category,
                                       std::u16string_view query) {
  ui::ShowEmojiPanelInSpecificMode(category,
                                   ui::EmojiPickerFocusBehavior::kAlwaysShow,
                                   base::UTF16ToUTF8(query));
}

void PickerController::ShowEditor(std::optional<std::string> preset_query_id,
                                  std::optional<std::string> freeform_text) {
  if (!show_editor_callback_.is_null()) {
    std::move(show_editor_callback_)
        .Run(std::move(preset_query_id), std::move(freeform_text));
  }
}

// TODO: b:370885630 - Considers making selected_text as an argument of this
// method.
void PickerController::ShowLobster(std::optional<std::string> freeform_text) {
  if (!show_lobster_callback_.is_null()) {
    std::move(show_lobster_callback_)
        .Run(session_ != nullptr && session_->model.selected_text() != u""
                 ? base::UTF16ToUTF8(session_->model.selected_text())
                 : std::move(freeform_text));
  }
}

PickerAssetFetcher* PickerController::GetAssetFetcher() {
  return asset_fetcher_.get();
}

PickerSessionMetrics& PickerController::GetSessionMetrics() {
  return session_->session_metrics;
}

PickerActionType PickerController::GetActionForResult(
    const PickerSearchResult& result) {
  CHECK(session_);
  const PickerModeType mode = session_->model.GetMode();
  return std::visit(
      base::Overloaded{
          [mode](const PickerTextResult& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerEmojiResult& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerGifResult& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerClipboardResult& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerBrowsingHistoryResult& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [mode](const PickerLocalFileResult& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [mode](const PickerDriveFileResult& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [](const PickerCategoryResult& data) {
            return PickerActionType::kDo;
          },
          [](const PickerSearchRequestResult& data) {
            return PickerActionType::kDo;
          },
          [](const PickerEditorResult& data) {
            return PickerActionType::kCreate;
          },
          [](const PickerLobsterResult& data) {
            return PickerActionType::kCreate;
          },
          [](const PickerNewWindowResult& data) {
            return PickerActionType::kDo;
          },
          [](const PickerCapsLockResult& data) {
            return PickerActionType::kDo;
          },
          [&](const PickerCaseTransformResult& data) {
            return PickerActionType::kDo;
          }},
      result);
}

std::vector<PickerEmojiResult> PickerController::GetSuggestedEmoji() {
  CHECK(session_);
  return session_->emoji_suggester.GetSuggestedEmoji();
}

bool PickerController::IsGifsEnabled() {
  CHECK(session_);
  return session_->model.IsGifsEnabled();
}

PrefService* PickerController::GetPrefs() {
  CHECK(client_);
  return client_->GetPrefs();
}

PickerModeType PickerController::GetMode() {
  CHECK(session_);
  return session_->model.GetMode();
}

void PickerController::OnViewIsDeleting(views::View* view) {
  view_observation_.Reset();

  session_.reset();
}

scoped_refptr<network::SharedURLLoaderFactory>
PickerController::GetSharedURLLoaderFactory() {
  return client_->GetSharedURLLoaderFactory();
}

void PickerController::FetchFileThumbnail(const base::FilePath& path,
                                          const gfx::Size& size,
                                          FetchFileThumbnailCallback callback) {
  client_->FetchFileThumbnail(path, size, std::move(callback));
}

PickerController::Session::Session(
    PrefService* prefs,
    ui::TextInputClient* focused_client,
    input_method::ImeKeyboard* ime_keyboard,
    PickerModel::EditorStatus editor_status,
    PickerModel::LobsterStatus lobster_status,
    PickerEmojiSuggester::GetNameCallback get_name)
    : model(prefs, focused_client, ime_keyboard, editor_status, lobster_status),
      emoji_history_model(prefs),
      emoji_suggester(&emoji_history_model, std::move(get_name)),
      session_metrics(prefs) {
  session_metrics.OnStartSession(focused_client);
  feature_usage_metrics.StartUsage();
}

PickerController::Session::~Session() {
  feature_usage_metrics.StopUsage();
}

void PickerController::ShowWidget(base::TimeTicks trigger_event_timestamp,
                                  WidgetTriggerSource trigger_source) {
  show_editor_callback_ = client_->CacheEditorContext();
  show_lobster_callback_ = client_->GetShowLobsterCallback();

  ui::TextInputClient* focused_client = GetFocusedTextInputClient();
  input_method::ImeKeyboard& keyboard = GetImeKeyboard();

  if (focused_client &&
      focused_client->GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD) {
    bool should_enable = !keyboard.IsCapsLockEnabled();
    keyboard.SetCapsLockEnabled(should_enable);
    return;
  }

  session_ = std::make_unique<Session>(
      GetPrefs(), focused_client, &keyboard,
      show_editor_callback_.is_null() ? PickerModel::EditorStatus::kDisabled
                                      : PickerModel::EditorStatus::kEnabled,
      show_lobster_callback_.is_null() ? PickerModel::LobsterStatus::kDisabled
                                       : PickerModel::LobsterStatus::kEnabled,
      base::BindRepeating(
          [](base::WeakPtr<PickerController> weak_controller,
             std::string_view emoji) -> std::string {
            if (weak_controller == nullptr) {
              return "";
            }
            return weak_controller->search_controller_.GetEmojiName(emoji);
          },
          weak_ptr_factory_.GetWeakPtr()));

  const gfx::Rect anchor_bounds = GetPickerAnchorBounds(
      GetCaretBounds(), GetCursorPoint(), GetFocusedWindowBounds());
  if (trigger_source == WidgetTriggerSource::kFeatureTour &&
      session_->model.GetMode() == PickerModeType::kUnfocused) {
    widget_ = PickerWidget::CreateCentered(this, anchor_bounds,
                                           trigger_event_timestamp);
  } else {
    widget_ =
        PickerWidget::Create(this, anchor_bounds, trigger_event_timestamp);
  }
  widget_->Show();

  view_observation_.Observe(widget_->GetContentsView());
}

void PickerController::CloseWidget() {
  if (!widget_) {
    return;
  }

  session_->session_metrics.SetOutcome(
      PickerSessionMetrics::SessionOutcome::kAbandoned);
  widget_->Close();
}

void PickerController::ShowWidgetPostFeatureTour() {
  ShowWidget(base::TimeTicks::Now(), WidgetTriggerSource::kFeatureTour);
}

std::optional<PickerWebPasteTarget> PickerController::GetWebPasteTarget() {
  return client_ ? client_->GetWebPasteTarget() : std::nullopt;
}

void PickerController::InsertResultOnNextFocus(
    const PickerSearchResult& result) {
  if (!widget_) {
    return;
  }

  // Update emoji history in prefs the result is an emoji/symbol/emoticon.
  CHECK(session_);
  if (auto* data = std::get_if<PickerEmojiResult>(&result);
      data != nullptr && session_->model.should_do_learning()) {
    session_->emoji_history_model.UpdateRecentEmoji(
        EmojiResultTypeToCategory(data->type), base::UTF16ToUTF8(data->text));
  }

  std::visit(
      base::Overloaded{
          [&](PickerRichMedia media) {
            ui::InputMethod* input_method = widget_->GetInputMethod();
            if (input_method == nullptr) {
              return;
            }

            // This cancels the previous request if there was one.
            insert_media_request_ = std::make_unique<PickerInsertMediaRequest>(
                input_method, media, kInsertMediaTimeout,
                base::BindOnce(
                    [](base::WeakPtr<PickerController> weak_controller) {
                      return weak_controller
                                 ? weak_controller->GetWebPasteTarget()
                                 : std::nullopt;
                    },
                    weak_ptr_factory_.GetWeakPtr()),
                base::BindOnce(&PickerController::OnInsertCompleted,
                               weak_ptr_factory_.GetWeakPtr(), media));
          },
          [&](PickerClipboardResult data) {
            // This cancels the previous request if there was one.
            paste_request_ = std::make_unique<PickerPasteRequest>(
                ClipboardHistoryController::Get(),
                aura::client::GetFocusClient(widget_->GetNativeView()),
                data.item_id);
          },
          [](std::monostate) { NOTREACHED(); },
      },
      GetInsertionContentForResult(result));

  session_->session_metrics.SetOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
}

void PickerController::OnInsertCompleted(
    const PickerRichMedia& media,
    PickerInsertMediaRequest::Result result) {
  // Fallback to copying to the clipboard on failure.
  if (result != PickerInsertMediaRequest::Result::kSuccess) {
    CopyMediaToClipboard(media);
  }
}

PickerCapsLockPosition PickerController::GetCapsLockPosition() {
  PrefService* prefs = GetPrefs();
  if (prefs == nullptr) {
    return PickerCapsLockPosition::kTop;
  }

  int caps_lock_displayed_count =
      prefs->GetInteger(prefs::kPickerCapsLockDislayedCountPrefName);
  int caps_lock_selected_count =
      prefs->GetInteger(prefs::kPickerCapsLockSelectedCountPrefName);
  float caps_lock_selected_ratio =
      static_cast<float>(caps_lock_selected_count) / caps_lock_displayed_count;

  if (caps_lock_displayed_count < kCapsLockMinimumTopDisplayCount ||
      caps_lock_selected_ratio >= kCapsLockRatioThresholdForTop) {
    return PickerCapsLockPosition::kTop;
  }
  if (caps_lock_selected_ratio >= kCapsLockRatioThresholdForBottom) {
    return PickerCapsLockPosition::kMiddle;
  }
  return PickerCapsLockPosition::kBottom;
}

}  // namespace ash
