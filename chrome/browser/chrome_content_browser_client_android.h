// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_

namespace content {
class PosixFileDescriptorInfo;
}  // namespace content

void GetMappedLocalePacksForChildProcess(
    content::PosixFileDescriptorInfo* mappings);

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_ANDROID_H_
