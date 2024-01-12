// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_ANSWER_RESULT_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_ANSWER_RESULT_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_card_provider.h"

namespace app_list {

class CpuAnswerResult : public SystemInfoAnswerResult,
                        public SystemInfoCardProvider::CpuDataObserver {
 public:
  CpuAnswerResult(Profile* profile,
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
                  SystemInfoCardProvider::UpdateCpuResultCallback callback,
                  std::unique_ptr<base::RepeatingTimer> timer,
                  SystemInfoCardProvider* provider);

  ~CpuAnswerResult() override;

  CpuAnswerResult(const CpuAnswerResult& other) = delete;
  CpuAnswerResult& operator=(const CpuAnswerResult& other) = delete;

  void OnCpuDataUpdated(const std::u16string& title,
                        const std::u16string& description,
                        const std::u16string& accessibility_label) override;
  void UpdateResult();

 private:
  SystemInfoCardProvider::UpdateCpuResultCallback callback_;
  std::unique_ptr<base::RepeatingTimer> timer_;
  raw_ptr<SystemInfoCardProvider, DanglingUntriaged> provider_;
  base::WeakPtrFactory<CpuAnswerResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SYSTEM_INFO_CPU_ANSWER_RESULT_H_
