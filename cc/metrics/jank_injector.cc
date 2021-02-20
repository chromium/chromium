// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/jank_injector.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "url/gurl.h"

namespace cc {

namespace {

const char kJankInjectionAllowedURLs[] = "allowed_urls";
const char kJankInjectionClusterSize[] = "cluster";
const char kJankInjectionTargetPercent[] = "percent";

struct JankInjectionParams {
  JankInjectionParams() = default;
  ~JankInjectionParams() = default;

  JankInjectionParams(JankInjectionParams&&) = default;
  JankInjectionParams& operator=(JankInjectionParams&&) = default;

  JankInjectionParams(const JankInjectionParams&) = delete;
  JankInjectionParams& operator=(const JankInjectionParams&) = delete;

  // The jank injection code blocks the main thread for |jank_duration| amount
  // of time.
  base::TimeDelta jank_duration;

  // When |busy_loop| is set, blocks the main thread in a busy loop for
  // |jank_duration|. Otherwise, sleeps for |jank_duration|.
  bool busy_loop = true;
};

bool IsJankInjectionEnabled() {
  static bool enabled =
      base::FeatureList::IsEnabled(features::kJankInjectionAblationFeature);
  return enabled;
}

using AllowedURLsMap = std::map<std::string, std::vector<std::string>>;
// Returns a map of <host, <list of paths>> pairs.
AllowedURLsMap GetAllowedURLs() {
  DCHECK(IsJankInjectionEnabled());
  AllowedURLsMap urls;
  std::string url_list = base::GetFieldTrialParamValueByFeature(
      features::kJankInjectionAblationFeature, kJankInjectionAllowedURLs);
  for (auto& it : base::SplitString(url_list, ",", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_ALL)) {
    GURL url = GURL(it);
    urls[url.host()].emplace_back(url.path());
  }
  return urls;
}

bool IsJankInjectionEnabledForURL(const GURL& url) {
  DCHECK(IsJankInjectionEnabled());
  static base::NoDestructor<AllowedURLsMap> allowed_urls(GetAllowedURLs());
  if (allowed_urls->empty())
    return false;

  const auto iter = allowed_urls->find(url.host());
  if (iter == allowed_urls->end())
    return false;

  const auto& paths = iter->second;
  const auto& path = url.path_piece();
  return paths.end() !=
         std::find_if(paths.begin(), paths.end(), [path](const std::string& p) {
           return base::StartsWith(path, p);
         });
}

void RunJank(JankInjectionParams params) {
  TRACE_EVENT0("cc,benchmark", "Injected Jank");
  if (params.busy_loop) {
    // Do some useless work, and prevent any weird compiler optimization from
    // doing anything here.
    base::TimeTicks start = base::TimeTicks::Now();
    std::vector<base::TimeTicks> dummy;
    while (base::TimeTicks::Now() - start < params.jank_duration) {
      dummy.push_back(base::TimeTicks::Now());
      if (dummy.size() > 100) {
        dummy.erase(dummy.begin());
      }
    }
    base::debug::Alias(&dummy);
  } else {
    base::PlatformThread::Sleep(params.jank_duration);
  }
}

}  // namespace

JankInjector::JankInjector() {
  if (IsJankInjectionEnabled()) {
    config_.target_dropped_frames_percent =
        base::GetFieldTrialParamByFeatureAsInt(
            features::kJankInjectionAblationFeature,
            kJankInjectionTargetPercent, config_.target_dropped_frames_percent);
    config_.dropped_frame_cluster_size = base::GetFieldTrialParamByFeatureAsInt(
        features::kJankInjectionAblationFeature, kJankInjectionClusterSize,
        config_.dropped_frame_cluster_size);
  }
}

JankInjector::~JankInjector() = default;

bool JankInjector::IsEnabled(const GURL& url) {
  return IsJankInjectionEnabled() && IsJankInjectionEnabledForURL(url);
}

void JankInjector::ScheduleJankIfNeeded(
    const viz::BeginFrameArgs& args,
    base::SingleThreadTaskRunner* task_runner) {
  if (ShouldJankCurrentFrame(args)) {
    ScheduleJank(args, task_runner);
  } else {
    // TODO(sad): update internal state.
  }
}

bool JankInjector::ShouldJankCurrentFrame(
    const viz::BeginFrameArgs& args) const {
  // TODO(sad): We can do something more complicated here if/when needed. Things
  // to take into consideration:
  //   . number of consecutive dropped frames.
  //   . max number of dropped frames in a specific interval.
  //   . min number of dropped frames in a specific interval.
  //   . e
  //   . t
  //   . c
  return false;
}

void JankInjector::ScheduleJank(const viz::BeginFrameArgs& args,
                                base::SingleThreadTaskRunner* task_runner) {
  JankInjectionParams params;
  params.jank_duration = config_.dropped_frame_cluster_size * args.interval;
  params.busy_loop = true;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&RunJank, std::move(params)));
}

}  // namespace cc
