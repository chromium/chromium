// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

namespace ash {

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

CoralController::CoralController() = default;

CoralController::~CoralController() = default;

void CoralController::GenerateContentGroups(const CoralRequest& request,
                                            CoralResponseCallback callback) {
  // Not implemented yet.
  std::move(callback).Run(nullptr);
}

void CoralController::CacheEmbeddings(const CoralRequest& request,
                                      base::OnceCallback<void(bool)> callback) {
  // Not implemented yet.
  std::move(callback).Run(false);
}

}  // namespace ash
