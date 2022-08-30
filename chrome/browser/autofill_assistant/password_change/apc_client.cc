// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client.h"

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

// static
void ApcClient::RegisterPrefs(PrefRegistrySimple* registry) {
  // TODO(crbug.com/1358131): Set default value to true once the pref keys
  // been adjusted in Settings.
  registry->RegisterBooleanPref(prefs::kAutofillAssistantOnDesktopEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kAutofillAssistantOnDesktopConsent,
                                false);
}

// static
ApcClient* ApcClient::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  ApcClientImpl::CreateForWebContents(web_contents);
  return ApcClientImpl::FromWebContents(web_contents);
}
