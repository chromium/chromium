// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

namespace {

const char kMicrosoftAuthLastDismissedTimePrefName[] =
    "NewTabPage.MicrosoftAuthentication.LastDimissedTime";

}  // namespace

// static
void MicrosoftAuthPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kMicrosoftAuthLastDismissedTimePrefName,
                             base::Time());
}

MicrosoftAuthPageHandler::MicrosoftAuthPageHandler(
    mojo::PendingReceiver<ntp::authentication::mojom::MicrosoftAuthPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      profile_(profile),
      pref_service_(profile_->GetPrefs()) {}

MicrosoftAuthPageHandler::~MicrosoftAuthPageHandler() = default;

void MicrosoftAuthPageHandler::DismissModule() {
  // TODO(b:377378212): Resurface module after 12 hours.
  pref_service_->SetTime(kMicrosoftAuthLastDismissedTimePrefName,
                         base::Time::Now());
}

void MicrosoftAuthPageHandler::RestoreModule() {
  pref_service_->SetTime(kMicrosoftAuthLastDismissedTimePrefName, base::Time());
}
