// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace extensions {

// Converts a `PublicKeyCredentialCreationOptions` into a `base::Value`, which
// can be JSON serialized and included in an
// `webAuthenticationProxy.onCreateRequest` event.
//
// The output conforms to the WebAuthn `PublicKeyCredentialCreationOptions`
// dictionary IDL, but with all ArrayBuffer-valued attributes represented as
// base64URL-encoded strings instead.
// (https://w3c.github.io/webauthn/#dictdef-publickeycredentialcreationoptions)
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options);

// Converts a `base::Value` encoding a WebAuthn
// `PublicKeyCredential` instance into an equivalent
// `MakeCredentialAuthenticatorResponse`.
//
// The input is expected to be a JSON-serialized `PublicKeyCredential` in which
// ArrayBuffer-valued attributes are replaced by base64URL-encoded strings.
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
blink::mojom::MakeCredentialAuthenticatorResponsePtr FromValue(
    const base::Value& value);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_
