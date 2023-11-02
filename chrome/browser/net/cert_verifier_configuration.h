// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_
#define CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_

#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

cert_verifier::mojom::CertVerifierServiceParamsPtr
GetChromeCertVerifierServiceParams();

#endif  // CHROME_BROWSER_NET_CERT_VERIFIER_CONFIGURATION_H_
