// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_DEVELOPER_ID_CERTIFICATE_REAUTHORIZE_H_
#define CHROME_BROWSER_MAC_DEVELOPER_ID_CERTIFICATE_REAUTHORIZE_H_

namespace chrome {

// Performs Developer ID certificate reauthorization. In branded builds, this
// rewrites the Safe Storage item in the Keychain anew, so that its access
// control list includes the identity of the running process taken from its
// designated requirement. This is done so that the product can retain access to
// the Safe Storage item through a Developer ID code signing certificate change.
// Reauthorization is attempted a maximum of two times, and is not attempted if
// a successful reauthorizatin already occurred. If reauthorization is to be
// attempted and the running code has access to Safe Storage items even when
// limited to being accessed by applications signed with the old certificate,
// the attempt will be made in-process. Otherwise, a helper stub executable
// signed with the old certificate will be launched to attempt reauthorization.
void DeveloperIDCertificateReauthorizeInApp();

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_DEVELOPER_ID_CERTIFICATE_REAUTHORIZE_H_
