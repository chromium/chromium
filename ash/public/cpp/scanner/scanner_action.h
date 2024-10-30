// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_

#include <variant>

#include "components/manta/proto/scanner.pb.h"

namespace ash {

// Holds a particular action the user can complete in a ScannerSession,
// equivalently a single command that can be applied to the system.
using ScannerAction = std::variant<manta::proto::NewEventAction,
                                   manta::proto::NewContactAction,
                                   manta::proto::NewGoogleDocAction,
                                   manta::proto::NewGoogleSheetAction,
                                   manta::proto::CopyToClipboardAction>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
