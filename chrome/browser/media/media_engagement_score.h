// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/origin.h"

class HostContentSettingsMap;

// Calculates and stores the Media Engagement Index score for a certain origin.
class MediaEngagementScore final {
 public:
  // The dictionary keys to store individual metrics. kVisitsKey will store the
  // number of visits to an origin and kMediaPlaybacksKey will store the number
  // of media playbacks on an origin. kLastMediaPlaybackTimeKey will store the
  // timestamp of the last media playback on an origin. kHasHighScoreKey will
  // store whether the score is considered high.
  static const char kVisitsKey[];
  static const char kMediaPlaybacksKey[];
  static const char kLastMediaPlaybackTimeKey[];
  static const char kHasHighScoreKey[];

  // Origins with a number of visits less than this number will recieve
  // a score of zero.
  static int GetScoreMinVisits();

  // The upper and lower threshold of whether the total score is considered
  // to be high.
  static double GetHighScoreLowerThreshold();
  static double GetHighScoreUpperThreshold();

  MediaEngagementScore(base::Clock* clock,
                       const url::Origin& origin,
                       HostContentSettingsMap* settings);

  MediaEngagementScore(const MediaEngagementScore&) = delete;
  MediaEngagementScore& operator=(const MediaEngagementScore&) = delete;

  ~MediaEngagementScore();

  MediaEngagementScore(MediaEngagementScore&&);
  MediaEngagementScore& operator=(MediaEngagementScore&&);

  // Returns the total score, as per the formula.
  double actual_score() const { return actual_score_; }

  // Returns whether the total score is considered high.
  bool high_score() const { return is_high_; }

  // Returns the origin associated with this score.
  const url::Origin& origin() const { return origin_; }

  // Writes the values in this score into |settings_map_|. If there are multiple
  // instances of a score object for an origin, this could result in stale data
  // being stored. Takes in a boolean indicating whether to force an update
  // even if properties of the score are unchanged.
  void Commit(bool force_update = false);

  // Get/increment the number of visits this origin has had.
  int visits() const { return visits_; }
  void IncrementVisits() { SetVisits(visits() + 1); }

  // Get/increment the number of media playbacks this origin has had.
  int media_playbacks() const { return media_playbacks_; }
  void IncrementMediaPlaybacks();

  // Gets/sets the last time media was played on this origin.
  base::Time last_media_playback_time() const {
    return last_media_playback_time_;
  }
  void set_last_media_playback_time(base::Time new_time) {
    last_media_playback_time_ = new_time;
  }

  // Get a breakdown of the score that can be serialized by Mojo.
  media::mojom::MediaEngagementScoreDetailsPtr GetScoreDetails() const;

 protected:
  friend class MediaEngagementAutoplayBrowserTest;
  friend class MediaEngagementContentsObserverTest;
  friend class MediaEngagementContentsObserverMPArchBrowserTest;
  friend class MediaEngagementSessionTest;
  friend class MediaEngagementService;

  // Only used by the Media Engagement service when bulk loading data.
  MediaEngagementScore(base::Clock* clock,
                       const url::Origin& origin,
                       base::Value::Dict score_dict,
                       HostContentSettingsMap* settings);

  static const char kScoreMinVisitsParamName[];
  static const char kHighScoreLowerThresholdParamName[];
  static const char kHighScoreUpperThresholdParamName[];

  void SetVisits(int visits);
  void SetMediaPlaybacks(int media_playbacks);

 private:
  friend class MediaEngagementServiceTest;
  friend class MediaEngagementScoreTest;
  friend class MediaEngagementScoreWithOverrideFieldTrialsTest;

  // Update the dictionary continaing the latest score values and return whether
  // they have changed or not (since what was last retrieved from content
  // settings). Takes in a boolean indicating whether to force an update
  // even if properties of the score are unchanged.
  bool UpdateScoreDict(bool force_update = false);

  // If the number of playbacks or visits is updated then this will recalculate
  // the total score and whether the score is considered high.
  void Recalculate();

  // The number of media playbacks this origin has had.
  int media_playbacks_ = 0;

  // The number of visits this origin has had.
  int visits_ = 0;

  // If the current score is considered high.
  bool is_high_ = false;

  // The current engagement score.
  double actual_score_ = 0.0;

  // The last time media was played back on this origin.
  base::Time last_media_playback_time_;

  // The origin this score represents.
  url::Origin origin_;

  // A clock that can be used for testing, owned by the service.
  raw_ptr<base::Clock> clock_;

  // The dictionary that represents this engagement score.
  base::Value::Dict score_dict_;

  // The content settings map that will persist the score,
  // has a lifetime of the Profile like the service which owns |this|.
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SCORE_H_
