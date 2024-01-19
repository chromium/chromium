// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_asset_fetcher_impl.h"
#include "ash/picker/picker_insert_media_request.h"
#include "ash/picker/views/picker_view.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "ui/base/ime/ash/input_method_manager.h"

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

}  // namespace

PickerController::PickerController()
    : should_paint_(MatchPickerFeatureKeyHash() == PickerFeatureKeyType::kDev) {
  asset_fetcher_ = std::make_unique<PickerAssetFetcherImpl>(base::BindRepeating(
      &PickerController::DownloadGifToString, weak_ptr_factory_.GetWeakPtr()));
  if (auto* manager = ash::input_method::InputMethodManager::Get()) {
    observation_.Observe(manager->GetImeKeyboard());
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
    widget_ = PickerView::CreateWidget(this, trigger_event_timestamp);
    widget_->Show();
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
            PickerSearchResult::Gif(GURL(
                "https://media.tenor.com/BzfS_9uPq_AAAAAd/cat-bonfire.gif"))}}),
      PickerSearchResults::Section(u"Matching links",
                                   {{PickerSearchResult::Text(u"www.foo.com"),
                                     PickerSearchResult::Text(u"crbug.com")}}),
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
  // TODO: b/316936577 - Support inserting images.
  if (const auto* data =
          std::get_if<PickerSearchResult::TextData>(&result.data())) {
    insert_media_request_ = std::make_unique<PickerInsertMediaRequest>(
        input_method, data->text, kInsertMediaTimeout);
  }
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
