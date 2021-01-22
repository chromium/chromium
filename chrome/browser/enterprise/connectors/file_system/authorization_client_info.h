// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_AUTHORIZATION_CLIENT_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_AUTHORIZATION_CLIENT_INFO_H_

namespace enterprise_connectors {
// OAuth2 client info for the FileSystem Connector; tokens are stored in
// connectors_prefs.h/cc
extern const char kFileSystemClientId[];
extern const char kFileSystemClientSecret[];
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_AUTHORIZATION_CLIENT_INFO_H_
