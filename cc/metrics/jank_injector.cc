// Copyright 2021 The Chromium Authors
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
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "url/gurl.h"

namespace cc {

namespace {

constexpr char kTraceCategory[] =
    "cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");

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

bool g_jank_enabled_for_test = false;

bool IsJankInjectionEnabled() {
  static bool enabled =
      base::FeatureList::IsEnabled(features::kJankInjectionAblationFeature);
  return enabled || g_jank_enabled_for_test;
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
  return base::ranges::any_of(paths, [path](const std::string& p) {
    return base::StartsWith(path, p);
  });
}

void RunJank(JankInjectionParams params) {
  TRACE_EVENT0(kTraceCategory, "Injected Jank");
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

ScopedJankInjectionEnabler::ScopedJankInjectionEnabler() {
  DCHECK(!g_jank_enabled_for_test);
  g_jank_enabled_for_test = true;
}

ScopedJankInjectionEnabler::~ScopedJankInjectionEnabler() {
  DCHECK(g_jank_enabled_for_test);
  g_jank_enabled_for_test = false;
}

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
    did_jank_last_time_ = true;
  } else {
    ++total_frames_;
    did_jank_last_time_ = false;
  }
}

bool JankInjector::ShouldJankCurrentFrame(
    const viz::BeginFrameArgs& args) const {
  // If jank was injected during the previous frame, then do not inject jank
  // again now.
  if (did_jank_last_time_)
    return false;

  // Do not jank during the first frame.
  if (!total_frames_)
    return false;

  auto current_jank = janked_frames_ * 100 / total_frames_;
  // Do not drop any more frames if the injected jank is already above or at the
  // target.
  if (current_jank >= config_.target_dropped_frames_percent)
    return false;

  // If janking now makes the dropped the frames goes beyond the target, then do
  // not inject the jank yet.
  auto next_jank = (janked_frames_ + config_.dropped_frame_cluster_size) * 100 /
                   (total_frames_ + config_.dropped_frame_cluster_size);
  if (next_jank > config_.target_dropped_frames_percent)
    return false;

  return true;
}

void JankInjector::ScheduleJank(const viz::BeginFrameArgs& args,
                                base::SingleThreadTaskRunner* task_runner) {
  JankInjectionParams params;
  params.jank_duration = config_.dropped_frame_cluster_size * args.interval;
  params.busy_loop = true;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&RunJank, std::move(params)));

  janked_frames_ += config_.dropped_frame_cluster_size;
  total_frames_ += config_.dropped_frame_cluster_size;
}

}  // namespace cc
