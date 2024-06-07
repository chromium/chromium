// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl.h"
#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/picker_copy_media.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/picker_paste_request.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "ash/picker/search/picker_search_controller.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_positioning.h"
#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_widget.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/scoped_refptr.h"
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
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

bool g_should_check_key = true;

// The hash value for the feature key of the Picker feature, used for
// development.
constexpr std::string_view kPickerFeatureDevKeyHash(
    "\xE1\xC0\x09\x7F\xBE\x03\xBF\x48\xA7\xA0\x30\x53\x07\x4F\xFB\xC5\x6D\xD4"
    "\x22\x5F",
    base::kSHA1Length);

// The hash value for the feature key of the Picker feature, used in some tests.
constexpr std::string_view kPickerFeatureTestKeyHash(
    "\xE7\x2C\x99\xD7\x99\x89\xDB\xA5\x9D\x06\x4A\xED\xDF\xE5\x30\xA7\x8C\x76"
    "\x00\x89",
    base::kSHA1Length);

// Time from when a start starts to when the first set of results are published.
constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

enum class PickerFeatureKeyType { kNone, kDev, kTest };

constexpr int kMaxRecentFiles = 10;
constexpr int kMaxRecentEmoji = 20;

constexpr std::string_view kEmojiHistoryValueFieldName = "text";

PickerFeatureKeyType MatchPickerFeatureKeyHash() {
  // Command line looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --picker-feature-key="INSERT KEY HERE" --enable-features=PickerFeature
  const std::string provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPickerFeatureKey));
  if (provided_key_hash == kPickerFeatureDevKeyHash) {
    return PickerFeatureKeyType::kDev;
  }
  if (provided_key_hash == kPickerFeatureTestKeyHash) {
    return PickerFeatureKeyType::kTest;
  }
  return PickerFeatureKeyType::kNone;
}

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
using InsertionContent = std::
    variant<PickerRichMedia, PickerSearchResult::ClipboardData, std::monostate>;

InsertionContent GetInsertionContentForResult(
    const PickerSearchResult& result) {
  using ReturnType = InsertionContent;
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) -> ReturnType {
            return PickerTextMedia(data.primary_text);
          },
          [](const PickerSearchResult::EmojiData& data) -> ReturnType {
            return PickerTextMedia(data.emoji);
          },
          [](const PickerSearchResult::SymbolData& data) -> ReturnType {
            return PickerTextMedia(data.symbol);
          },
          [](const PickerSearchResult::EmoticonData& data) -> ReturnType {
            return PickerTextMedia(data.emoticon);
          },
          [](const PickerSearchResult::ClipboardData& data) -> ReturnType {
            return data;
          },
          [](const PickerSearchResult::GifData& data) -> ReturnType {
            return PickerImageMedia(data.full_url, data.full_dimensions,
                                    data.content_description);
          },
          [](const PickerSearchResult::BrowsingHistoryData& data)
              -> ReturnType { return PickerLinkMedia(data.url); },
          [](const PickerSearchResult::LocalFileData& data) -> ReturnType {
            return PickerLocalFileMedia(data.file_path);
          },
          [](const PickerSearchResult::DriveFileData& data) -> ReturnType {
            return PickerLinkMedia(data.url);
          },
          [](const PickerSearchResult::CategoryData& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerSearchResult::SearchRequestData& data) -> ReturnType {
            return std::monostate();
          },
          [](const PickerSearchResult::EditorData& data) -> ReturnType {
            return std::monostate();
          },
      },
      result.data());
}

std::vector<PickerSearchResultsSection> CreateSingleSectionForCategoryResults(
    std::vector<PickerSearchResult> results) {
  return {PickerSearchResultsSection(PickerSectionType::kNone,
                                     std::move(results),
                                     /*has_more_results*/ false)};
}

bool u16_isalpha(char16_t ch) {
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}

bool u16_is_sentence_end(char16_t ch) {
  return ch == u'.' || ch == u'!' || ch == '?';
}

std::u16string ToTitleCase(std::u16string_view text) {
  std::u16string result(text);
  std::u16string uppercase_text = base::i18n::ToUpper(text);
  for (size_t i = 0; i < result.length(); i++) {
    if (u16_isalpha(result[i]) && (i == 0 || result[i - 1] == u' ')) {
      result[i] = uppercase_text[i];
    }
  }
  return result;
}

std::u16string ToSentenceCase(std::u16string_view text) {
  std::u16string result(text);
  std::u16string uppercase_text = base::i18n::ToUpper(text);
  bool sentence_start = true;
  for (size_t i = 0; i < result.length(); i++) {
    if (u16_isalpha(result[i]) && sentence_start) {
      result[i] = uppercase_text[i];
    }
    if (u16_is_sentence_end(result[i])) {
      sentence_start = true;
    } else if (result[i] != u' ') {
      sentence_start = false;
    }
  }
  return result;
}

std::u16string TransformText(std::u16string_view text,
                             PickerCategory category) {
  switch (category) {
    case PickerCategory::kUpperCase:
      return base::i18n::ToUpper(text);
    case PickerCategory::kLowerCase:
      return base::i18n::ToLower(text);
    case PickerCategory::kSentenceCase:
      return ToSentenceCase(text);
    case PickerCategory::kTitleCase:
      return ToTitleCase(text);
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
    case PickerCategory::kLinks:
    case PickerCategory::kExpressions:
    case PickerCategory::kDriveFiles:
    case PickerCategory::kLocalFiles:
    case PickerCategory::kDatesTimes:
    case PickerCategory::kUnitsMaths:
    case PickerCategory::kClipboard:
    case PickerCategory::kCapsOn:
    case PickerCategory::kCapsOff:
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

void OpenLink(const GURL& url) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewWindow);
}

void OpenFile(const base::FilePath& path) {
  ash::NewWindowDelegate::GetPrimary()->OpenFile(path);
}

std::string ConvertToString(ui::EmojiPickerCategory category) {
  switch (category) {
    case ui::EmojiPickerCategory::kEmojis:
      return "emoji";
    case ui::EmojiPickerCategory::kSymbols:
      return "symbol";
    case ui::EmojiPickerCategory::kEmoticons:
      return "emoticon";
    case ui::EmojiPickerCategory::kGifs:
      return "gif";
  }
}

}  // namespace

PickerController::PickerController()
    : asset_fetcher_(std::make_unique<PickerAssetFetcherImpl>(this)) {
  clipboard_provider_ = std::make_unique<PickerClipboardProvider>();
}

PickerController::~PickerController() {
  // `widget_` depends on `this`. Destroy the widget synchronously to avoid a
  // dangling pointer.
  if (widget_) {
    widget_->CloseNow();
  }
}

bool PickerController::IsFeatureKeyMatched() {
  if (!g_should_check_key) {
    return true;
  }

  if (base::FeatureList::IsEnabled(ash::features::kPickerDogfood)) {
    // This flag allows PickerController to be created, but ToggleWidget will
    // still check if the feature is allowed by the client.
    return true;
  }

  if (MatchPickerFeatureKeyHash() == PickerFeatureKeyType::kNone) {
    LOG(ERROR) << "Provided feature key does not match with the expected one.";
    return false;
  }

  return true;
}

void PickerController::DisableFeatureKeyCheckForTesting() {
  CHECK_IS_TEST();
  g_should_check_key = false;
}

void PickerController::SetClient(PickerClient* client) {
  client_ = client;
  // The destructor of `PickerSearchRequest` inside `PickerSearchController` may
  // result in "stop search" calls to the PREVIOUS `PickerClient`.
  if (client_ == nullptr) {
    search_controller_ = nullptr;
  } else {
    search_controller_ =
        std::make_unique<PickerSearchController>(client_, kBurnInPeriod);
  }
}

void PickerController::ToggleWidget(
    const base::TimeTicks trigger_event_timestamp) {
  CHECK(client_);
  if (base::FeatureList::IsEnabled(ash::features::kPickerDogfood) &&
      !client_->IsFeatureAllowedForDogfood()) {
    LOG(ERROR) << "Picker feature is blocked";
    return;
  }

  if (widget_) {
    CloseWidget();
  } else {
    ShowWidget(trigger_event_timestamp);
  }
}

std::vector<PickerCategory> PickerController::GetAvailableCategories() {
  return model_ == nullptr ? std::vector<PickerCategory>{}
                           : model_->GetAvailableCategories();
}

std::vector<PickerCategory> PickerController::GetRecentResultsCategories() {
  return model_ == nullptr ? std::vector<PickerCategory>{}
                           : model_->GetRecentResultsCategories();
}

void PickerController::GetResultsForCategory(PickerCategory category,
                                             SearchResultsCallback callback) {
  // TODO: b/325977099 - Get actual results for each category.
  std::vector<ash::PickerSearchResult> recent_results;
  switch (category) {
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
    case PickerCategory::kUpperCase:
    case PickerCategory::kLowerCase:
    case PickerCategory::kSentenceCase:
    case PickerCategory::kTitleCase:
    case PickerCategory::kCapsOn:
    case PickerCategory::kCapsOff:
      NOTREACHED_NORETURN();
    case PickerCategory::kLinks:
      client_->GetSuggestedLinkResults(
          base::BindRepeating(CreateSingleSectionForCategoryResults)
              .Then(std::move(callback)));
      return;
    case PickerCategory::kExpressions:
      NOTREACHED_NORETURN();
    case PickerCategory::kDriveFiles:
      client_->GetRecentDriveFileResults(
          kMaxRecentFiles,
          base::BindRepeating(CreateSingleSectionForCategoryResults)
              .Then(std::move(callback)));
      return;
    case PickerCategory::kLocalFiles:
      client_->GetRecentLocalFileResults(
          kMaxRecentFiles,
          base::BindRepeating(CreateSingleSectionForCategoryResults)
              .Then(std::move(callback)));
      return;
    case PickerCategory::kDatesTimes:
      std::move(callback).Run(
          CreateSingleSectionForCategoryResults(PickerSuggestedDateResults()));
      break;
    case PickerCategory::kUnitsMaths:
      std::move(callback).Run(
          CreateSingleSectionForCategoryResults(PickerMathExamples()));
      break;
    case PickerCategory::kClipboard:
      clipboard_provider_->FetchResults(
          base::BindRepeating(CreateSingleSectionForCategoryResults)
              .Then(std::move(callback)));
      return;
  }
}

void PickerController::TransformSelectedText(PickerCategory category) {
  if (!model_) {
    return;
  }
  std::u16string_view selected_text = model_->selected_text();
  InsertResultOnNextFocus(PickerSearchResult::Text(
      TransformText(selected_text, category),
      PickerSearchResult::TextData::Source::kCaseTransform));
}

void PickerController::StartSearch(const std::u16string& query,
                                   std::optional<PickerCategory> category,
                                   SearchResultsCallback callback) {
  CHECK(search_controller_);
  search_controller_->StartSearch(query, std::move(category),
                                  GetAvailableCategories(),
                                  std::move(callback));
}

void PickerController::InsertResultOnNextFocus(
    const PickerSearchResult& result) {
  if (!widget_) {
    return;
  }

  // Update emoji history in prefs the result is an emoji/symbol/emoticon.
  std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::EmojiData& data) {
            UpdateRecentEmoji(ui::EmojiPickerCategory::kEmojis, data.emoji);
          },
          [&](const PickerSearchResult::SymbolData& data) {
            UpdateRecentEmoji(ui::EmojiPickerCategory::kSymbols, data.symbol);
          },
          [&](const PickerSearchResult::EmoticonData& data) {
            UpdateRecentEmoji(ui::EmojiPickerCategory::kEmoticons,
                              data.emoticon);
          },
          [](const auto& data) {}},
      result.data());

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
                    [](const PickerRichMedia& media,
                       PickerInsertMediaRequest::Result result) {
                      // Fallback to copying to the clipboard on failure.
                      if (result !=
                          PickerInsertMediaRequest::Result::kSuccess) {
                        CopyMediaToClipboard(media);
                      }
                    },
                    media));
          },
          [&](PickerSearchResult::ClipboardData data) {
            // This cancels the previous request if there was one.
            paste_request_ = std::make_unique<PickerPasteRequest>(
                ClipboardHistoryController::Get(),
                aura::client::GetFocusClient(widget_->GetNativeView()),
                data.item_id);
          },
          [](std::monostate) { NOTREACHED_NORETURN(); },
      },
      GetInsertionContentForResult(result));

  session_metrics_->SetOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
}

void PickerController::OpenResult(const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::EmojiData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::SymbolData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::EmoticonData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::ClipboardData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::GifData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::BrowsingHistoryData& data) {
            OpenLink(data.url);
          },
          [](const PickerSearchResult::LocalFileData& data) {
            OpenFile(data.file_path);
          },
          [](const PickerSearchResult::DriveFileData& data) {
            OpenLink(data.url);
          },
          [](const PickerSearchResult::CategoryData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::SearchRequestData& data) {
            NOTREACHED_NORETURN();
          },
          [](const PickerSearchResult::EditorData& data) {
            NOTREACHED_NORETURN();
          },
      },
      result.data());
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

void PickerController::SetCapsLockEnabled(bool enabled) {
  GetImeKeyboard().SetCapsLockEnabled(enabled);
}

void PickerController::GetSuggestedEditorResults(
    SuggestedEditorResultsCallback callback) {
  client_->GetSuggestedEditorResults(std::move(callback));
}

PickerAssetFetcher* PickerController::GetAssetFetcher() {
  return asset_fetcher_.get();
}

PickerSessionMetrics& PickerController::GetSessionMetrics() {
  return *session_metrics_;
}

PickerActionType PickerController::GetActionForResult(
    const PickerSearchResult& result) {
  const PickerModeType mode = model_->GetMode();
  return std::visit(
      base::Overloaded{
          [mode](const PickerSearchResult::TextData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::EmojiData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::SymbolData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::EmoticonData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::ClipboardData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::GifData& data) {
            CHECK(mode == PickerModeType::kNoSelection ||
                  mode == PickerModeType::kHasSelection);
            return PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::BrowsingHistoryData& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::LocalFileData& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [mode](const PickerSearchResult::DriveFileData& data) {
            return mode == PickerModeType::kUnfocused
                       ? PickerActionType::kOpen
                       : PickerActionType::kInsert;
          },
          [](const PickerSearchResult::CategoryData& data) {
            return PickerActionType::kDo;
          },
          [](const PickerSearchResult::SearchRequestData& data) {
            return PickerActionType::kDo;
          },
          [](const PickerSearchResult::EditorData& data) {
            return PickerActionType::kCreate;
          },
      },
      result.data());
}

std::vector<std::string> PickerController::GetRecentEmoji(
    ui::EmojiPickerCategory category) {
  if (client_ == nullptr || client_->GetPrefs() == nullptr) {
    return {};
  }

  const base::Value::List* history = client_->GetPrefs()
                                         ->GetDict(prefs::kEmojiPickerHistory)
                                         .FindList(ConvertToString(category));
  if (history == nullptr) {
    return {};
  }
  std::vector<std::string> results;
  for (const auto& it : *history) {
    const base::Value::Dict* value_dict = it.GetIfDict();
    if (value_dict == nullptr) {
      continue;
    }
    const std::string* text =
        value_dict->FindString(kEmojiHistoryValueFieldName);
    if (text != nullptr) {
      results.push_back(*text);
    }
  }
  return results;
}

std::vector<std::string> PickerController::GetPlaceholderEmojis() {
  return {"😀", "😃", "😄"};
}

void PickerController::OnWidgetDestroying(views::Widget* widget) {
  feature_usage_metrics_.StopUsage();
  session_metrics_.reset();
  widget_observation_.Reset();
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

void PickerController::ShowWidget(base::TimeTicks trigger_event_timestamp) {
  session_metrics_ = std::make_unique<PickerSessionMetrics>();
  show_editor_callback_ = client_->CacheEditorContext();

  model_ = std::make_unique<PickerModel>(
      GetFocusedTextInputClient(), &GetImeKeyboard(),
      show_editor_callback_.is_null() ? PickerModel::EditorStatus::kDisabled
                                      : PickerModel::EditorStatus::kEnabled);
  widget_ = PickerWidget::Create(
      this,
      GetPickerAnchorBounds(GetCaretBounds(), GetCursorPoint(),
                            GetFocusedWindowBounds()),
      trigger_event_timestamp);
  widget_->Show();

  feature_usage_metrics_.StartUsage();
  session_metrics_->OnStartSession(GetFocusedTextInputClient());
  widget_observation_.Observe(widget_.get());
}

void PickerController::CloseWidget() {
  session_metrics_->SetOutcome(
      PickerSessionMetrics::SessionOutcome::kAbandoned);
  widget_->Close();
  model_.reset();
}

void PickerController::UpdateRecentEmoji(ui::EmojiPickerCategory category,
                                         std::u16string_view text) {
  if (client_ == nullptr || client_->GetPrefs() == nullptr) {
    return;
  }
  std::string utf8_text = base::UTF16ToUTF8(text);

  std::vector<std::string> history = GetRecentEmoji(category);
  base::Value::List history_value;
  history_value.Append(
      base::Value::Dict().Set(kEmojiHistoryValueFieldName, text));
  for (const std::string& value : history) {
    if (value == utf8_text) {
      continue;
    }
    history_value.Append(
        base::Value::Dict().Set(kEmojiHistoryValueFieldName, value));
    if (history_value.size() == kMaxRecentEmoji) {
      break;
    }
  }

  ScopedDictPrefUpdate update(client_->GetPrefs(), prefs::kEmojiPickerHistory);
  update->Set(ConvertToString(category), std::move(history_value));
}

}  // namespace ash
