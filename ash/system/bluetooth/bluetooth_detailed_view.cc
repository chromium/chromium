// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_detailed_view.h"

#include "ash/system/bluetooth/bluetooth_detailed_view_impl.h"

namespace ash {
namespace {
BluetoothDetailedView::Factory* g_test_factory = nullptr;
}  // namespace

BluetoothDetailedView::BluetoothDetailedView(Delegate* delegate)
    : delegate_(delegate) {}

std::unique_ptr<BluetoothDetailedView> BluetoothDetailedView::Factory::Create(
    DetailedViewDelegate* detailed_view_delegate,
    Delegate* delegate) {
  if (g_test_factory) {
    return g_test_factory->CreateForTesting(delegate);  // IN-TEST
  }
  return std::make_unique<BluetoothDetailedViewImpl>(detailed_view_delegate,
                                                     delegate);
}

void BluetoothDetailedView::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory = test_factory;
}

}  // namespace ash
