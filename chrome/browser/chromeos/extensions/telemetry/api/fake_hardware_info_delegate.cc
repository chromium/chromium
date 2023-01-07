// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/fake_hardware_info_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"

namespace chromeos {

FakeHardwareInfoDelegate::Factory::Factory(std::string manufacturer)
    : manufacturer_(std::move(manufacturer)) {}

FakeHardwareInfoDelegate::Factory::~Factory() = default;

std::unique_ptr<HardwareInfoDelegate>
FakeHardwareInfoDelegate::Factory::CreateInstance() {
  return base::WrapUnique<HardwareInfoDelegate>(
      new FakeHardwareInfoDelegate(manufacturer_));
}

FakeHardwareInfoDelegate::FakeHardwareInfoDelegate(std::string manufacturer)
    : manufacturer_(std::move(manufacturer)) {}

FakeHardwareInfoDelegate::~FakeHardwareInfoDelegate() = default;

void FakeHardwareInfoDelegate::GetManufacturer(ManufacturerCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), manufacturer_));
}

}  // namespace chromeos
