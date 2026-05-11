// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_
#define CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_

class GURL;
class Profile;

namespace content {
class BrowserContext;
class StoragePartitionConfig;
}  // namespace content

namespace glic {

// Decorates any Glic guest FRE URL (such as the Glic FRE URL or the
// experimental opt-in URL) with standard dynamic environment parameters like
// hotkeys, theme, and localization.
GURL DecorateGlicFreUrl(Profile* profile, GURL url);

// Returns fully-decorated Glic FRE URL.
GURL GetFreURL(Profile* profile);

// Returns the storage partition config used for the Glic first-run experience.
content::StoragePartitionConfig GetFreStoragePartitionConfig(
    content::BrowserContext* browser_context);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_
