// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/mock/mock_snapper_provider.h"

#include "components/manta/snapper_provider.h"

MockSnapperProvider::MockSnapperProvider()
    : manta::SnapperProvider(nullptr, nullptr) {}

MockSnapperProvider::~MockSnapperProvider() = default;
