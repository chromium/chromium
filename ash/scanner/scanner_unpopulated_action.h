// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
#define ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/functional/callback.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

// Represents a `manta::proto::ScannerAction` from an initial response from the
// Scanner service, which is expected to be "unpopulated".
//
// Takes in a `PopulateToProtoCallback` which should return a "populated"
// `manta::proto::ScannerAction`. Provides methods to populate this to a
// `ScannerAction`.
class ASH_EXPORT ScannerUnpopulatedAction {
 public:
  using PopulatedProtoCallback = base::OnceCallback<void(
      std::optional<manta::proto::ScannerAction> populated_action)>;
  using PopulateToProtoCallback = base::RepeatingCallback<void(
      manta::proto::ScannerAction unpopulated_action,
      PopulatedProtoCallback callback)>;

  using PopulateToVariantCallback =
      base::OnceCallback<void(std::optional<ScannerAction> action)>;

  // Returns nullopt iff `unpopulated_action.action_case() == ACTION_NOT_SET`.
  static std::optional<ScannerUnpopulatedAction> FromProto(
      manta::proto::ScannerAction unpopulated_action,
      PopulateToProtoCallback populate_to_proto_callback);

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

  // Calls the provided `PopulateToProtoCallback` and asynchronously returns a
  // `ScannerAction`. If any errors occur, such as `PopulateToProtoCallback`
  // returning a different type of action to this action, nullopt is returned.
  // This method will crash if this has been previously moved.
  void PopulateToVariant(PopulateToVariantCallback callback) const&;

 private:
  ScannerUnpopulatedAction(manta::proto::ScannerAction unpopulated_action,
                           PopulateToProtoCallback populate_to_proto_callback);

  // Guaranteed to have `action_case()` to be set to a known value.
  manta::proto::ScannerAction unpopulated_action_;
  PopulateToProtoCallback populate_to_proto_callback_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
