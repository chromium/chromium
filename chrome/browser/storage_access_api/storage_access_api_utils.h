// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_UTILS_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_UTILS_H_

namespace content {
class RenderFrameHost;
}

// Returns true if the given frame should be disallowed from interacting with
// unpartitioned storage and the `storage-access` permission (including querying
// the permission status).
bool IsAccessRestrictedInFrame(content::RenderFrameHost* rfh);

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_UTILS_H_
