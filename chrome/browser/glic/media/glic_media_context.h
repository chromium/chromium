// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "content/public/browser/document_user_data.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace content {
class RenderFrameHost;
class MediaSession;
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
  void OnPeerConnectionRemoved();

  bool is_excluded_from_transcript_for_testing() {
    return IsExcludedFromTranscript();
  }

  DOCUMENT_USER_DATA_KEY_DECL();

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

  // Returns a copy of the transcript chunks.
  std::list<TranscriptChunk> GetTranscriptChunks() const;

 protected:
  // Gets the current media session, if one exists. Virtual for testing.
  virtual content::MediaSession* GetMediaSessionIfExists() const;

 private:
  // Represents the state of a single transcript.
  struct Transcript {
    Transcript();
    ~Transcript();
    Transcript(const Transcript&) = delete;
    Transcript& operator=(const Transcript&) = delete;

    // Stores transcript chunks in timestamp order.
    std::list<TranscriptChunk> transcript_chunks_;

    // Iterator to the most recent non-final transcript chunk.
    std::list<TranscriptChunk>::iterator nonfinal_chunk_it_ =
        transcript_chunks_.end();

    // The next sequence number to assign to a new chunk.
    uint64_t next_sequence_number_ = 0;

    // Iterator to the last inserted final chunk, to optimize insertion.
    std::list<TranscriptChunk>::iterator last_insertion_it_ =
        transcript_chunks_.end();

    // The maximum transcript size that we've recorded.
    size_t max_transcript_size_ = 0u;
  };

  bool IsExcludedFromTranscript() const;

  // Handles a non-final speech recognition result by inserting or updating a
  // temporary non-final chunk in `transcript_chunks_`.
  void HandleNonFinalResult(Transcript* transcript, TranscriptChunk new_chunk);

  // Handles a final speech recognition result by removing any existing
  // non-final chunk, inserting the new final chunk in the correct order, and
  // trimming the transcript.
  void HandleFinalResult(Transcript* transcript, TranscriptChunk new_chunk);

  // Trims the transcript to a maximum size by removing the oldest chunks until
  // the total size is within the limit.
  void TrimTranscript(Transcript* transcript);

  // Removes any chunks in `transcript_chunks_` that overlap with `new_chunk`.
  void RemoveOverlappingChunks(Transcript* transcript,
                               const TranscriptChunk& new_chunk);

  // Return the title for the current transcript, or nullopt if there should not
  // be a transcript.
  std::optional<std::u16string> GetTranscriptTitle() const;

  // Gets an existing transcript, or returns a new one.  May return nullptr if
  // no transcript should be created.
  Transcript* GetOrCreateTranscript();

  // Returns the current transcript, or nullptr if it doesn't exist.
  Transcript* GetTranscriptIfExists() const;

  // Map from media session title to transcript.
  std::map<std::u16string, std::unique_ptr<Transcript>> transcripts_by_title_;

  size_t num_peer_connections_ = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
