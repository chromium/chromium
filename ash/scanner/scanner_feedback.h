// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_FEEDBACK_H_
#define ASH_SCANNER_SCANNER_FEEDBACK_H_

#include "ash/ash_export.h"
#include "base/values.h"

namespace manta::proto {
class ScannerAction;
}

namespace ash {

// Converts a `manta::proto::ScannerAction` into an "externally tagged"
// `base::Value::Dict` for use in filing feedback.
// Required as Chromium's lite runtime for Protobuf does not support descriptors
// and, by extension, text format.
ASH_EXPORT base::Value::Dict ScannerActionToDict(
    manta::proto::ScannerAction action);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_FEEDBACK_H_
