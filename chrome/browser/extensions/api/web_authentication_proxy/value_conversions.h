// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace extensions {

// TODO(https://crbug.com/1231802): Cover with unit tests.

// Converts a `PublicKeyCredentialCreationOptions` into a `base::Value`, which
// can be JSON serialized and included in an
// `webAuthenticationProxy.onCreateRequest` event.
//
// The output conforms to the WebAuthn `PublicKeyCredentialCreationOptions`
// dictionary IDL, but with all ArrayBuffer and BufferSource attributes
// represented as base64url-encoded strings instead. The `timeout` field is
// omitted.
// (https://w3c.github.io/webauthn/#dictdef-publickeycredentialcreationoptions)
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options);

// Converts a `PublicKeyCredentialRequestOptions` into a `base::Value`, which
// can be JSON serialized and included in an
// `webAuthenticationProxy.onGetRequest` event.
//
// The output conforms to the WebAuthn `PublicKeyCredentialRequestOptions`
// dictionary IDL, but with all ArrayBuffer and BufferSource attributes
// represented as base64url-encoded strings instead. The `timeout` field is
// omitted.
// (https://w3c.github.io/webauthn/#dictdef-publickeycredentialrequestoptions)
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
base::Value ToValue(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `MakeCredentialAuthenticatorResponse`.
//
// The input is expected to be a JSON-serialized `PublicKeyCredential` in which
// ArrayBuffer-valued attributes are replaced by base64url-encoded strings. The
// `response` value must be a an `AuthenticatorAttestationResponse`.
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
blink::mojom::MakeCredentialAuthenticatorResponsePtr
MakeCredentialResponseFromValue(const base::Value& value);

// Converts a `base::Value` encoding a `PublicKeyCredential` instance from a
// WebAuthn `get()` request into a `GetAssertionAuthenticatorResponse`.
//
// The input is expected to be a JSON-serialized `PublicKeyCredential` in which
// ArrayBuffer-valued attributes are replaced by base64url-encoded strings. The
// `response` value must be a an `AuthenticatorAssertionResponse`.
//
// TODO(crbug.com/1231802): Reference serialization method once available. Also
// update the IDL docs at that point.
blink::mojom::GetAssertionAuthenticatorResponsePtr
GetAssertionResponseFromValue(const base::Value& value);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_VALUE_CONVERSIONS_H_
