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

#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl.h"
#include "ash/picker/picker_copy_media.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/picker_paste_request.h"
#include "ash/picker/picker_rich_media.h"
#include "ash/picker/search/picker_search_controller.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_positioning.h"
#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_widget.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/sha1.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/input_method.h"
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

// Gets the current caret bounds in universal screen coordinates in DIP. Returns
// an empty rect if there is no active caret or the caret bounds can't be
// determined (e.g. no focused input field).
gfx::Rect GetCaretBounds() {
  const ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method || !input_method->GetTextInputClient()) {
    return gfx::Rect();
  }

  return input_method->GetTextInputClient()->GetCaretBounds();
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

// The user can ask to insert rich media, a clipboard item, or insert nothing.
using InsertionContent = std::
    variant<PickerRichMedia, PickerSearchResult::ClipboardData, std::monostate>;

InsertionContent GetInsertionContentForResult(
    const PickerSearchResult& result) {
  using ReturnType = InsertionContent;
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) -> ReturnType {
            return PickerTextMedia(data.text);
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
          }},
      result.data());
}

}  // namespace

PickerController::PickerController() {
  // `base::Unretained` is safe here because this class owns `asset_fetcher_`.
  asset_fetcher_ = std::make_unique<PickerAssetFetcherImpl>(base::BindRepeating(
      &PickerController::GetSharedURLLoaderFactory, base::Unretained(this)));
  if (auto* manager = ash::input_method::InputMethodManager::Get()) {
    keyboard_observation_.Observe(manager->GetImeKeyboard());
  }
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
  if (client_ == nullptr) {
    search_controller_ = nullptr;
  } else {
    search_controller_ = std::make_unique<PickerSearchController>(
        client_, PickerModel().GetAvailableCategories(), kBurnInPeriod);
  }
}

void PickerController::ToggleWidget(
    const base::TimeTicks trigger_event_timestamp) {
  CHECK(client_);

  if (widget_) {
    session_metrics_->RecordOutcome(
        PickerSessionMetrics::SessionOutcome::kAbandoned);
    widget_->Close();
  } else {
    widget_ = PickerWidget::Create(
        this,
        GetPickerAnchorBounds(GetCaretBounds(), GetCursorPoint(),
                              GetFocusedWindowBounds()),
        trigger_event_timestamp);
    widget_->Show();

    feature_usage_metrics_.StartUsage();
    session_metrics_ = std::make_unique<PickerSessionMetrics>();
    widget_observation_.Observe(widget_.get());
  }
}

std::unique_ptr<AshWebView> PickerController::CreateWebView(
    const AshWebView::InitParams& params) {
  return client_->CreateWebView(params);
}

void PickerController::GetResultsForCategory(PickerCategory category,
                                             SearchResultsCallback callback) {
  // TODO: b/325977099 - Get actual results for each category.
  std::vector<ash::PickerSearchResult> recent_results;
  switch (category) {
    case PickerCategory::kEmojis:
    case PickerCategory::kSymbols:
    case PickerCategory::kEmoticons:
    case PickerCategory::kGifs:
      break;
    case PickerCategory::kOpenTabs:
    case PickerCategory::kBrowsingHistory:
    case PickerCategory::kBookmarks:
      recent_results.push_back(PickerSearchResult::BrowsingHistory(
          GURL("http://crbug.com"), u"Crbug",
          GetIconForPickerCategory(category)));
      recent_results.push_back(PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          GetIconForPickerCategory(category)));
      break;
    case PickerCategory::kDriveFiles:
    case PickerCategory::kLocalFiles:
      break;
  }
  callback.Run({
      PickerSearchResultsSection(PickerSectionType::kRecentlyUsed,
                                 recent_results),
  });
}

void PickerController::StartSearch(const std::u16string& query,
                                   std::optional<PickerCategory> category,
                                   SearchResultsCallback callback) {
  CHECK(search_controller_);
  search_controller_->StartSearch(query, std::move(category),
                                  std::move(callback));
}

void PickerController::InsertResultOnNextFocus(
    const PickerSearchResult& result) {
  if (!widget_) {
    return;
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

  session_metrics_->RecordOutcome(
      PickerSessionMetrics::SessionOutcome::kInsertedOrCopied);
}

void PickerController::ShowEmojiPicker(ui::EmojiPickerCategory category) {
  ui::ShowEmojiPanelInSpecificMode(category);
}

PickerAssetFetcher* PickerController::GetAssetFetcher() {
  return asset_fetcher_.get();
}

void PickerController::OnCapsLockChanged(bool enabled) {
  // TODO: b/319301963 - Remove this behaviour once the experiment is over.
  ToggleWidget();
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

}  // namespace ash
