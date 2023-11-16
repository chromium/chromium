// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_controller.h"

#include <string_view>

#include "ash/constants/ash_switches.h"
#include "ash/picker/views/picker_view.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace ash {
namespace {

// The hash value for the feature key of the Picker feature.
constexpr std::string_view kPickerFeatureKeyHash =
    "\xE1\xC0\x09\x7F\xBE\x03\xBF\x48\xA7\xA0\x30\x53\x07\x4F\xFB\xC5\x6D\xD4"
    "\x22\x5F";
}  // namespace

bool PickerController::IsFeatureKeyMatched() {
  // Command line looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --picker-feature-key="INSERT KEY HERE" --enable-features=PickerFeature
  const std::string& provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPickerFeatureKey));

  bool picker_key_matched = (provided_key_hash == kPickerFeatureKeyHash);
  if (!picker_key_matched) {
    LOG(ERROR) << "Provided feature key does not match with the expected one.";
  }

  return picker_key_matched;
}

void PickerController::ToggleWidget() {
  if (widget_) {
    widget_->Close();
  } else {
    widget_ = PickerView::CreateWidget();
    widget_->Show();
  }
}

}  // namespace ash
