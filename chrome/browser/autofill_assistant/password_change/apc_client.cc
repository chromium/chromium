// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client.h"

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

// static
void ApcClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAutofillAssistantOnDesktopEnabled,
                                false);
}

// static
ApcClient* ApcClient::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  ApcClientImpl::CreateForWebContents(web_contents);
  return ApcClientImpl::FromWebContents(web_contents);
}
