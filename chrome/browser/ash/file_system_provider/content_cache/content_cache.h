// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_

namespace ash::file_system_provider {

// A singleton that is the hub for all FileSystemProvider extensions that are
// enabled with a content cache. Currently this is just an experiment hidden
// behind the FileSystemProviderContentCache flag and only enabled on ODFS when
// the flag is toggled on.
class ContentCache {
 public:
  ContentCache();

  ContentCache(const ContentCache&) = delete;
  ContentCache& operator=(const ContentCache&) = delete;

  ~ContentCache();
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
