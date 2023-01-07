// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_REMOVE_REQUESTED_PREF_HANDLER_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_REMOVE_REQUESTED_PREF_HANDLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace arc {
namespace data_snapshotd {

// This class handles ARC data remove requests and notifies the owner if ARC
// data remove is requested.
class ArcDataRemoveRequestedPrefHandler final {
 public:
  ArcDataRemoveRequestedPrefHandler(const ArcDataRemoveRequestedPrefHandler&) =
      delete;
  ArcDataRemoveRequestedPrefHandler& operator=(
      const ArcDataRemoveRequestedPrefHandler&) = delete;
  ~ArcDataRemoveRequestedPrefHandler();

  // Creates the instance if ARC data removal is not requested yet, otherwise
  // returns nullptr and calls |callback| right away.
  // |callback| is called once ARC data removal is requested.
  static std::unique_ptr<ArcDataRemoveRequestedPrefHandler> Create(
      PrefService* prefs,
      base::OnceClosure callback);

 private:
  // The instance observes the changes of kArcDataRemoveRequested in |prefs|.
  // |callback| is invoked once the pref is set.
  ArcDataRemoveRequestedPrefHandler(PrefService* prefs,
                                    base::OnceClosure callback);
  void OnPrefChanged();

  // If |callback| is a valid callback, it is invoked once ARC data remove is
  // requested.
  base::OnceClosure callback_;
  // Observes changes to kArcDataRemoveRequested pref.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ArcDataRemoveRequestedPrefHandler> weak_ptr_factory_{
      this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_REMOVE_REQUESTED_PREF_HANDLER_H_
