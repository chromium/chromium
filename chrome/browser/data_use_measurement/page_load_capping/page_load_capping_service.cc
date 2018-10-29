// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_blacklist.h"
#include "chrome/common/chrome_constants.h"
#include "components/blacklist/opt_out_blacklist/sql/opt_out_store_sql.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Whether an opt out store should be used or not.
bool PageCappingOptOutStoreDisabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      "OptOutStoreDisabled", false);
}

PageLoadCappingService::PageLoadCappingService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PageLoadCappingService::~PageLoadCappingService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PageLoadCappingService::Initialize(const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<blacklist::OptOutStoreSQL> opt_out_store;

  if (!PageCappingOptOutStoreDisabled()) {
    // Get the background thread to run SQLite on.
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    opt_out_store = std::make_unique<blacklist::OptOutStoreSQL>(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI}),
        background_task_runner,
        profile_path.Append(chrome::kPageLoadCappingOptOutDBFilename));
  }

  page_load_capping_blacklist_ = std::make_unique<PageLoadCappingBlacklist>(
      std::move(opt_out_store), base::DefaultClock::GetInstance(), this);
}
