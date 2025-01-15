// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
#define ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

// Represents a `manta::proto::ScannerAction` from an initial response from the
// Scanner service, which is expected to be "unpopulated".
//
// Takes in a `PopulateCallback` which should return a "populated"
// `manta::proto::ScannerAction`, or one with no action case if there is an
// error. Provides methods to populate this to a `ScannerAction`.
class ASH_EXPORT ScannerUnpopulatedAction {
 public:
  using PopulatedActionCallback =
      base::OnceCallback<void(manta::proto::ScannerAction populated_action)>;
  using PopulateCallback = base::RepeatingCallback<void(
      manta::proto::ScannerAction unpopulated_action,
      PopulatedActionCallback callback)>;

  // Returns nullopt iff `unpopulated_action.action_case() == ACTION_NOT_SET`.
  static std::optional<ScannerUnpopulatedAction> FromProto(
      manta::proto::ScannerAction unpopulated_action,
      PopulateCallback populate_to_proto_callback);

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

  // Calls the provided `PopulatedActionCallback` and asynchronously returns a
  // `ScannerAction`. If any errors occur, such as `callback` returning a
  // different type of action to this action, an empty action is returned.
  // This method will crash if this has been previously moved.
  void Populate(PopulatedActionCallback callback) const&;

 private:
  ScannerUnpopulatedAction(manta::proto::ScannerAction unpopulated_action,
                           PopulateCallback populate_to_proto_callback);

  // Guaranteed to have `action_case()` to be set to a known value.
  manta::proto::ScannerAction unpopulated_action_;
  PopulateCallback populate_callback_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_UNPOPULATED_ACTION_H_
