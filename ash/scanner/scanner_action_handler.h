// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ACTION_HANDLER_H_
#define ASH_SCANNER_SCANNER_ACTION_HANDLER_H_

#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/functional/callback.h"

namespace ash {

// Given a ScannerAction this method will apply the contained command to the
// system. The callback passed will be invoked after the action has completed,
// with a bool specifying if the command was completed successfully.
ASH_EXPORT void HandleScannerAction(
    const ScannerAction& action,
    base::OnceCallback<void(bool success)> callback);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ACTION_HANDLER_H_
