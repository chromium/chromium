// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHOOSER_CONTROLLER_FAKE_BLUETOOTH_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_CHOOSER_CONTROLLER_FAKE_BLUETOOTH_CHOOSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

// A subclass of ChooserController that pretends to be a Bluetooth device
// chooser for testing. The result should be visually similar to the real
// version of the dialog for interactive tests.
class FakeBluetoothChooserController : public ChooserController {
 public:
  enum class BluetoothStatus {
    UNAVAILABLE,
    IDLE,
    SCANNING,
  };

  enum ConnectionStatus {
    NOT_CONNECTED = false,
    CONNECTED = true,
  };

  enum PairStatus {
    NOT_PAIRED = false,
    PAIRED = true,
  };

  static constexpr int kSignalStrengthUnknown = -1;
  static constexpr int kSignalStrengthLevel0 = 0;
  static constexpr int kSignalStrengthLevel1 = 1;
  static constexpr int kSignalStrengthLevel2 = 2;
  static constexpr int kSignalStrengthLevel3 = 3;
  static constexpr int kSignalStrengthLevel4 = 4;

  struct FakeDevice {
    std::string name;
    bool connected;
    bool paired;
    int signal_strength;
  };

  explicit FakeBluetoothChooserController(std::vector<FakeDevice> devices = {});
  ~FakeBluetoothChooserController() override;

  // ChooserController:
  bool ShouldShowIconBeforeText() const override;
  bool ShouldShowReScanButton() const override;
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  bool TableViewAlwaysDisabled() const override;
  size_t NumOptions() const override;
  int GetSignalStrengthLevel(size_t index) const override;
  base::string16 GetOption(size_t index) const override;
  bool IsConnected(size_t index) const override;
  bool IsPaired(size_t index) const override;
  base::string16 GetStatus() const override;
  MOCK_METHOD0(RefreshOptions, void());
  MOCK_METHOD1(Select, void(const std::vector<size_t>& indices));
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(OpenHelpCenterUrl, void());
  MOCK_CONST_METHOD0(OpenAdapterOffHelpUrl, void());

  void SetBluetoothStatus(BluetoothStatus status);
  void AddDevice(FakeDevice device);
  void RemoveDevice(size_t index);
  void UpdateDevice(size_t index, FakeDevice new_device);
  void set_table_view_always_disabled(bool table_view_always_disabled) {
    table_view_always_disabled_ = table_view_always_disabled;
  }

 private:
  BluetoothStatus status_ = BluetoothStatus::UNAVAILABLE;
  std::vector<FakeDevice> devices_;
  bool table_view_always_disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothChooserController);
};

#endif  // CHROME_BROWSER_CHOOSER_CONTROLLER_FAKE_BLUETOOTH_CHOOSER_CONTROLLER_H_
