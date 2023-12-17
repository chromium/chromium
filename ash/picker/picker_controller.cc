// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string_view>

#include "ash/constants/ash_switches.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

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

class PickerViewDelegateImpl : public PickerView::Delegate {
 public:
  explicit PickerViewDelegateImpl(PickerClient* client)
      : client_(client),
        should_paint_(MatchPickerFeatureKeyHash() ==
                      PickerFeatureKeyType::kDev) {}

  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    return client_->CreateWebView(params);
  }

  void StartSearch(const std::u16string& query,
                   SearchResultsCallback callback) override {
    // TODO(b/310088338): Do a real search.
    callback.Run(PickerSearchResults({{
        PickerSearchResults::Section(u"Matching expressions", {{u"👍", u"😊"}}),
    }}));
  }

  bool ShouldPaint() override { return should_paint_; }

 private:
  raw_ptr<PickerClient> client_ = nullptr;
  bool should_paint_;
};

}  // namespace

PickerController::PickerController() = default;

PickerController::~PickerController() {
  // `widget_` depends on `client_`, which is only valid for the lifetime of
  // this class. Destroy the widget synchronously to avoid a dangling pointer.
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
  // The widget depends on `client_`, so destroy it synchronously to avoid a
  // dangling pointer.
  if (widget_) {
    widget_->CloseNow();
  }

  client_ = client;
}

void PickerController::ToggleWidget(
    const base::TimeTicks trigger_event_timestamp) {
  CHECK(client_);

  if (widget_) {
    widget_->Close();
  } else {
    widget_ = PickerView::CreateWidget(
        std::make_unique<PickerViewDelegateImpl>(client_),
        trigger_event_timestamp);
    widget_->Show();
  }
}

}  // namespace ash
