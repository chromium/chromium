// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_ACTION_HANDLER_H_
#define ASH_SCANNER_SCANNER_ACTION_HANDLER_H_

#include <string_view>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/scanner/scanner_command.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class ScannerCommandDelegate;

// The callback which is called when a command to the system is finished. A
// boolean is provided to signify whether the command succeeded or not.
using ScannerCommandCallback = base::OnceCallback<void(bool success)>;

// Converts a `ScannerAction` to a `ScannerCommand` for use in
// `HandleScannerCommand`.
ASH_EXPORT ScannerCommand ScannerActionToCommand(ScannerAction action);

// Given a ScannerCommand this method will apply the contained command to the
// system. The callback passed will be invoked after the action has completed,
// with a bool specifying if the command was completed successfully.
//
// Requires a `ScannerCommandDelegate` to perform the actions given. All calls
// to `delegate`'s methods are guaranteed to be on the same sequence as the
// sequence which called this function.
//
// As `delegate`'s methods may be called asynchronously from this function, this
// function expects a weak pointer to it to ensure that use after free errors do
// not occur. If at any point `delegate` is null when this function attempts to
// call methods on `delegate`, `callback` will be called with a success value of
// false.
ASH_EXPORT void HandleScannerCommand(
    base::WeakPtr<ScannerCommandDelegate> delegate,
    ScannerCommand command,
    ScannerCommandCallback callback);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_ACTION_HANDLER_H_
