// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_KEY_CREDENTIAL_MANAGER_SUPPORT_REPORTER_WIN_H_
#define CHROME_BROWSER_METRICS_KEY_CREDENTIAL_MANAGER_SUPPORT_REPORTER_WIN_H_

namespace key_credential_manager_support {

// Report whether the current device is capable of provisioning a key
// credential.
void ReportKeyCredentialManagerSupport();

}  // namespace key_credential_manager_support

#endif  // CHROME_BROWSER_METRICS_KEY_CREDENTIAL_MANAGER_SUPPORT_REPORTER_WIN_H_
