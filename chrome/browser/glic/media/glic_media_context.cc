// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace glic {

DOCUMENT_USER_DATA_KEY_IMPL(GlicMediaContext);

GlicMediaContext::GlicMediaContext(content::RenderFrameHost* frame)
    : DocumentUserData(frame) {}

GlicMediaContext::~GlicMediaContext() = default;

bool GlicMediaContext::OnResult(const media::SpeechRecognitionResult& result) {
  if (IsExcludedFromTranscript()) {
    return false;
  }

  // Nonfinal chunks get stored separately, and have no timing information.  It
  // will be inserted at the right place in `GetContext`.
  if (!result.is_final) {
    most_recent_nonfinal_chunk_ = {result.transcription, {}};
    return true;
  }

  // Discard results that have multiple media timestamps.  These happen around
  // seeks, but we can't attribute them to the right place in the transcript.
  // Since it's a corner case, just discard.
  std::optional<media::MediaTimestampRange> media_timestamp_range;
  size_t timestamp_count = 0;
  if (result.timing_information &&
      result.timing_information->originating_media_timestamps) {
    timestamp_count =
        result.timing_information->originating_media_timestamps->size();
  }
  base::UmaHistogramExactLinear("Glic.Media.TimestampRangeCount",
                                timestamp_count, 10);

  if (timestamp_count > 1) {
    // Continue transcribing, but discard this particular result.
    return true;
  } else if (timestamp_count == 1) {
    // We'll copy this one to the `TranscriptChunk`.
    media_timestamp_range.emplace(
        (*result.timing_information->originating_media_timestamps)[0]);
  }

  // Process final result.
  TranscriptChunk new_chunk = {result.transcription, media_timestamp_range};
  new_chunk.sequence_number = next_sequence_number_++;

  if (new_chunk.HasMediaTimestamps()) {
    // New chunk has timing information, process overlaps by removing existing
    // overlapping chunks.
    RemoveOverlappingChunks(new_chunk);

    // Insert the new chunk into the updated list, maintaining order by start
    // time.  This is the place before which the chunk will be inserted, so
    // setting it equal to end() will append it to the list.
    std::optional<std::list<TranscriptChunk>::iterator> insert_pos;

    // Optimization: check if we can insert after the last insertion point.
    if (last_insertion_it_ != final_transcript_chunks_.end()) {
      if (new_chunk.GetStartTime() >= last_insertion_it_->GetStartTime()) {
        // The new chunk does come after the previous chunk.  Make sure that the
        // next chunk comes after, or there's no next chunk.
        auto next_it = std::next(last_insertion_it_);
        if (next_it == final_transcript_chunks_.end() ||
            new_chunk.GetStartTime() < next_it->GetStartTime()) {
          // Insert immediately before this.
          insert_pos = next_it;
        }
      }
    }

    // If the optimization didn't work, find the correct position.
    if (!insert_pos) {
      insert_pos = std::upper_bound(
          final_transcript_chunks_.begin(), final_transcript_chunks_.end(),
          new_chunk, [](const TranscriptChunk& a, const TranscriptChunk& b) {
            return a.GetStartTime() < b.GetStartTime();
          });
    }
    last_insertion_it_ =
        final_transcript_chunks_.insert(*insert_pos, std::move(new_chunk));
  } else {
    // New chunk has no timing information, just append it.
    final_transcript_chunks_.push_back(std::move(new_chunk));
    last_insertion_it_ = std::prev(final_transcript_chunks_.end());
  }

  // Clear the most recent non-final result after a final result is processed.
  most_recent_nonfinal_chunk_.reset();

  // Trim `final_transcript_chunks_` to a reasonable size.
  constexpr size_t kMaxTranscriptLength = 1000000;
  size_t total_size = 0;
  for (const auto& chunk : final_transcript_chunks_) {
    total_size += chunk.text.length();
  }

  while (total_size > kMaxTranscriptLength) {
    auto oldest_chunk_it = std::min_element(
        final_transcript_chunks_.begin(), final_transcript_chunks_.end(),
        [](const TranscriptChunk& a, const TranscriptChunk& b) {
          return a.sequence_number < b.sequence_number;
        });
    if (oldest_chunk_it == final_transcript_chunks_.end()) {
      // This should not be reached if `total_size` is greater than zero.
      break;
    }
    total_size -= oldest_chunk_it->text.length();
    // If we're about to remove the chunk that was also the append point,
    // start over.  This should be unlikely; unless there's ~one really big
    // chunk, we're not appending after the oldest chunk.
    if (last_insertion_it_ == oldest_chunk_it) {
      last_insertion_it_ = final_transcript_chunks_.end();
    }
    final_transcript_chunks_.erase(oldest_chunk_it);
  }

  return true;
}

std::string GlicMediaContext::GetContext() const {
  if (IsExcludedFromTranscript()) {
    return "";
  }

  // If there are no final chunks, the transcript is either empty or just the
  // non-final chunk.
  if (final_transcript_chunks_.empty()) {
    return most_recent_nonfinal_chunk_ ? most_recent_nonfinal_chunk_->text : "";
  }

  std::vector<std::string_view> pieces;
  pieces.reserve(final_transcript_chunks_.size() + 1);

  // If `last_insertion_it_` is invalid, it's ambiguous where the non-final
  // chunk should go, so we omit it.
  if (last_insertion_it_ == final_transcript_chunks_.end()) {
    for (const auto& chunk : final_transcript_chunks_) {
      pieces.push_back(chunk.text);
    }
    return base::JoinString(pieces, "");
  }

  // Otherwise, insert the non-final chunk immediately after the chunk that
  // `last_insertion_it_` points to.  Assume that it will be the next in-order
  // chunk, which is usually correct.
  for (auto it = final_transcript_chunks_.begin();
       it != final_transcript_chunks_.end(); ++it) {
    pieces.push_back(it->text);
    if (it == last_insertion_it_ && most_recent_nonfinal_chunk_) {
      pieces.push_back(most_recent_nonfinal_chunk_->text);
    }
  }

  return base::JoinString(pieces, "");
}

void GlicMediaContext::OnPeerConnectionAdded() {
  is_excluded_from_transcript_ = true;
}

bool GlicMediaContext::IsExcludedFromTranscript() const {
  if (is_excluded_from_transcript_) {
    return true;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  is_excluded_from_transcript_ |= MediaCaptureDevicesDispatcher::GetInstance()
                                      ->GetMediaStreamCaptureIndicator()
                                      ->IsCapturingUserMedia(web_contents);

  return is_excluded_from_transcript_;
}

void GlicMediaContext::RemoveOverlappingChunks(
    const TranscriptChunk& new_chunk) {
  auto it = final_transcript_chunks_.begin();
  while (it != final_transcript_chunks_.end()) {
    if (it->HasMediaTimestamps()) {
      // Existing chunk has timing information, check for overlap.
      if (new_chunk.DoesOverlapWith(*it)) {
        // If `new_chunk` somehow overlaps with the insertion hint, forget the
        // hint and search the whole list next time.  This is very rare; it
        // requires the next chunk to overlap with the chunk we just added.
        if (last_insertion_it_ == it) {
          last_insertion_it_ = final_transcript_chunks_.end();
        }
        // Overlap, erase the current chunk and get the iterator to the next.
        it = final_transcript_chunks_.erase(it);
      } else {
        // No overlap, move to the next chunk.
        ++it;
      }
    } else {
      // Existing chunk has no timing information, keep it and move to the
      // next.
      ++it;
    }
  }
}

GlicMediaContext::TranscriptChunk::TranscriptChunk() = default;
GlicMediaContext::TranscriptChunk::TranscriptChunk(
    std::string text,
    std::optional<media::MediaTimestampRange> timestamp_range)
    : text(std::move(text)),
      media_timestamp_range(std::move(timestamp_range)) {}
GlicMediaContext::TranscriptChunk::TranscriptChunk(const TranscriptChunk&) =
    default;
GlicMediaContext::TranscriptChunk& GlicMediaContext::TranscriptChunk::operator=(
    const TranscriptChunk&) = default;
GlicMediaContext::TranscriptChunk::~TranscriptChunk() = default;

base::TimeDelta GlicMediaContext::TranscriptChunk::GetStartTime() const {
  // Return a large value if no timing info, so these chunks sort last.
  return media_timestamp_range.has_value() ? media_timestamp_range->start
                                           : base::TimeDelta::Max();
}

base::TimeDelta GlicMediaContext::TranscriptChunk::GetEndTime() const {
  // Return a small value if no timing info, so these chunks don't overlap based
  // on time.
  return media_timestamp_range.has_value() ? media_timestamp_range->end
                                           : base::TimeDelta::Min();
}

bool GlicMediaContext::TranscriptChunk::DoesOverlapWith(
    const TranscriptChunk& chunk2) const {
  if (!HasMediaTimestamps() || !chunk2.HasMediaTimestamps()) {
    // Cannot determine overlap without timing info
    return false;
  }

  base::TimeDelta chunk1_start = GetStartTime();
  base::TimeDelta chunk1_end = GetEndTime();
  base::TimeDelta chunk2_start = chunk2.GetStartTime();
  base::TimeDelta chunk2_end = chunk2.GetEndTime();

  // The end times are exclusive, so we need strict inequality.
  // Also note tht we could swap the chunks and the result wouldn't change.
  return chunk1_start < chunk2_end && chunk2_start < chunk1_end;
}

bool GlicMediaContext::TranscriptChunk::HasMediaTimestamps() const {
  return media_timestamp_range.has_value();
}

}  // namespace glic
