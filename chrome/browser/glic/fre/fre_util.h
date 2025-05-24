// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_
#define CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_

class GURL;
class Profile;
class ThemeService;

namespace content {
class BrowserContext;
class StoragePartitionConfig;
}  // namespace content

namespace glic {

GURL GetFreURL(Profile* profile);
bool UseDarkMode(ThemeService* theme_service);

// Returns the storage partition config used for the Glic first-run experience.
content::StoragePartitionConfig GetFreStoragePartitionConfig(
    content::BrowserContext* browser_context);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_FRE_UTIL_H_
