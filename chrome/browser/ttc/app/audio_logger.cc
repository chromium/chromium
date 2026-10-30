// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_logger.h"

#include "base/byte_size.h"
#include "base/containers/span_writer.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/session_journal.h"
#include "media/base/audio_parameters.h"

namespace ttc {

namespace {

constexpr base::ByteSize kPrerecordBufferSize = base::KiB(96);

}  // namespace

std::vector<uint8_t> EncodeWav(
    const base::circular_deque<uint8_t>& prerecord_buffer,
    base::span<const uint8_t> buffer,
    int sample_rate,
    int num_channels) {
  constexpr uint16_t kBitsPerSample = 16;
  constexpr uint16_t kBytesPerSample = kBitsPerSample / 8;
  constexpr size_t kWavHeaderSize = 44;

  const size_t total_pcm_size = prerecord_buffer.size() + buffer.size();
  const uint32_t data_size = static_cast<uint32_t>(total_pcm_size);
  const uint32_t riff_chunk_size = 36 + data_size;
  const uint16_t channels = static_cast<uint16_t>(num_channels);
  const uint32_t rate = static_cast<uint32_t>(sample_rate);
  const uint32_t byte_rate = rate * channels * kBytesPerSample;
  const uint16_t block_align = channels * kBytesPerSample;

  std::vector<uint8_t> wav_bytes(kWavHeaderSize + total_pcm_size);
  base::SpanWriter<uint8_t> writer(wav_bytes);
  writer.Write(base::byte_span_from_cstring("RIFF"));
  writer.WriteU32LittleEndian(riff_chunk_size);
  writer.Write(base::byte_span_from_cstring("WAVE"));
  writer.Write(base::byte_span_from_cstring("fmt "));
  writer.WriteU32LittleEndian(16u);  // PCM fmt chunk size
  writer.WriteU16LittleEndian(1u);   // AudioFormat = 1 (PCM)
  writer.WriteU16LittleEndian(channels);
  writer.WriteU32LittleEndian(rate);
  writer.WriteU32LittleEndian(byte_rate);
  writer.WriteU16LittleEndian(block_align);
  writer.WriteU16LittleEndian(kBitsPerSample);
  writer.Write(base::byte_span_from_cstring("data"));
  writer.WriteU32LittleEndian(data_size);

  auto remaining = writer.remaining_span();
  auto [prerecord_dest, buffer_dest] =
      remaining.split_at(prerecord_buffer.size());
  std::ranges::copy(prerecord_buffer, prerecord_dest.begin());
  buffer_dest.copy_from(buffer);

  return wav_bytes;
}

AudioLogger::AudioLogger() = default;

AudioLogger::~AudioLogger() = default;

void AudioLogger::OnCapturedAudio(base::span<const uint8_t> pcm_data,
                                  const media::AudioParameters& params,
                                  Speaker speaker) {
  // The kUser speaker state is updated based on detecting speech activity on
  // the server so all the audio data that triggered the detection occurs while
  // in other speaker states. Use PrerecordAudio which uses a small ring buffer
  // to continually keep ~3 seconds of audio that gets prepended to the audio
  // that's captured during kUser state.
  switch (speaker) {
    case Speaker::kIdle:
    case Speaker::kModel:
      PrerecordAudio(pcm_data, params);
      break;
    case Speaker::kUser:
      RecordAudio(pcm_data, params);
      break;
  }
}

void AudioLogger::OnCaptureStopped() {
  prerecord_buffer_.clear();
  buffer_.clear();
  sample_rate_ = 0;
  num_channels_ = 0;
}

void AudioLogger::PrerecordAudio(base::span<const uint8_t> pcm_data,
                                 const media::AudioParameters& params) {
  UpdateParams(params);
  prerecord_buffer_.insert(prerecord_buffer_.end(), pcm_data.begin(),
                           pcm_data.end());
  if (prerecord_buffer_.size() > kPrerecordBufferSize.InBytes()) {
    prerecord_buffer_.erase(
        prerecord_buffer_.begin(),
        prerecord_buffer_.begin() +
            (prerecord_buffer_.size() - kPrerecordBufferSize.InBytes()));
  }
}

void AudioLogger::RecordAudio(base::span<const uint8_t> pcm_data,
                              const media::AudioParameters& params) {
  UpdateParams(params);
  buffer_.insert(buffer_.end(), pcm_data.begin(), pcm_data.end());
}

void AudioLogger::DumpToJournal(SessionJournal& journal) {
  if (params_changed_) {
    journal.Log("AudioLoggerError",
                actor::JournalDetailsBuilder()
                    .AddError("Audio parameters changed during recording; "
                              "audio partially dropped.")
                    .Build());
    params_changed_ = false;
  }

  if ((!prerecord_buffer_.empty() || !buffer_.empty()) && sample_rate_ > 0 &&
      num_channels_ > 0) {
    journal.LogAudio("audio/wav", EncodeWav(prerecord_buffer_, buffer_,
                                            sample_rate_, num_channels_));
  }
  prerecord_buffer_.clear();
  buffer_.clear();
}

void AudioLogger::UpdateParams(const media::AudioParameters& params) {
  const int new_sample_rate = params.sample_rate();
  const int new_num_channels = params.channels();
  if (sample_rate_ != 0 && num_channels_ != 0 &&
      (sample_rate_ != new_sample_rate || num_channels_ != new_num_channels)) {
    prerecord_buffer_.clear();
    buffer_.clear();
    params_changed_ = true;
  }
  sample_rate_ = new_sample_rate;
  num_channels_ = new_num_channels;
}

}  // namespace ttc
