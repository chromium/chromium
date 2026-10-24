// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/public/audio_envelope.h"

#include <algorithm>
#include <cmath>

namespace ttc {

namespace {

constexpr float kSilenceThreshold = 0.035f;
constexpr size_t kMinSyllableFrames = 5;
constexpr size_t kMaxSyllableFrames = 11;
constexpr float kLowBandZcrCutoff = 0.12f;
constexpr float kMidBandZcrCenter = 0.12f;
constexpr float kMidBandZcrHalfWidth = 0.08f;
constexpr float kHighBandZcrFloor = 0.10f;
constexpr float kHighBandZcrSpan = 0.15f;
constexpr float kSyllableOnsetFluxThreshold = 0.15f;

struct SliceFeatures {
  size_t start_sample = 0;
  size_t sample_count = 0;
  float rms = 0.0f;
  float zcr = 0.0f;
  float peak = 0.0f;
  float flux = 0.0f;
  float low = 0.0f;
  float mid = 0.0f;
  float high = 0.0f;
  bool is_speech = false;
};

}  // namespace

AudioEnvelope::AudioEnvelope() = default;
AudioEnvelope::AudioEnvelope(const AudioEnvelope&) = default;
AudioEnvelope& AudioEnvelope::operator=(const AudioEnvelope&) = default;
AudioEnvelope::AudioEnvelope(AudioEnvelope&&) noexcept = default;
AudioEnvelope& AudioEnvelope::operator=(AudioEnvelope&&) noexcept = default;
AudioEnvelope::~AudioEnvelope() = default;

void AudioEnvelopeExtractor::Reset() {
  total_audio_output_samples_ = 0;
}

AudioEnvelope AudioEnvelopeExtractor::Extract(
    base::span<const int16_t> audio_data) {
  AudioEnvelope envelope;
  if (audio_data.empty()) {
    return envelope;
  }

  std::vector<SliceFeatures> slice_features;
  const size_t total_samples = audio_data.size();
  float prev_rms = 0.0f;

  for (size_t offset = 0; offset < total_samples; offset += kFramesPerSlice) {
    const size_t count = std::min(kFramesPerSlice, total_samples - offset);
    float sum_sq = 0.0f;
    float peak = 0.0f;
    size_t zcr_count = 0;

    size_t zcr_pairs = 0;

    for (size_t i = 0; i < count; ++i) {
      const float s = audio_data[offset + i] / 32768.0f;
      sum_sq += s * s;
      peak = std::max(peak, std::abs(s));
      if (offset + i > 0) {
        ++zcr_pairs;
        if ((audio_data[offset + i] >= 0) !=
            (audio_data[offset + i - 1] >= 0)) {
          ++zcr_count;
        }
      }
    }

    const float rms = std::sqrt(sum_sq / count);
    const float zcr =
        zcr_pairs > 0 ? static_cast<float>(zcr_count) / zcr_pairs : 0.0f;
    const float flux = std::abs(rms - prev_rms);
    prev_rms = rms;

    // Zero Crossing Rate formant mapping:
    // Low ZCR (<0.08) -> deep vowel resonance (O/U)
    // High ZCR (>0.15) -> sibilance / fricative consonants (EE/S/T)
    // Mid ZCR (~0.12) -> open vowels (AH/EH)
    const float low_band =
        rms * std::clamp(1.0f - zcr / kLowBandZcrCutoff, 0.0f, 1.0f);
    const float high_band =
        rms *
        std::clamp((zcr - kHighBandZcrFloor) / kHighBandZcrSpan, 0.0f, 1.0f);
    const float mid_band =
        rms * std::clamp(1.0f - std::abs(zcr - kMidBandZcrCenter) /
                                    kMidBandZcrHalfWidth,
                         0.0f, 1.0f);

    slice_features.push_back({
        .start_sample = offset,
        .sample_count = count,
        .rms = rms,
        .zcr = zcr,
        .peak = peak,
        .flux = flux,
        .low = low_band,
        .mid = mid_band,
        .high = high_band,
        .is_speech = (rms >= kSilenceThreshold),
    });
  }

  size_t overall_peak_idx = 0;
  for (size_t idx = 1; idx < slice_features.size(); ++idx) {
    if (slice_features[idx].rms > slice_features[overall_peak_idx].rms) {
      overall_peak_idx = idx;
    }
  }
  const auto& overall_peak = slice_features[overall_peak_idx];
  envelope.rms = std::clamp(overall_peak.rms, 0.0f, 1.0f);
  envelope.zero_crossing_rate = std::clamp(overall_peak.zcr, 0.0f, 1.0f);
  envelope.peak_amplitude = std::clamp(overall_peak.peak, 0.0f, 1.0f);
  envelope.spectral_flux = std::clamp(overall_peak.flux, 0.0f, 1.0f);
  envelope.band_energies = {std::clamp(overall_peak.low, 0.0f, 1.0f),
                            std::clamp(overall_peak.mid, 0.0f, 1.0f),
                            std::clamp(overall_peak.high, 0.0f, 1.0f)};

  // Aggregate 20ms analysis slices into discrete syllable or pause segments.
  size_t i = 0;
  while (i < slice_features.size()) {
    const bool is_speech = slice_features[i].is_speech;
    const size_t start_idx = i;
    float peak_rms = slice_features[i].rms;
    float peak_flux = slice_features[i].flux;
    size_t peak_idx = i;

    if (!is_speech) {
      while (i < slice_features.size() && !slice_features[i].is_speech) {
        if (slice_features[i].rms > peak_rms) {
          peak_rms = slice_features[i].rms;
          peak_idx = i;
        }
        ++i;
      }
    } else {
      // Group contiguous speech into a single syllable event.
      // Minimum syllable duration ~100ms (5 slices); maximum ~220ms (11
      // slices).
      while (i < slice_features.size() && slice_features[i].is_speech) {
        const size_t current_len = i - start_idx;
        if (slice_features[i].rms > peak_rms) {
          peak_rms = slice_features[i].rms;
          peak_idx = i;
        }
        if (slice_features[i].flux > peak_flux) {
          peak_flux = slice_features[i].flux;
        }

        if (current_len >= kMinSyllableFrames) {
          if (current_len >= kMaxSyllableFrames) {
            break;
          }
          // Onset of a new syllable: sharp volume rise after peak.
          if (i > peak_idx &&
              slice_features[i].flux > kSyllableOnsetFluxThreshold &&
              slice_features[i].rms > slice_features[i - 1].rms) {
            break;
          }
        }
        ++i;
      }
    }

    size_t seg_samples = 0;
    for (size_t idx = start_idx; idx < i; ++idx) {
      seg_samples += slice_features[idx].sample_count;
    }
    const int64_t seg_start_ms =
        (static_cast<int64_t>(total_audio_output_samples_ +
                              slice_features[start_idx].start_sample) *
         1000) /
        static_cast<int64_t>(kSampleRate);
    const int64_t seg_duration_ms = (static_cast<int64_t>(seg_samples) * 1000 +
                                     static_cast<int64_t>(kSampleRate / 2)) /
                                    static_cast<int64_t>(kSampleRate);

    const auto& peak_frame = slice_features[peak_idx];
    AudioEnvelopeSegment segment;
    segment.start_time = base::Milliseconds(seg_start_ms);
    segment.duration = base::Milliseconds(seg_duration_ms);
    segment.energy_rms = std::clamp(peak_frame.rms, 0.0f, 1.0f);
    segment.spectral_flux = std::clamp(peak_flux, 0.0f, 1.0f);
    segment.peak_amplitude = std::clamp(peak_frame.peak, 0.0f, 1.0f);
    segment.zero_crossing_rate = std::clamp(peak_frame.zcr, 0.0f, 1.0f);
    segment.band_energies = {std::clamp(peak_frame.low, 0.0f, 1.0f),
                             std::clamp(peak_frame.mid, 0.0f, 1.0f),
                             std::clamp(peak_frame.high, 0.0f, 1.0f)};
    envelope.segments.push_back(segment);
  }

  total_audio_output_samples_ += total_samples;
  return envelope;
}

}  // namespace ttc
