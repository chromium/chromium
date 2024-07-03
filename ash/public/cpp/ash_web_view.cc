// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_web_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

// AshWebView ------------------------------------------------------------

AshWebView::AshWebView() = default;
AshWebView::~AshWebView() = default;

AshWebView::InitParams::InitParams() = default;
AshWebView::InitParams::InitParams(const InitParams&) = default;
AshWebView::InitParams& AshWebView::InitParams::operator=(const InitParams&) =
    default;
AshWebView::InitParams::InitParams(InitParams&&) = default;
AshWebView::InitParams& AshWebView::InitParams::operator=(InitParams&&) =
    default;
AshWebView::InitParams::~InitParams() = default;

BEGIN_METADATA(AshWebView)
END_METADATA

}  // namespace ash
