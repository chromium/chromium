// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace glic {

DOCUMENT_USER_DATA_KEY_IMPL(GlicMediaContext);

GlicMediaContext::GlicMediaContext(content::RenderFrameHost* frame)
    : DocumentUserData(frame) {}

GlicMediaContext::~GlicMediaContext() {
  // If we got any transcript, then record its max length we saw as its total.
  // If anything is close to the cut-off, then we can infer that we likely
  // truncated it.
  for (const auto& pair : transcripts_by_title_) {
    const auto& transcript = pair.second;
    if (transcript->max_transcript_size_ > 0) {
      UMA_HISTOGRAM_COUNTS_1M("Glic.Media.TotalContextLength",
                              transcript->max_transcript_size_);
    }
  }
}

bool GlicMediaContext::OnResult(const media::SpeechRecognitionResult& result) {
  Transcript* transcript = GetOrCreateTranscript();
  if (!transcript) {
    // Do not turn off transcription here, since there's no way to re-enable it
    // later.  For example, if `IsExcludedByTranscript()` changes, then we'd be
    // stuck without transcription.
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

  if (timestamp_count > 1) {
    // Continue transcribing, but discard this particular result.
    return true;
  } else if (timestamp_count == 1) {
    // We'll copy this one to the `TranscriptChunk`.
    media_timestamp_range.emplace(
        (*result.timing_information->originating_media_timestamps)[0]);
  }

  TranscriptChunk new_chunk = {result.transcription, media_timestamp_range};

  if (!result.is_final) {
    HandleNonFinalResult(transcript, std::move(new_chunk));
  } else {
    // Record timestamp metric for final result.
    base::UmaHistogramExactLinear("Glic.Media.TimestampCount", timestamp_count,
                                  10);
    HandleFinalResult(transcript, std::move(new_chunk));
  }

  return true;
}

void GlicMediaContext::HandleNonFinalResult(Transcript* transcript,
                                            TranscriptChunk new_chunk) {
  // If a non-final chunk already exists, it must be removed before adding the
  // new one, unless it's being updated in-place.
  if (transcript->nonfinal_chunk_it_ != transcript->transcript_chunks_.end()) {
    // If the new chunk has a timestamp and its start time matches the existing
    // non-final chunk, we can update it in-place.
    if (new_chunk.HasMediaTimestamps() &&
        transcript->nonfinal_chunk_it_->HasMediaTimestamps() &&
        new_chunk.GetStartTime() ==
            transcript->nonfinal_chunk_it_->GetStartTime()) {
      transcript->nonfinal_chunk_it_->text = new_chunk.text;
      transcript->nonfinal_chunk_it_->media_timestamp_range =
          new_chunk.media_timestamp_range;
      return;
    }
    // Otherwise, the old non-final chunk is invalid.
    transcript->transcript_chunks_.erase(transcript->nonfinal_chunk_it_);
    transcript->nonfinal_chunk_it_ = transcript->transcript_chunks_.end();
  }

  // Now, insert the new non-final chunk.
  if (new_chunk.HasMediaTimestamps()) {
    // Insert in order of its start time.
    auto insert_pos = std::upper_bound(
        transcript->transcript_chunks_.begin(),
        transcript->transcript_chunks_.end(), new_chunk,
        [](const TranscriptChunk& a, const TranscriptChunk& b) {
          return a.GetStartTime() < b.GetStartTime();
        });
    transcript->nonfinal_chunk_it_ =
        transcript->transcript_chunks_.insert(insert_pos, std::move(new_chunk));
  } else {
    // A non-final chunk without a timestamp can't be sorted by time. Instead,
    // insert it right after the last final chunk.
    auto insert_pos = transcript->last_insertion_it_;
    if (insert_pos != transcript->transcript_chunks_.end()) {
      ++insert_pos;
    }
    transcript->nonfinal_chunk_it_ =
        transcript->transcript_chunks_.insert(insert_pos, std::move(new_chunk));
  }
}

void GlicMediaContext::HandleFinalResult(Transcript* transcript,
                                         TranscriptChunk new_chunk) {
  if (transcript->nonfinal_chunk_it_ != transcript->transcript_chunks_.end()) {
    // A non-final chunk exists and we will remove it so that the new final
    // chunk can be added in media time order.
    transcript->transcript_chunks_.erase(transcript->nonfinal_chunk_it_);
    transcript->nonfinal_chunk_it_ = transcript->transcript_chunks_.end();
  }

  // Process final result.
  new_chunk.sequence_number = transcript->next_sequence_number_++;

  if (new_chunk.HasMediaTimestamps()) {
    // New chunk has timing information, process overlaps by removing existing
    // overlapping chunks.
    RemoveOverlappingChunks(transcript, new_chunk);

    // Insert the new chunk into the updated list, maintaining order by start
    // time.  This is the place before which the chunk will be inserted, so
    // setting it equal to end() will append it to the list.
    std::optional<std::list<TranscriptChunk>::iterator> insert_pos;

    // Optimization: check if we can insert after the last insertion point.
    if (transcript->last_insertion_it_ !=
        transcript->transcript_chunks_.end()) {
      if (new_chunk.GetStartTime() >=
          transcript->last_insertion_it_->GetStartTime()) {
        // The new chunk does come after the previous chunk.  Make sure that the
        // next chunk comes after, or there's no next chunk.
        auto next_it = std::next(transcript->last_insertion_it_);
        if (next_it == transcript->transcript_chunks_.end() ||
            new_chunk.GetStartTime() < next_it->GetStartTime()) {
          // Insert immediately before this.
          insert_pos = next_it;
        }
      }
    }

    // If the optimization didn't work, find the correct position.
    if (!insert_pos) {
      insert_pos = std::upper_bound(
          transcript->transcript_chunks_.begin(),
          transcript->transcript_chunks_.end(), new_chunk,
          [](const TranscriptChunk& a, const TranscriptChunk& b) {
            return a.GetStartTime() < b.GetStartTime();
          });
    }
    transcript->last_insertion_it_ = transcript->transcript_chunks_.insert(
        *insert_pos, std::move(new_chunk));
  } else {
    // New chunk without a timestamp will be inserted right after the last final
    // chunk.
    auto insert_pos = transcript->last_insertion_it_;
    if (insert_pos != transcript->transcript_chunks_.end()) {
      ++insert_pos;
    }
    transcript->last_insertion_it_ =
        transcript->transcript_chunks_.insert(insert_pos, std::move(new_chunk));
  }

  TrimTranscript(transcript);
}

void GlicMediaContext::TrimTranscript(Transcript* transcript) {
  // Trim `transcript_chunks_` to a reasonable size.
  constexpr size_t kMaxTranscriptLength = 1000000;
  size_t total_size = 0;
  for (const auto& chunk : transcript->transcript_chunks_) {
    total_size += chunk.text.length();
  }

  // For metrics, record the maximum size this transcript reaches.
  if (total_size > transcript->max_transcript_size_) {
    transcript->max_transcript_size_ = total_size;
  }

  while (total_size > kMaxTranscriptLength) {
    auto oldest_chunk_it = std::min_element(
        transcript->transcript_chunks_.begin(),
        transcript->transcript_chunks_.end(),
        [](const TranscriptChunk& a, const TranscriptChunk& b) {
          return a.sequence_number < b.sequence_number;
        });
    if (oldest_chunk_it == transcript->transcript_chunks_.end()) {
      // This should not be reached if `total_size` is greater than zero.
      break;
    }
    total_size -= oldest_chunk_it->text.length();
    // If we're about to remove the chunk that was also the append point,
    // start over.  This should be unlikely; unless there's ~one really big
    // chunk, we're not appending after the oldest chunk.
    if (transcript->last_insertion_it_ == oldest_chunk_it) {
      transcript->last_insertion_it_ = transcript->transcript_chunks_.end();
    }
    transcript->transcript_chunks_.erase(oldest_chunk_it);
  }
}

std::string GlicMediaContext::GetContext() const {
  const Transcript* transcript = GetTranscriptIfExists();
  if (!transcript) {
    return "";
  }

  std::vector<std::string_view> pieces;
  for (const auto& chunk : transcript->transcript_chunks_) {
    pieces.push_back(chunk.text);
  }
  return base::JoinString(pieces, "");
}

std::list<GlicMediaContext::TranscriptChunk>
GlicMediaContext::GetTranscriptChunks() const {
  const Transcript* transcript = GetTranscriptIfExists();
  if (!transcript) {
    return {};
  }
  return transcript->transcript_chunks_;
}

void GlicMediaContext::OnPeerConnectionAdded() {
  num_peer_connections_++;
}

void GlicMediaContext::OnPeerConnectionRemoved() {
  if (num_peer_connections_ > 0) {
    num_peer_connections_--;
  }
}

bool GlicMediaContext::IsExcludedFromTranscript() const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  return num_peer_connections_ > 0 ||
         MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->IsCapturingUserMedia(web_contents);
}

void GlicMediaContext::RemoveOverlappingChunks(
    Transcript* transcript,
    const TranscriptChunk& new_chunk) {
  auto it = transcript->transcript_chunks_.begin();
  while (it != transcript->transcript_chunks_.end()) {
    if (it->HasMediaTimestamps()) {
      // Existing chunk has timing information, check for overlap.
      if (new_chunk.DoesOverlapWith(*it)) {
        // If `new_chunk` somehow overlaps with the insertion hint, forget the
        // hint and search the whole list next time.  This is very rare; it
        // requires the next chunk to overlap with the chunk we just added.
        if (transcript->last_insertion_it_ == it) {
          transcript->last_insertion_it_ = transcript->transcript_chunks_.end();
        }
        // Overlap, erase the current chunk and get the iterator to the next.
        it = transcript->transcript_chunks_.erase(it);
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

std::optional<std::u16string> GlicMediaContext::GetTranscriptTitle() const {
  if (IsExcludedFromTranscript()) {
    return {};
  }

  content::MediaSession* session =
      const_cast<GlicMediaContext*>(this)->GetMediaSessionIfExists();

  // If there is a session, then insist that it matches the routed frame else
  // this frame shouldn't be contributing to the routed frame's transcript.  If
  // there is not a session, then probably this is a test.
  if (session && session->GetRoutedFrame() != &render_frame_host()) {
    return {};
  }

  std::u16string title = u"Unknown";
  if (session) {
    const media_session::MediaMetadata& metadata =
        session->GetMediaSessionMetadata();
    if (!metadata.title.empty()) {
      title = metadata.title;
    }
  }
  return title;
}

GlicMediaContext::Transcript* GlicMediaContext::GetOrCreateTranscript() {
  if (auto* transcript = GetTranscriptIfExists()) {
    return transcript;
  }

  auto title = GetTranscriptTitle();
  if (!title) {
    return nullptr;
  }

  // Create a new transcript for this title.
  auto new_transcript = std::make_unique<Transcript>();
  Transcript* transcript_ptr = new_transcript.get();
  transcripts_by_title_[*title] = std::move(new_transcript);
  return transcript_ptr;
}

GlicMediaContext::Transcript* GlicMediaContext::GetTranscriptIfExists() const {
  auto title = GetTranscriptTitle();
  if (!title) {
    return nullptr;
  }

  auto it = transcripts_by_title_.find(*title);
  if (it == transcripts_by_title_.end()) {
    return nullptr;
  }

  return it->second.get();
}

GlicMediaContext::Transcript::Transcript() = default;
GlicMediaContext::Transcript::~Transcript() = default;

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

content::MediaSession* GlicMediaContext::GetMediaSessionIfExists() const {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  return content::MediaSession::GetIfExists(web_contents);
}

}  // namespace glic
