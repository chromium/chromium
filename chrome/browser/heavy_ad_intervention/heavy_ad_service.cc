// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/blacklist/opt_out_blacklist/sql/opt_out_store_sql.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Whether an opt out store should be used or not.
bool HeavyAdOptOutStoreDisabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kHeavyAdPrivacyMitigations, "OptOutStoreDisabled", false);
}

HeavyAdService::HeavyAdService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

HeavyAdService::~HeavyAdService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void HeavyAdService::Initialize(const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kHeavyAdPrivacyMitigations))
    return;

  std::unique_ptr<blacklist::OptOutStoreSQL> opt_out_store;
  if (!HeavyAdOptOutStoreDisabled()) {
    // Get the background thread to run SQLite on.
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::BEST_EFFORT});

    opt_out_store = std::make_unique<blacklist::OptOutStoreSQL>(
        base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}),
        background_task_runner,
        profile_path.Append(chrome::kHeavyAdInterventionOptOutDBFilename));
  }

  heavy_ad_blocklist_ = std::make_unique<HeavyAdBlocklist>(
      std::move(opt_out_store), base::DefaultClock::GetInstance(), this);
}

void HeavyAdService::InitializeOffTheRecord() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kHeavyAdPrivacyMitigations))
    return;

  // Providing a null out_out_store which sets up the blocklist in-memory only.
  heavy_ad_blocklist_ = std::make_unique<HeavyAdBlocklist>(
      nullptr /* opt_out_store */, base::DefaultClock::GetInstance(), this);
}
