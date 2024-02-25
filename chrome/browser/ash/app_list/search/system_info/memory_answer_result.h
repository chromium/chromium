// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_MEMORY_ANSWER_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_MEMORY_ANSWER_RESULT_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

namespace app_list {

class MemoryAnswerResult : public SystemInfoAnswerResult,
                           public SystemInfoCardProvider::MemoryObserver {
 public:
  MemoryAnswerResult(
      Profile* profile,
      const std::u16string& query,
      const std::string& url_path,
      const gfx::ImageSkia& icon,
      double relevance_score,
      const std::u16string& title,
      const std::u16string& description,
      const std::u16string& accessibility_label,
      SystemInfoCategory system_info_category,
      SystemInfoCardType system_info_card_type,
      const ash::SystemInfoAnswerCardData& answer_card_info,
      SystemInfoCardProvider::UpdateMemoryResultCallback callback,
      std::unique_ptr<base::RepeatingTimer> timer,
      SystemInfoCardProvider* provider);

  ~MemoryAnswerResult() override;

  MemoryAnswerResult(const MemoryAnswerResult& other) = delete;
  MemoryAnswerResult& operator=(const MemoryAnswerResult& other) = delete;

  void OnMemoryUpdated(const double memory_usage_percentage,
                       const std::u16string& description,
                       const std::u16string& accessibility_label) override;
  void UpdateResult();

 private:
  SystemInfoCardProvider::UpdateMemoryResultCallback callback_;
  std::unique_ptr<base::RepeatingTimer> timer_;
  raw_ptr<SystemInfoCardProvider, DanglingUntriaged> provider_;
  base::WeakPtrFactory<MemoryAnswerResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_MEMORY_ANSWER_RESULT_H_
