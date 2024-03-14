// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_

#include <map>
#include <string>

#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

// A helper class that help MahiManager to manage its cache.
class MahiCacheManager {
 public:
  struct MahiQA {
    std::u16string question;
    std::u16string answer;
  };

  // Struct that store data of a page.
  struct MahiData {
    MahiData();
    MahiData(const std::string& url,
             const std::u16string& title,
             const std::optional<gfx::ImageSkia>& favicon_image,
             const std::u16string& page_content,
             const std::u16string& summary,
             const std::vector<MahiQA>& previous_qa);
    MahiData(const MahiData&);
    MahiData& operator=(const MahiData&);
    ~MahiData();

    // URL of the webpage
    std::string url;
    // The title of the page.
    std::u16string title;
    // The favicon of the page.
    std::optional<gfx::ImageSkia> favicon_image;
    // The extracted content of the page.
    std::u16string page_content;
    // The summary of the page;
    std::u16string summary;
    // List of previous questions and answers for this page.
    std::vector<MahiQA> previous_qa;
  };

  MahiCacheManager();

  MahiCacheManager(const MahiCacheManager&) = delete;
  MahiCacheManager& operator=(const MahiCacheManager&) = delete;

  ~MahiCacheManager();

  // Add page cache for a given url. If the url exists in the cache, replace
  // with the new one.
  void AddCacheForUrl(const std::string& url, const MahiData& data);

  // Return the summary for the given url. If it's not in the cache, return
  // nullopt.
  std::optional<std::u16string> GetSummaryForUrl(const std::string& url) const;

  // Return list of questions and answers for the given url.
  std::vector<MahiQA> GetQAForUrl(const std::string& url) const;

  // Clear the cache.
  void ClearCache();

 private:
  friend class MahiCacheManagerTest;

  // A map from a url to it's corresponding data. It's used to store the cache
  // for mahi.
  std::map<GURL, MahiData> page_cache_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_
