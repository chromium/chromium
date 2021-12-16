// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace chromeos {

namespace {

std::string OnGetHardwareInfo(base::SysInfo::HardwareInfo hardware_info) {
  std::string manufacturer;
  base::TrimWhitespaceASCII(hardware_info.manufacturer,
                            base::TrimPositions::TRIM_ALL, &manufacturer);

  return manufacturer;
}

}  // namespace

// static
HardwareInfoDelegate::Factory* HardwareInfoDelegate::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<HardwareInfoDelegate> HardwareInfoDelegate::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }
  return base::WrapUnique<HardwareInfoDelegate>(new HardwareInfoDelegate());
}

// static
void HardwareInfoDelegate::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

HardwareInfoDelegate::Factory::~Factory() = default;

HardwareInfoDelegate::HardwareInfoDelegate() = default;
HardwareInfoDelegate::~HardwareInfoDelegate() = default;

void HardwareInfoDelegate::GetManufacturer(ManufacturerCallback callback) {
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&OnGetHardwareInfo).Then(std::move(callback)));
}

}  // namespace chromeos
