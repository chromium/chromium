// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include <string_view>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/site_engagement/content/site_engagement_metrics.h"
#include "media/base/media_switches.h"

const char MediaEngagementScore::kVisitsKey[] = "visits";
const char MediaEngagementScore::kMediaPlaybacksKey[] = "mediaPlaybacks";
const char MediaEngagementScore::kLastMediaPlaybackTimeKey[] =
    "lastMediaPlaybackTime";
const char MediaEngagementScore::kHasHighScoreKey[] = "hasHighScore";

const char MediaEngagementScore::kScoreMinVisitsParamName[] = "min_visits";
const char MediaEngagementScore::kHighScoreLowerThresholdParamName[] =
    "lower_threshold";
const char MediaEngagementScore::kHighScoreUpperThresholdParamName[] =
    "upper_threshold";

base::TimeDelta kScoreExpirationDuration = base::Days(90);

namespace {

const int kScoreMinVisitsParamDefault = 20;
const double kHighScoreLowerThresholdParamDefault = 0.2;
const double kHighScoreUpperThresholdParamDefault = 0.3;

base::Value::Dict GetMediaEngagementScoreDictForSettings(
    const HostContentSettingsMap* settings,
    const url::Origin& origin) {
  if (!settings)
    return base::Value::Dict();

  base::Value value = settings->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
      nullptr);
  if (value.is_dict())
    return std::move(value).TakeDict();

  return base::Value::Dict();
}

void GetIntegerFromScore(const base::Value::Dict& dict,
                         std::string_view key,
                         int* out) {
  if (std::optional<int> v = dict.FindInt(key)) {
    *out = v.value();
  }
}

}  // namespace

// static.
double MediaEngagementScore::GetHighScoreLowerThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaEngagementBypassAutoplayPolicies,
      kHighScoreLowerThresholdParamName, kHighScoreLowerThresholdParamDefault);
}

// static.
double MediaEngagementScore::GetHighScoreUpperThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaEngagementBypassAutoplayPolicies,
      kHighScoreUpperThresholdParamName, kHighScoreUpperThresholdParamDefault);
}

// static.
int MediaEngagementScore::GetScoreMinVisits() {
  return base::GetFieldTrialParamByFeatureAsInt(
      media::kMediaEngagementBypassAutoplayPolicies, kScoreMinVisitsParamName,
      kScoreMinVisitsParamDefault);
}

MediaEngagementScore::MediaEngagementScore(base::Clock* clock,
                                           const url::Origin& origin,
                                           HostContentSettingsMap* settings)
    : MediaEngagementScore(
          clock,
          origin,
          GetMediaEngagementScoreDictForSettings(settings, origin),
          settings) {}

MediaEngagementScore::MediaEngagementScore(base::Clock* clock,
                                           const url::Origin& origin,
                                           base::Value::Dict score_dict,
                                           HostContentSettingsMap* settings)
    : origin_(origin),
      clock_(clock),
      score_dict_(std::move(score_dict)),
      settings_map_(settings) {
  // This is to prevent using previously saved data to mark an HTTP website as
  // allowed to autoplay.
  if (base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly) &&
      origin_.scheme() != url::kHttpsScheme) {
    return;
  }

  GetIntegerFromScore(score_dict_, kVisitsKey, &visits_);
  GetIntegerFromScore(score_dict_, kMediaPlaybacksKey, &media_playbacks_);

  if (std::optional<bool> has_high_score =
          score_dict_.FindBool(kHasHighScoreKey)) {
    is_high_ = has_high_score.value();
  }

  if (std::optional<double> last_time =
          score_dict_.FindDouble(kLastMediaPlaybackTimeKey)) {
    last_media_playback_time_ =
        base::Time::FromInternalValue(last_time.value());
  }

  // Recalculate the total score and high bit. If the high bit changed we
  // should commit this. This should only happen if we change the threshold
  // or if we have data from before the bit was introduced.
  bool was_high = is_high_;
  Recalculate();
  bool needs_commit = is_high_ != was_high;

  // If we need to commit because of a migration and we have the settings map
  // then we should commit.
  if (needs_commit && settings_map_)
    Commit();
}

// TODO(beccahughes): Add typemap.
media::mojom::MediaEngagementScoreDetailsPtr
MediaEngagementScore::GetScoreDetails() const {
  return media::mojom::MediaEngagementScoreDetails::New(
      origin_, actual_score(), visits(), media_playbacks(),
      last_media_playback_time().InMillisecondsFSinceUnixEpoch(), high_score());
}

MediaEngagementScore::~MediaEngagementScore() = default;

MediaEngagementScore::MediaEngagementScore(MediaEngagementScore&&) = default;
MediaEngagementScore& MediaEngagementScore::operator=(MediaEngagementScore&&) =
    default;

void MediaEngagementScore::Commit(bool force_update) {
  DCHECK(settings_map_);

  if (origin_.opaque())
    return;

  if (!UpdateScoreDict(force_update))
    return;

  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(kScoreExpirationDuration);
  settings_map_->SetWebsiteSettingDefaultScope(
      origin_.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
      base::Value(std::move(score_dict_)), constraints);
  score_dict_.clear();
}

void MediaEngagementScore::IncrementMediaPlaybacks() {
  SetMediaPlaybacks(media_playbacks() + 1);
  last_media_playback_time_ = clock_->Now();
}

bool MediaEngagementScore::UpdateScoreDict(bool force_update) {
  int stored_visits = 0;
  int stored_media_playbacks = 0;
  double stored_last_media_playback_internal = 0;
  bool is_high = false;

  // This is to prevent saving data that we would otherwise not use.
  if (base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly) &&
      origin_.scheme() != url::kHttpsScheme) {
    return false;
  }

  if (std::optional<bool> has_high_score =
          score_dict_.FindBool(kHasHighScoreKey)) {
    is_high = has_high_score.value();
  }

  if (std::optional<double> last_time =
          score_dict_.FindDouble(kLastMediaPlaybackTimeKey)) {
    stored_last_media_playback_internal = last_time.value();
  }

  GetIntegerFromScore(score_dict_, kVisitsKey, &stored_visits);
  GetIntegerFromScore(score_dict_, kMediaPlaybacksKey, &stored_media_playbacks);

  bool changed = stored_visits != visits() ||
                 stored_media_playbacks != media_playbacks() ||
                 is_high_ != is_high ||
                 stored_last_media_playback_internal !=
                     last_media_playback_time_.ToInternalValue();

  if (!changed && !force_update)
    return false;

  score_dict_.Set(kVisitsKey, visits_);
  score_dict_.Set(kMediaPlaybacksKey, media_playbacks_);
  score_dict_.Set(kLastMediaPlaybackTimeKey,
                  double(last_media_playback_time_.ToInternalValue()));
  score_dict_.Set(kHasHighScoreKey, is_high_);

  // visitsWithMediaTag was deprecated in https://crbug.com/998687 and should
  // be removed if we see it in |score_dict_|.
  score_dict_.Remove("visitsWithMediaTag");

  // These keys were deprecated in https://crbug.com/998892 and should be
  // removed if we see it in |score_dict_|.
  score_dict_.Remove("audiblePlaybacks");
  score_dict_.Remove("significantPlaybacks");
  score_dict_.Remove("highScoreChanges");
  score_dict_.Remove("mediaElementPlaybacks");
  score_dict_.Remove("audioContextPlaybacks");

  return true;
}

void MediaEngagementScore::Recalculate() {
  // Use the minimum visits to compute the score to allow websites that would
  // surely have a high MEI to pass the bar early.
  double effective_visits = std::max(visits(), GetScoreMinVisits());
  actual_score_ = static_cast<double>(media_playbacks()) / effective_visits;

  // Recalculate whether the engagement score is considered high.
  if (is_high_) {
    is_high_ = actual_score_ >= GetHighScoreLowerThreshold();
  } else {
    is_high_ = actual_score_ >= GetHighScoreUpperThreshold();
  }
}

void MediaEngagementScore::SetVisits(int visits) {
  visits_ = visits;
  Recalculate();
}

void MediaEngagementScore::SetMediaPlaybacks(int media_playbacks) {
  media_playbacks_ = media_playbacks;
  Recalculate();
}
