// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ANSWER_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ANSWER_RESULT_H_

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

class SystemInfoAnswerResult : public ChromeSearchResult {
 public:
  SystemInfoAnswerResult(Profile* profile,
                         const std::u16string& query,
                         const std::string& url_path);
  SystemInfoAnswerResult(const SystemInfoAnswerResult&) = delete;
  SystemInfoAnswerResult& operator=(const SystemInfoAnswerResult&) = delete;

  ~SystemInfoAnswerResult() override;

  void Open(int event_flags) override;

 private:
  void UpdateIcon();

  Profile* const profile_;
  const std::string url_path_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_SYSTEM_INFO_ANSWER_RESULT_H_
