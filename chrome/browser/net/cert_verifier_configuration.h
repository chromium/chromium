// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_
#define CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_

#include "components/prefs/pref_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

// Gets parameters to use for creating the Chrome Cert Verifier.
//
// `local_state` may be passed in to support running in minimal_browser_mode,
// where some services start up before the Browser process
// (see
// https://docs.google.com/document/d/1ybmGWRWXu0aYNxA99IcHFesDAslIaO1KFP6eGdHTJaE/edit#heading=h.7bk05syrcom).
//
// If `local_state` is null, g_browser_process->local_state() will be used.
cert_verifier::mojom::CertVerifierServiceParamsPtr
GetChromeCertVerifierServiceParams(PrefService* local_state);

#endif  // CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_
