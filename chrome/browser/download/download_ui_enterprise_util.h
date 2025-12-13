// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_ENTERPRISE_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_ENTERPRISE_UTIL_H_

class GURL;
class Profile;

namespace download {

// The enterprise download connectors can be enabled in blocking or nonblocking
// mode. This returns false if the connector is disabled.
bool DoesDownloadConnectorBlock(Profile* profile, const GURL& url);

}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UI_ENTERPRISE_UTIL_H_
