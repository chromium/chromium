// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/arc_data_remove_requested_pref_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_service.h"

namespace arc {
namespace data_snapshotd {

// static
std::unique_ptr<ArcDataRemoveRequestedPrefHandler>
ArcDataRemoveRequestedPrefHandler::Create(PrefService* prefs,
                                          base::OnceClosure callback) {
  if (prefs->GetBoolean(prefs::kArcDataRemoveRequested)) {
    std::move(callback).Run();
    return nullptr;
  }
  return base::WrapUnique(
      new ArcDataRemoveRequestedPrefHandler(prefs, std::move(callback)));
}

ArcDataRemoveRequestedPrefHandler::~ArcDataRemoveRequestedPrefHandler() =
    default;

ArcDataRemoveRequestedPrefHandler::ArcDataRemoveRequestedPrefHandler(
    PrefService* prefs,
    base::OnceClosure callback) {
  DCHECK(prefs);
  DCHECK(!prefs->GetBoolean(prefs::kArcDataRemoveRequested));

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kArcDataRemoveRequested,
      base::BindRepeating(&ArcDataRemoveRequestedPrefHandler::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  callback_ = std::move(callback);
}

void ArcDataRemoveRequestedPrefHandler::OnPrefChanged() {
  if (callback_.is_null())
    return;
  std::move(callback_).Run();
}

}  // namespace data_snapshotd
}  // namespace arc
