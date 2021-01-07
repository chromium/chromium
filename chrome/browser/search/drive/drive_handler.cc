// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_handler.h"
#include "base/strings/string_number_conversions.h"

DriveHandler::DriveHandler(
    mojo::PendingReceiver<drive::mojom::DriveHandler> handler)
    : handler_(this, std::move(handler)) {}

DriveHandler::~DriveHandler() = default;

void DriveHandler::GetTestString(GetTestStringCallback callback) {
  std::move(callback).Run("This is the return from the Drive Handler");
}
