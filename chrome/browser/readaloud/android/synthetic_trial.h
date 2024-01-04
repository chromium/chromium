// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_ANDROID_SYNTHETIC_TRIAL_H_
#define CHROME_BROWSER_READALOUD_ANDROID_SYNTHETIC_TRIAL_H_

#include <memory>
#include <string>
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "components/prefs/pref_service.h"

namespace readaloud {

/**
 * This class registers a synthetic field trial based on an ordinary FieldTrial
 * to allow custom activation criteria.
 */
class SyntheticTrial {
 public:
  /**
   * Remove any reactivation signals for trials whose base trials have changed.
   * Feature list must be initialized before calling.
   */
  static void ClearStalePrefs();

  /**
   * Create a SyntheticTrial. Feature list should be initialized before calling.
   *
   * Can't be used with features or trials whose names include "|||" as this is
   * used internally as a separator.
   *
   * Reactivates the synthetic trial associated with `feature_name` if the
   * current base trial matches what was stored when the synthetic trial was
   * previously activated.
   * @param feature_name Feature controlled by the base trial, used to retrieve
   *                     base trial name and group name.
   * @param trial_suffix String to append to the synthetic trial name to
   *                     distinguish it from the base trial. Must be non-empty.
   */
  static std::unique_ptr<SyntheticTrial> Create(
      const std::string& feature_name,
      const std::string& trial_suffix);

  SyntheticTrial(const SyntheticTrial& other) = delete;
  SyntheticTrial& operator=(const SyntheticTrial& other) = delete;

  /**
   * Perform first-time registration of the trial.
   *
   * If the base trial controlling the feature specified by
   * `target_feature_name` has trial name "BaseTrial" and group name
   * "BaseGroup", the synthetic trial will be named "BaseTrial"+`trial_suffix`
   * and it will use the same group name "BaseGroup".
   *
   * This function sets a pref so that the trial will be reactivated in future
   * sessions.
   *
   * This method only registers the trial the first time it is called.
   * Subsequent calls have no effect.
   */
  void Activate();

 private:
  friend std::unique_ptr<SyntheticTrial> std::make_unique<SyntheticTrial>(
      const std::string&,
      const std::string&,
      base::FieldTrial*&);

  SyntheticTrial(const std::string& feature_name,
                 const std::string& trial_suffix,
                 base::FieldTrial* trial);
  std::string pref_key() const;

  PrefService* prefs();

  const std::string feature_name_;
  const std::string suffix_;
  bool synthetic_trial_active_ = false;

  // Owned by base::FieldTrialList singleton which lives for the lifetime of the
  // browser process.
  raw_ptr<base::FieldTrial> base_trial_;
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_ANDROID_SYNTHETIC_TRIAL_H_
