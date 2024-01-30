// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/hash/sha1.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/input_method.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

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

// Time from when the insert is issued and when we give up inserting.
constexpr base::TimeDelta kInsertMediaTimeout = base::Seconds(2);

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

// TODO: b/316936687 - Use the icons from real search results.
const gfx::VectorIcon& kPlaceholderIcon = kCheckIcon;

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

PickerInsertMediaRequest::MediaData ResultToInsertMediaData(
    const PickerSearchResult& result) {
  return std::visit(
      base::Overloaded{
          [](const PickerSearchResult::TextData& data) {
            return PickerInsertMediaRequest::MediaData::Text(data.text);
          },
          [](const PickerSearchResult::GifData& data) {
            return PickerInsertMediaRequest::MediaData::Image(data.url);
          },
          [](const PickerSearchResult::BrowsingHistoryData& data) {
            return PickerInsertMediaRequest::MediaData::Link(data.url);
          },
      },
      result.data());
}

}  // namespace

PickerController::PickerController()
    : should_paint_(MatchPickerFeatureKeyHash() == PickerFeatureKeyType::kDev) {
  asset_fetcher_ = std::make_unique<PickerAssetFetcherImpl>(base::BindRepeating(
      &PickerController::DownloadGifToString, weak_ptr_factory_.GetWeakPtr()));
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
  if (MatchPickerFeatureKeyHash() == PickerFeatureKeyType::kNone) {
    LOG(ERROR) << "Provided feature key does not match with the expected one.";
    return false;
  }

  return true;
}

void PickerController::SetClient(PickerClient* client) {
  client_ = client;
}

void PickerController::ToggleWidget(
    const base::TimeTicks trigger_event_timestamp) {
  CHECK(client_);

  if (widget_) {
    widget_->Close();
  } else {
    widget_ = PickerView::CreateWidget(GetCaretBounds(), this,
                                       trigger_event_timestamp);
    widget_->Show();

    feature_usage_metrics_.StartUsage();
    widget_observation_.Observe(widget_.get());
  }
}

std::unique_ptr<AshWebView> PickerController::CreateWebView(
    const AshWebView::InitParams& params) {
  return client_->CreateWebView(params);
}

void PickerController::GetResultsForCategory(PickerCategory category,
                                             SearchResultsCallback callback) {
  // TODO: b/316936620 - Get actual results for the category.
  callback.Run(PickerSearchResults({{
      PickerSearchResults::Section(u"Recently used",
                                   {{PickerSearchResult::Text(u"😊")}}),
  }}));
}

void PickerController::StartSearch(const std::u16string& query,
                                   std::optional<PickerCategory> category,
                                   SearchResultsCallback callback) {
  // TODO(b/310088338): Do a real search.
  callback.Run(PickerSearchResults({{
      PickerSearchResults::Section(
          u"Matching expressions",
          {{PickerSearchResult::Text(u"👍"), PickerSearchResult::Text(u"😊"),
            PickerSearchResult::Gif(
                GURL(
                    "https://media.tenor.com/BzfS_9uPq_AAAAAd/cat-bonfire.gif"),
                gfx::Size(140, 140))}}),
      PickerSearchResults::Section(
          u"Matching links",
          {{
              PickerSearchResult::BrowsingHistory(
                  GURL("http://www.foo.com"),
                  ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
              PickerSearchResult::BrowsingHistory(
                  GURL("http://crbug.com"),
                  ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
          }}),
      PickerSearchResults::Section(
          u"Matching files", {{PickerSearchResult::Text(u"my file"),
                               PickerSearchResult::Text(u"my other file")}}),
  }}));
}

void PickerController::InsertResultOnNextFocus(
    const PickerSearchResult& result) {
  if (!widget_) {
    return;
  }

  ui::InputMethod* input_method = widget_->GetInputMethod();
  if (input_method == nullptr) {
    return;
  }

  // This cancels the previous request if there was one.
  insert_media_request_ = std::make_unique<PickerInsertMediaRequest>(
      input_method, ResultToInsertMediaData(result), kInsertMediaTimeout);
}

bool PickerController::ShouldPaint() {
  return should_paint_;
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
  widget_observation_.Reset();
}

void PickerController::DownloadGifToString(
    const GURL& url,
    base::OnceCallback<void(const std::string&)> callback) {
  if (!client_) {
    // TODO: b/316936723 - Add better handling of errors.
    std::move(callback).Run(std::string());
    return;
  }
  client_->DownloadGifToString(url, std::move(callback));
}

}  // namespace ash
