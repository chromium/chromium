// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/signin/public/base/signin_switches.h"

bool IsAccountManagerAvailable(const Profile* const profile) {
  if (!base::FeatureList::IsEnabled(switches::kUseAccountManagerFacade))
    return false;

  // Account Manager / Mirror is only enabled on Lacros's Main Profile for now.
  if (!profile->IsMainProfile())
    return false;

  // TODO(anastasiian): check for Web kiosk mode.

  // Account Manager is unavailable on Guest (Incognito) Sessions.
  if (profile->IsGuestSession() || profile->IsOffTheRecord())
    return false;

  // Account Manager is unavailable on Managed Guest Sessions / Public Sessions.
  if (profiles::IsPublicSession())
    return false;

  // Available in all other cases.
  return true;
}
