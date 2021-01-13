// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/fake_usb_chooser_controller.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

FakeUsbChooserController::FakeUsbChooserController(int device_count)
    : ChooserController(nullptr, 0, 0), device_count_(device_count) {
  set_title_for_testing(l10n_util::GetStringFUTF16(
      IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN, base::ASCIIToUTF16("example.com")));
}

base::string16 FakeUsbChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 FakeUsbChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<base::string16, base::string16>
FakeUsbChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t FakeUsbChooserController::NumOptions() const {
  return device_count_;
}

base::string16 FakeUsbChooserController::GetOption(size_t index) const {
  return base::ASCIIToUTF16(base::StringPrintf("Device #%zu", index));
}
