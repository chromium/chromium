// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_query_controller.h"

namespace lens {

LensOverlayQueryController::LensOverlayQueryController() = default;

LensOverlayQueryController::~LensOverlayQueryController() {}

void LensOverlayQueryController::StartQueryFlow(
    const SkBitmap& screenshot,
    base::OnceCallback<void(lens::proto::LensOverlayResponse)> callback) {}

void LensOverlayQueryController::EndQuery() {}

void LensOverlayQueryController::SendInteraction(
    lens::proto::LensOverlayRequest request,
    base::OnceCallback<void(const GURL&, std::string)> callback) {}

}  // namespace lens
