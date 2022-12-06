// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_scoped_pref_update.h"

#include "base/values.h"

namespace arc {

ArcAppScopedPrefUpdate::ArcAppScopedPrefUpdate(PrefService* service,
                                               const std::string& id,
                                               const std::string& path)
    : id_(id), pref_update_(service, path) {}

ArcAppScopedPrefUpdate::~ArcAppScopedPrefUpdate() = default;

base::Value::Dict& ArcAppScopedPrefUpdate::Get() {
  return *pref_update_->EnsureDict(id_);
}

}  // namespace arc
