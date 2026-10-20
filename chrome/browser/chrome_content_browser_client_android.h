// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_

class GURL;

namespace content {
class PosixFileDescriptorInfo;
}  // namespace content

void GetMappedLocalePacksForChildProcess(
    content::PosixFileDescriptorInfo* mappings);

// Returns whether navigation to a content:// URI is allowed in Chrome on
// Android. Blocks navigations to unexported (android:exported="false") content
// providers running under Chrome's Android UID, which Chrome can otherwise
// open without URI permission grants on behalf of web content. Also blocks
// invalid content:// URLs and ones without an authority. Allows everything if
// features::kBlockNonExportedContentProviderNavigation is disabled.
// `url` must satisfy url.SchemeIs(url::kContentScheme).
bool IsContentSchemeURLNavigationAllowed(const GURL& url);

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_
