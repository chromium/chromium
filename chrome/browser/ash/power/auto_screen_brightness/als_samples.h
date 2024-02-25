// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_SAMPLES_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_SAMPLES_H_

#include <deque>
#include <optional>

#include "base/time/time.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

struct AlsAvgStdDev {
  double avg = 0;
  double stddev = 0;
};

// AmbientLightSampleBuffer stores most recent ambient light samples, with
// horizon defined in its ctor.
class AmbientLightSampleBuffer {
 public:
  struct Sample {
    double lux;
    base::TimeTicks sample_time;
  };

  // Constructs a buffer that keeps samples younger than |horizon|. |horizon|
  // should be greater than 0.
  explicit AmbientLightSampleBuffer(base::TimeDelta horizon);

  AmbientLightSampleBuffer(const AmbientLightSampleBuffer&) = delete;
  AmbientLightSampleBuffer& operator=(const AmbientLightSampleBuffer&) = delete;

  ~AmbientLightSampleBuffer();

  // Adds |sample| to the buffer and discards samples that are now too old.
  // |sample| must be later than any previously added sample.
  void SaveToBuffer(const Sample& sample);

  // Clears out all the samples in the buffer.
  void ClearBuffer();

  // Returns average and std-dev of ambient lux from the buffer (discarding
  // samples that are now too old). |now| must be no earlier than any previously
  // added sample. If there are no valid samples, returns nullopt.
  std::optional<AlsAvgStdDev> AverageAmbientWithStdDev(base::TimeTicks now);

  // Returns the number of recorded samples within |horizon| of the last
  // observed time point. |now| must be no earlier than any previously added
  // sample, and this function will discard old examples.
  size_t NumberOfSamples(base::TimeTicks now);

  // Same as |NumberOfSamples| but without pruning.
  size_t NumberOfSamplesForTesting() const;

 private:
  const base::TimeDelta horizon_;
  std::deque<Sample> samples_;

  // Removes samples from |samples_| that have time <= |now| - |horizon_|.
  void Prune(base::TimeTicks now);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_ALS_SAMPLES_H_
