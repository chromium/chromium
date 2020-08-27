// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_keyed_service.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/lite_video/lite_video_decider.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/blocklist/opt_out_blocklist/sql/opt_out_store_sql.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

constexpr base::FilePath::CharType kLiteVideoOptOutDBFilename[] =
    FILE_PATH_LITERAL("lite_video_opt_out.db");

LiteVideoKeyedService::LiteVideoKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

LiteVideoKeyedService::~LiteVideoKeyedService() = default;

void LiteVideoKeyedService::Initialize(const base::FilePath& profile_path) {
  if (!lite_video::features::IsLiteVideoEnabled())
    return;

  std::unique_ptr<blocklist::OptOutStoreSQL> opt_out_store;
  // Get the background thread to run SQLite on.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  opt_out_store = std::make_unique<blocklist::OptOutStoreSQL>(
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}),
      background_task_runner, profile_path.Append(kLiteVideoOptOutDBFilename));

  optimization_guide::OptimizationGuideDecider* opt_guide_decider = nullptr;
  if (lite_video::features::LiteVideoUseOptimizationGuide()) {
    opt_guide_decider = OptimizationGuideKeyedServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context_));
  }

  decider_ = std::make_unique<lite_video::LiteVideoDecider>(
      std::move(opt_out_store), base::DefaultClock::GetInstance(),
      opt_guide_decider);
}

void LiteVideoKeyedService::ClearData(const base::Time& delete_begin,
                                      const base::Time& delete_end) {
  if (decider_)
    decider_->ClearData(delete_begin, delete_end);
}
