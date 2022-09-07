// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_web_view.h"

namespace ash {

// AshWebView ------------------------------------------------------------

AshWebView::AshWebView() = default;
AshWebView::~AshWebView() = default;

// AshWebView::InitParams ------------------------------------------------

AshWebView::InitParams::InitParams() = default;
AshWebView::InitParams::InitParams(const InitParams& copy) = default;
AshWebView::InitParams::~InitParams() = default;

}  // namespace ash
