// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wifi_credentials.h"

namespace ash::quick_start {

WifiCredentials::WifiCredentials() = default;

WifiCredentials::~WifiCredentials() = default;

WifiCredentials::WifiCredentials(const WifiCredentials& other) = default;

WifiCredentials& WifiCredentials::operator=(const WifiCredentials& other) =
    default;

}  // namespace ash::quick_start
