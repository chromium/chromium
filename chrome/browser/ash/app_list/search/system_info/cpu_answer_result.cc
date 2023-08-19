// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/cpu_answer_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"

namespace app_list {
namespace {

constexpr int kCpuUsageRefreshIntervalInSeconds = 1;
using AnswerCardInfo = ::ash::SystemInfoAnswerCardData;

}  // namespace

CpuAnswerResult::CpuAnswerResult(
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
    const AnswerCardInfo& answer_card_info,
    SystemInfoCardProvider::UpdateCpuResultCallback callback,
    std::unique_ptr<base::RepeatingTimer> timer,
    SystemInfoCardProvider* provider)
    : SystemInfoAnswerResult(profile,
                             query,
                             url_path,
                             icon,
                             relevance_score,
                             title,
                             description,
                             accessibility_label,
                             system_info_category,
                             system_info_card_type,
                             answer_card_info),
      callback_(std::move(callback)),
      timer_(std::move(timer)),
      provider_(provider) {
  provider_->AddCpuDataObserver(this);
  timer_->Start(FROM_HERE, base::Seconds(kCpuUsageRefreshIntervalInSeconds),
                base::BindRepeating(&CpuAnswerResult::UpdateResult,
                                    weak_factory_.GetWeakPtr()));
}

CpuAnswerResult::~CpuAnswerResult() {
  timer_->Stop();
  if (provider_ && this->IsInObserverList()) {
    provider_->RemoveCpuDataObserver(this);
  }
}

void CpuAnswerResult::OnCpuDataUpdated(
    const std::u16string& title,
    const std::u16string& description,
    const std::u16string& accessibility_label) {
  UpdateTitleAndDetails(title, description, accessibility_label);
}

void CpuAnswerResult::UpdateResult() {
  callback_.Run(/*create_result=*/false);
}

}  // namespace app_list
