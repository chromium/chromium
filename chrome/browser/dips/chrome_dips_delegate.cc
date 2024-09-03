// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/chrome_dips_delegate.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/dips_delegate.h"

namespace {

ProfileSelections GetHumanProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOffTheRecordOnly)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

}  // namespace

// static
std::unique_ptr<content::DipsDelegate> ChromeDipsDelegate::Create() {
  return std::make_unique<ChromeDipsDelegate>();
}

bool ChromeDipsDelegate::ShouldEnableDips(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* result = GetHumanProfileSelections().ApplyProfileSelection(profile);
  // TODO: crbug.com/358137275 - Use CHECK() once we know it's safe.
  DUMP_WILL_BE_CHECK(!result || result == profile)
      << "ApplyProfileSelection() returned a different profile";
  return result == profile;
}
