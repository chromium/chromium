// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
#define ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

// Represents a `manta::proto::ScannerAction` from an initial response from the
// Scanner service, which is expected to be "unpopulated".
//
// Additionally stores JPEG bytes to be used to populate the action.
class ASH_EXPORT ScannerUnpopulatedAction {
 public:
  // Returns nullopt iff `unpopulated_action.action_case() == ACTION_NOT_SET`.
  static std::optional<ScannerUnpopulatedAction> FromProto(
      manta::proto::ScannerAction unpopulated_action,
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes);

  ScannerUnpopulatedAction(const ScannerUnpopulatedAction&);
  ScannerUnpopulatedAction& operator=(const ScannerUnpopulatedAction&);
  ScannerUnpopulatedAction(ScannerUnpopulatedAction&&);
  ScannerUnpopulatedAction& operator=(ScannerUnpopulatedAction&&);

  ~ScannerUnpopulatedAction();

  // If this has not been previously moved, this will never return
  // `ACTION_NOT_SET` and should always be a known enum value.
  manta::proto::ScannerAction::ActionCase action_case() const {
    return unpopulated_action_.action_case();
  }

  const manta::proto::ScannerAction& unpopulated_action() const {
    return unpopulated_action_;
  }

  const scoped_refptr<base::RefCountedMemory>& downscaled_jpeg_bytes() const {
    return downscaled_jpeg_bytes_;
  }

 private:
  ScannerUnpopulatedAction(
      manta::proto::ScannerAction unpopulated_action,
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes);

  // Guaranteed to have `action_case()` to be set to a known value.
  manta::proto::ScannerAction unpopulated_action_;
  scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
