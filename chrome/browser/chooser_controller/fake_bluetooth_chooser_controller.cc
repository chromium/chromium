// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/fake_bluetooth_chooser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

FakeBluetoothChooserController::FakeBluetoothChooserController(
    std::vector<FakeDevice> devices)
    : ChooserController(nullptr, 0, 0), devices_(std::move(devices)) {
  set_title_for_testing(
      l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT_ORIGIN,
                                 base::ASCIIToUTF16("example.com")));
}

FakeBluetoothChooserController::~FakeBluetoothChooserController() {}

bool FakeBluetoothChooserController::ShouldShowIconBeforeText() const {
  return true;
}

bool FakeBluetoothChooserController::ShouldShowReScanButton() const {
  return true;
}

base::string16 FakeBluetoothChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 FakeBluetoothChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_PAIR_BUTTON_TEXT);
}

bool FakeBluetoothChooserController::TableViewAlwaysDisabled() const {
  return table_view_always_disabled_;
}

size_t FakeBluetoothChooserController::NumOptions() const {
  return devices_.size();
}

int FakeBluetoothChooserController::GetSignalStrengthLevel(size_t index) const {
  return devices_.at(index).signal_strength;
}

base::string16 FakeBluetoothChooserController::GetOption(size_t index) const {
  return base::ASCIIToUTF16(devices_.at(index).name);
}

bool FakeBluetoothChooserController::IsConnected(size_t index) const {
  return devices_.at(index).connected;
}

bool FakeBluetoothChooserController::IsPaired(size_t index) const {
  return devices_.at(index).paired;
}

base::string16 FakeBluetoothChooserController::GetStatus() const {
  switch (status_) {
    case BluetoothStatus::UNAVAILABLE:
      return base::string16();
    case BluetoothStatus::IDLE:
      return l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN);
    case BluetoothStatus::SCANNING:
      return l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING);
  }
  NOTREACHED();
  return base::string16();
}

void FakeBluetoothChooserController::SetBluetoothStatus(
    BluetoothStatus status) {
  status_ = status;
  const bool available = status != BluetoothStatus::UNAVAILABLE;
  view()->OnAdapterEnabledChanged(available);
  if (available)
    view()->OnRefreshStateChanged(status_ == BluetoothStatus::SCANNING);
}

void FakeBluetoothChooserController::AddDevice(FakeDevice device) {
  devices_.push_back(device);
  view()->OnOptionAdded(devices_.size() - 1);
}

void FakeBluetoothChooserController::RemoveDevice(size_t index) {
  DCHECK_GT(devices_.size(), index);
  devices_.erase(devices_.begin() + index);
  view()->OnOptionRemoved(index);
}

void FakeBluetoothChooserController::UpdateDevice(size_t index,
                                                  FakeDevice new_device) {
  DCHECK_GT(devices_.size(), index);
  devices_[index] = new_device;
  view()->OnOptionUpdated(index);
}
