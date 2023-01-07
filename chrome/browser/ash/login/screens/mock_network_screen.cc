// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_network_screen.h"

namespace ash {

using ::testing::AtLeast;
using ::testing::_;

MockNetworkScreen::MockNetworkScreen(base::WeakPtr<NetworkScreenView> view,
                                     const ScreenExitCallback& exit_callback)
    : NetworkScreen(view, exit_callback) {}

MockNetworkScreen::~MockNetworkScreen() = default;

void MockNetworkScreen::ExitScreen(NetworkScreen::Result result) {
  exit_callback()->Run(result);
}

MockNetworkScreenView::MockNetworkScreenView() = default;

MockNetworkScreenView::~MockNetworkScreenView() = default;

}  // namespace ash
