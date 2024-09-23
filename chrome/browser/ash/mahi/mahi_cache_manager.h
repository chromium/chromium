// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_

#include <map>
#include <string>

#include "base/timer/timer.h"
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
             const std::u16string& page_content,
             const std::optional<gfx::ImageSkia>& favicon_image,
             const std::optional<std::u16string>& summary,
             const std::vector<MahiQA>& previous_qa);
    MahiData(const MahiData&);
    MahiData& operator=(const MahiData&);
    ~MahiData();

    // URL of the webpage
    std::string url;
    // The title of the page.
    std::u16string title;
    // The extracted content of the page.
    std::u16string page_content;
    // The favicon of the page.
    std::optional<gfx::ImageSkia> favicon_image;
    // The summary of the page;
    std::optional<std::u16string> summary;
    // List of previous questions and answers for this page.
    std::vector<MahiQA> previous_qa;

    // Time of creation of this object.
    base::Time creation_time;
  };

  MahiCacheManager();

  MahiCacheManager(const MahiCacheManager&) = delete;
  MahiCacheManager& operator=(const MahiCacheManager&) = delete;

  ~MahiCacheManager();

  // Adds page cache for a given url. If the url exists in the cache, replace
  // with the new one.
  void AddCacheForUrl(const std::string& url, const MahiData& data);

  // Updates summary for a URL that is already in the cache. If the cache
  // doesn't contain the URL, does nothing.
  void TryToUpdateSummaryForUrl(const std::string& url,
                                const std::u16string& summary);

  // Returns the content for the given url.
  std::u16string GetPageContentForUrl(const std::string& url) const;

  // Returns the summary for the given url. If it's not in the cache, return
  // nullopt.
  std::optional<std::u16string> GetSummaryForUrl(const std::string& url) const;

  // Returns list of questions and answers for the given url.
  std::vector<MahiQA> GetQAForUrl(const std::string& url) const;

  // Clears the cache.
  void ClearCache();

  // Delete the page cache for a given url. Does nothing if the url doesn't
  // exist in the cache.
  void DeleteCacheForUrl(const std::string& url);

  // Gets size of the cache.
  int size() { return page_cache_.size(); }

 private:
  friend class MahiCacheManagerTest;

  // Called when the |periodic_timer_| triggers.
  void OnTimerFired();

  // Timer to trigger periodically for clearing cache.
  std::unique_ptr<base::RepeatingTimer> periodic_timer_;

  // A map from a url to it's corresponding data. It's used to store the cache
  // for mahi.
  std::map<GURL, MahiData> page_cache_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_CACHE_MANAGER_H_
