// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/memory_answer_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/power_utils.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/app_list/search/system_info/cpu_data.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_util.h"

namespace app_list {
namespace {

constexpr int kMemoryRefreshIntervalInSeconds = 10;
using AnswerCardInfo = ::ash::SystemInfoAnswerCardData;

}  // namespace

MemoryAnswerResult::MemoryAnswerResult(
    Profile* profile,
    const std::u16string& query,
    const std::string& url_path,
    const gfx::ImageSkia& icon,
    double relevance_score,
    const std::u16string& title,
    const std::u16string& description,
    SystemInfoCategory system_info_category,
    const AnswerCardInfo& answer_card_info,
    SystemInfoCardProvider::UpdateMemoryResultCallback callback,
    std::unique_ptr<base::RepeatingTimer> timer,
    SystemInfoCardProvider* provider)
    : SystemInfoAnswerResult(profile,
                             query,
                             url_path,
                             icon,
                             relevance_score,
                             title,
                             description,
                             system_info_category,
                             answer_card_info),
      callback_(std::move(callback)),
      timer_(std::move(timer)),
      provider_(provider) {
  provider_->AddMemoryObserver(this);
  timer_->Start(FROM_HERE, base::Seconds(kMemoryRefreshIntervalInSeconds),
                base::BindRepeating(&MemoryAnswerResult::UpdateResult,
                                    weak_factory_.GetWeakPtr()));
}

MemoryAnswerResult::~MemoryAnswerResult() {
  timer_->Stop();
  if (provider_ && this->IsInObserverList()) {
    provider_->RemoveMemoryObserver(this);
  }
}

void MemoryAnswerResult::OnMemoryUpdated(const double memory_usage_percentage,
                                         const std::u16string& description) {
  UpdateTitleAndDetails(u"", description);
  UpdateBarChartPercentage(memory_usage_percentage);
}

void MemoryAnswerResult::UpdateResult() {
  callback_.Run(/*create_result=*/false);
}

}  // namespace app_list
