// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/chrome_dips_delegate.h"

#include <memory>

#include "base/check.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_browser_signin_detector.h"
#include "chrome/browser/dips/stateful_bounce_counter.h"
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

ChromeDipsDelegate::ChromeDipsDelegate(PassKey) {}

// static
std::unique_ptr<content::DipsDelegate> ChromeDipsDelegate::Create() {
  return std::make_unique<ChromeDipsDelegate>(PassKey());
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

void ChromeDipsDelegate::OnDipsServiceCreated(
    content::BrowserContext* browser_context,
    DIPSService* dips_service) {
  // Create DIPSBrowserSigninDetector.
  CHECK(DIPSBrowserSigninDetector::Get(browser_context));
  dips::StatefulBounceCounter::CreateFor(dips_service);
}
