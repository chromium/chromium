// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

CartService::CartService(Profile* profile) : profile_(profile) {}
CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleDismissed, false);
}

void CartService::Dismiss() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleDismissed, true);
}

void CartService::Restore() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleDismissed, false);
}

bool CartService::IsDismissed() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleDismissed);
}
