// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_

#include <string>
#include <vector>

#include "base/sequence_checker.h"

namespace base {
class FilePath;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

class MediaEngagementPreloadedList {
 public:
  static MediaEngagementPreloadedList* GetInstance();

  MediaEngagementPreloadedList();

  MediaEngagementPreloadedList(const MediaEngagementPreloadedList&) = delete;
  MediaEngagementPreloadedList& operator=(const MediaEngagementPreloadedList&) =
      delete;

  ~MediaEngagementPreloadedList();

  // Load the contents from |path|.
  bool LoadFromFile(const base::FilePath& path);

  // Checks whether |origin| has a high global engagement and is present in the
  // preloaded list.
  bool CheckOriginIsPresent(const url::Origin& origin) const;

  // Check whether we have loaded a list.
  bool loaded() const;

  // Check whether the list we have loaded is empty.
  bool empty() const;

 protected:
  friend class MediaEngagementPreloadedListTest;

  enum class DafsaResult {
    // The string was not found.
    kNotFound = -1,

    // The string was found.
    kFoundHttpsOnly,

    // The string was found and should allow both HTTP and HTTPS origins.
    kFoundHttpOrHttps,
  };

  // Checks if |input| is present in the preloaded data.
  DafsaResult CheckStringIsPresent(const std::string& input) const;

 private:
  // The preloaded data in dafsa format.
  std::vector<unsigned char> dafsa_;

  // If a list has been successfully loaded.
  bool is_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_PRELOADED_LIST_H_
