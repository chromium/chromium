// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_ENVELOPE_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_ENVELOPE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"

namespace ttc {

// Represents a discrete acoustic syllable or pause segment extracted from
// 20ms analysis slices of 24kHz 16-bit mono PCM audio output.
struct AudioEnvelopeSegment {
  // Offset from the beginning of the current model turn.
  base::TimeDelta start_time;

  // Duration of this segment.
  base::TimeDelta duration;

  // Normalized RMS acoustic energy in [0.0, 1.0].
  float energy_rms = 0.0f;

  // Normalized spectral flux (onset velocity) in [0.0, 1.0].
  float spectral_flux = 0.0f;

  // Normalized peak sample amplitude in [0.0, 1.0].
  float peak_amplitude = 0.0f;

  // Normalized zero-crossing rate in [0.0, 1.0].
  float zero_crossing_rate = 0.0f;

  // 3-band formant energy distribution [low, mid, high], each in [0.0, 1.0]:
  // - band_energies[0] (Low): Vowel / fundamental resonance (O/U)
  // - band_energies[1] (Mid): Open vowel / vocal tract formants (AH/EH)
  // - band_energies[2] (High): Sibilance / fricative consonants (EE/S/T)
  std::array<float, 3> band_energies = {0.0f, 0.0f, 0.0f};
};

// Acoustic envelope summary and lookahead syllable segments for a chunk of
// model audio output. When the model streams a PCM audio chunk to
// `AudioController::PlayAudio()`, `AudioEnvelopeExtractor` pre-analyzes the
// chunk before hardware playback finishes so the visualizer UI can schedule
// syllable-level energy pulses and formant-driven shape transitions in sync
// with the spoken audio.
struct AudioEnvelope {
  AudioEnvelope();
  AudioEnvelope(const AudioEnvelope&);
  AudioEnvelope& operator=(const AudioEnvelope&);
  AudioEnvelope(AudioEnvelope&&) noexcept;
  AudioEnvelope& operator=(AudioEnvelope&&) noexcept;
  ~AudioEnvelope();

  // Summary metrics across the audio chunk (taken at peak energy slice).
  float rms = 0.0f;
  float zero_crossing_rate = 0.0f;
  float peak_amplitude = 0.0f;
  float spectral_flux = 0.0f;
  std::array<float, 3> band_energies = {0.0f, 0.0f, 0.0f};

  // Aggregated syllable and pause lookahead segments.
  std::vector<AudioEnvelopeSegment> segments;
};

// Stateful extractor that partitions incoming 24kHz 16-bit mono PCM model
// output chunks into 20ms analysis slices, computes RMS energy, spectral flux,
// and 3-band formant ratios, and groups contiguous voiced/silent slices into
// timestamped `AudioEnvelopeSegment`s across a model turn.
class AudioEnvelopeExtractor {
 public:
  static constexpr size_t kSampleRate = 24000;
  static constexpr size_t kSliceDurationMs = 20;
  static constexpr size_t kFramesPerSlice =
      kSampleRate * kSliceDurationMs / 1000;  // 480

  AudioEnvelopeExtractor() = default;
  ~AudioEnvelopeExtractor() = default;
  AudioEnvelopeExtractor(const AudioEnvelopeExtractor&) = delete;
  AudioEnvelopeExtractor& operator=(const AudioEnvelopeExtractor&) = delete;

  // Resets sample counters at the start or end of a model turn.
  void Reset();

  // Extracts an `AudioEnvelope` from `audio_data` and advances the turn sample
  // offset.
  AudioEnvelope Extract(base::span<const int16_t> audio_data);

  size_t total_audio_output_samples() const {
    return total_audio_output_samples_;
  }

 private:
  size_t total_audio_output_samples_ = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_AUDIO_ENVELOPE_H_
