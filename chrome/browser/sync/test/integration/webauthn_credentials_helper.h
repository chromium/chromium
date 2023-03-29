// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_

#include "components/webauthn/core/browser/passkey_model.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace webauthn_credentials_helper {

PasskeyModel& GetModel(int profile_idx);

bool AwaitAllModelsMatch();

// Returns a new WebauthnCredentialSpecifics entity with a random credential ID,
// and fixed RP ID and user ID.
sync_pb::WebauthnCredentialSpecifics NewPasskey();

}  // namespace webauthn_credentials_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
