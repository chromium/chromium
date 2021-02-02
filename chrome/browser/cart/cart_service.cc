// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

CartService::CartService(Profile* profile)
    : profile_(profile), cart_db_(std::make_unique<CartDB>(profile_)) {}
CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleHidden, false);
  registry->RegisterBooleanPref(prefs::kCartModuleRemoved, false);
}

void CartService::Hide() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, true);
}

void CartService::RestoreHidden() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, false);
}

bool CartService::IsHidden() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleHidden);
}

void CartService::Remove() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleRemoved, true);
}

void CartService::RestoreRemoved() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleRemoved, false);
}

bool CartService::IsRemoved() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleRemoved);
}
