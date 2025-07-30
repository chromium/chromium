// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_

#include <list>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "content/public/browser/document_user_data.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace glic {

// Per-document (frame) context.
class GlicMediaContext : public content::DocumentUserData<GlicMediaContext>,
                         public GlicMediaPageCache::Entry {
 public:
  explicit GlicMediaContext(content::RenderFrameHost* frame);
  ~GlicMediaContext() override;

  bool OnResult(const media::SpeechRecognitionResult&);
  std::string GetContext() const;

  void OnPeerConnectionAdded();

  bool is_excluded_from_transcript_for_testing() {
    return IsExcludedFromTranscript();
  }

  DOCUMENT_USER_DATA_KEY_DECL();

 private:
  // Represents a chunk of the transcript with associated timing information.
  struct TranscriptChunk {
    TranscriptChunk();
    TranscriptChunk(
        std::string text,
        std::optional<media::MediaTimestampRange> timing_information);
    TranscriptChunk(const TranscriptChunk&);
    TranscriptChunk& operator=(const TranscriptChunk&);
    ~TranscriptChunk();

    std::string text;
    std::optional<media::MediaTimestampRange> media_timestamp_range;

    // The sequence number of this chunk, used to determine insertion order.
    uint64_t sequence_number = 0;

    // Helper to get the start time for sorting.  If there is no timing
    // information, returns a large value so that this chunk sorts last.
    base::TimeDelta GetStartTime() const;

    // Helper to get the end time for overlap checks.  If there is no timing
    // information, returns a small value so that this chunk doesn't overlap
    // with any other chunk based on time.
    base::TimeDelta GetEndTime() const;

    // Helper to check for overlap with another chunk.  Chunks without timing
    // information never overlap.
    bool DoesOverlapWith(const TranscriptChunk& chunk2) const;

    // Helper to see if this chunk has media timestamps.
    bool HasMediaTimestamps() const;
  };

  bool IsExcludedFromTranscript() const;

  // Removes any chunks in `final_transcript_chunks_` that overlap with
  // `new_chunk`.
  void RemoveOverlappingChunks(const TranscriptChunk& new_chunk);

  // Stores final transcript chunks in timestamp order.
  std::list<TranscriptChunk> final_transcript_chunks_;
  // Stores the most recent non-final transcript chunk.
  std::optional<TranscriptChunk> most_recent_nonfinal_chunk_;
  mutable bool is_excluded_from_transcript_ = false;

  // The next sequence number to assign to a new chunk.
  uint64_t next_sequence_number_ = 0;

  // Iterator to the last inserted chunk, to optimize insertion.  If it is
  // `end()`, then the next insertion will scan the whole list to find the right
  // insertion point.
  std::list<TranscriptChunk>::iterator last_insertion_it_ =
      final_transcript_chunks_.end();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
