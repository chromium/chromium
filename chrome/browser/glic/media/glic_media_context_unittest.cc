// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

#include <vector>

#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_media_session.h"
#include "content/public/test/test_renderer_host.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using testing::Return;
using testing::ReturnPointee;

namespace glic {

// Helper to create a SpeechRecognitionResult with optional timing.
media::SpeechRecognitionResult CreateSpeechRecognitionResult(
    const std::string& transcription,
    bool is_final,
    const std::vector<media::MediaTimestampRange>& timing_intervals = {}) {
  media::SpeechRecognitionResult result;
  result.transcription = transcription;
  result.is_final = is_final;
  if (!timing_intervals.empty()) {
    result.timing_information = media::TimingInformation();
    result.timing_information->originating_media_timestamps = timing_intervals;
  }
  return result;
}

// Test subclass to allow mocking GetMediaSessionIfExists.
class TestGlicMediaContext : public GlicMediaContext {
 public:
  explicit TestGlicMediaContext(content::RenderFrameHost* frame)
      : GlicMediaContext(frame) {}

  content::MediaSession* GetMediaSessionIfExists() const override {
    return mock_media_session_;
  }

  void SetMediaSession(content::MediaSession* session) {
    mock_media_session_ = session;
  }

 private:
  raw_ptr<content::MediaSession> mock_media_session_ = nullptr;
};

class GlicMediaContextTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // GlicMediaContext::CreateForCurrentDocument(rfh());
    context_ = std::make_unique<TestGlicMediaContext>(rfh());
    context_->SetMediaSession(&mock_media_session_);
    ON_CALL(mock_media_session(), GetMediaSessionMetadata)
        .WillByDefault(ReturnPointee(&metadata_));
    ON_CALL(mock_media_session(), GetRoutedFrame).WillByDefault(Return(rfh()));
  }

  void TearDown() override {
    context_->SetMediaSession(nullptr);
    context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::RenderFrameHost* rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestGlicMediaContext* context() { return context_.get(); }

  content::MockMediaSession& mock_media_session() {
    return mock_media_session_;
  }

  void SetMetadata(const media_session::MediaMetadata& metadata) {
    metadata_ = metadata;
  }

 private:
  media_session::MediaMetadata metadata_;
  content::MockMediaSession mock_media_session_;
  std::unique_ptr<TestGlicMediaContext> context_;
};

TEST_F(GlicMediaContextTest, InitialContextIsEmpty) {
  EXPECT_EQ(context()->GetContext(), "");
}

TEST_F(GlicMediaContextTest, GetVariantsWork) {
  auto* rfh = this->rfh();
  GlicMediaContext::CreateForCurrentDocument(rfh);
  auto* context = GlicMediaContext::GetForCurrentDocument(rfh);
  EXPECT_NE(context, nullptr);
  EXPECT_EQ(GlicMediaContext::GetForCurrentDocument(rfh), context);
}

TEST_F(GlicMediaContextTest, ContextContainsTranscript) {
  // Send the string in pieces.
  const std::string test_cap_1("ABC");
  const std::string test_cap_2("DEF");
  const std::string test_cap_3("GHIJ");
  EXPECT_TRUE(context()->OnResult(
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true)));
  EXPECT_TRUE(context()->OnResult(
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/true)));
  EXPECT_TRUE(context()->OnResult(
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true)));

  EXPECT_EQ(context()->GetContext(), "ABCDEFGHIJ");
}

TEST_F(GlicMediaContextTest, ContextShouldTruncate) {
  // Send in a very long string, and expect that it comes back shorter with
  // the end of the string retained.  We don't care exactly how long it is, as
  // long as there's some limit to prevent it from going forever.
  std::string long_cap(100000, 'A');
  constexpr int num_repeats = 15;
  for (int i = 0; i < num_repeats; ++i) {
    context()->OnResult(
        media::SpeechRecognitionResult(long_cap, /*is_final=*/true));
  }

  const std::string actual_cap = context()->GetContext();
  EXPECT_LT(actual_cap.length(), long_cap.length() * num_repeats);
}

TEST_F(GlicMediaContextTest, ContextContainsButReplacesNonFinal) {
  // Send the string in pieces, mixing final and non-final ones.
  const std::string test_cap_1("ABC");
  context()->OnResult(
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true));
  EXPECT_EQ(context()->GetContext(), test_cap_1);

  const std::string test_cap_2("DEF");
  context()->OnResult(
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/false));
  EXPECT_EQ(context()->GetContext(), test_cap_1 + test_cap_2);

  // The final result "GHI" will be appended, and the non-final result "DEF"
  // will be cleared.
  const std::string test_cap_3("GHI");
  context()->OnResult(
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true));
  EXPECT_EQ(context()->GetContext(), test_cap_1 + test_cap_3);
}

TEST_F(GlicMediaContextTest, AudioCaptureStopsTranscription) {
  auto capture_dispatcher = MediaCaptureDevicesDispatcher::GetInstance()
                                ->GetMediaStreamCaptureIndicator();
  const blink::MediaStreamDevice audio_device(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "id", "name");
  blink::mojom::StreamDevices devices(audio_device, {});
  auto stream =
      capture_dispatcher->RegisterMediaStream(web_contents(), devices);
  stream->OnStarted(base::DoNothing(), content::MediaStreamUI::SourceCallback(),
                    std::string(), {},
                    content::MediaStreamUI::StateChangeCallback());
  // It must report that the tab is capturing audio, else we've done something
  // wrong setting this up.
  ASSERT_TRUE(capture_dispatcher->IsCapturingAudio(web_contents()));

  // Send a transcription and verify that it is ignored.
  EXPECT_TRUE(context()->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context()->GetContext(), "");

  stream.reset();
}

TEST_F(GlicMediaContextTest, PeerConnectionStopsTranscription) {
  // Send a transcription and verify that it is ignored once a peer connection
  // is added to the WebContents.
  context()->OnPeerConnectionAdded();
  EXPECT_TRUE(context()->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context()->GetContext(), "");
}

TEST_F(GlicMediaContextTest, PeerConnectionAddedAndRemovedResetsExclusion) {
  context()->OnPeerConnectionAdded();
  EXPECT_TRUE(context()->is_excluded_from_transcript_for_testing());
  context()->OnPeerConnectionRemoved();
  EXPECT_FALSE(context()->is_excluded_from_transcript_for_testing());
}

TEST_F(GlicMediaContextTest, ExclusionRemainsIfUserMediaIsActive) {
  // Enable user media capture.
  auto capture_dispatcher = MediaCaptureDevicesDispatcher::GetInstance()
                                ->GetMediaStreamCaptureIndicator();
  const blink::MediaStreamDevice audio_device(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "id", "name");
  blink::mojom::StreamDevices devices(audio_device, {});
  auto stream =
      capture_dispatcher->RegisterMediaStream(web_contents(), devices);
  stream->OnStarted(base::DoNothing(), content::MediaStreamUI::SourceCallback(),
                    std::string(), {},
                    content::MediaStreamUI::StateChangeCallback());
  ASSERT_TRUE(capture_dispatcher->IsCapturingAudio(web_contents()));

  // Add and remove a peer connection.
  context()->OnPeerConnectionAdded();
  EXPECT_TRUE(context()->is_excluded_from_transcript_for_testing());
  context()->OnPeerConnectionRemoved();

  // Exclusion should remain active because of user media.
  EXPECT_TRUE(context()->is_excluded_from_transcript_for_testing());

  stream.reset();
}

TEST_F(GlicMediaContextTest, ExclusionRemainsIfPeerConnectionsAreActive) {
  context()->OnPeerConnectionAdded();
  context()->OnPeerConnectionAdded();
  EXPECT_TRUE(context()->is_excluded_from_transcript_for_testing());

  context()->OnPeerConnectionRemoved();
  EXPECT_TRUE(context()->is_excluded_from_transcript_for_testing());

  context()->OnPeerConnectionRemoved();
  EXPECT_FALSE(context()->is_excluded_from_transcript_for_testing());
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithTiming_EmptyContext) {
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "hello world", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))});

  EXPECT_TRUE(context()->OnResult(result));
  EXPECT_EQ(context()->GetContext(), "hello world");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithTiming_NoOverlap) {
  // Add initial chunks using OnResult
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Add a new chunk that fits in between
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))});

  EXPECT_TRUE(context()->OnResult(result));
  EXPECT_EQ(context()->GetContext(), "chunk onechunk twochunk three");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_OverlapsSingleChunk) {
  // Add initial chunks using OnResult
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Add a new chunk that overlaps with "chunk two".
  // End time is exclusive, so we set it to end exactly where chunk two ends
  // and chunk three starts, to be sure that only chunk two is removed.
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "new chunk", true,
      {media::MediaTimestampRange(base::Seconds(1.2), base::Seconds(2))});

  EXPECT_TRUE(context()->OnResult(result));
  EXPECT_EQ(context()->GetContext(), "chunk onenew chunkchunk three");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_OverlapsMultipleChunks) {
  // Add initial chunks using OnResult
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk four", true,
      {media::MediaTimestampRange(base::Seconds(3), base::Seconds(4))}));

  // Add a new chunk that overlaps with "chunk two" and "chunk three"
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "overlapping new chunk", true,
      {media::MediaTimestampRange(base::Seconds(1.5), base::Seconds(2.5))});

  EXPECT_TRUE(context()->OnResult(result));
  EXPECT_EQ(context()->GetContext(),
            "chunk oneoverlapping new chunkchunk four");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithoutTiming) {
  // Add an initial timed chunk using OnResult
  context()->OnResult(CreateSpeechRecognitionResult(
      "timed chunk", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Add a new chunk without timing information
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "untimed chunk", true);  // No timing intervals

  EXPECT_TRUE(context()->OnResult(result));
  // Untimed chunks are currently just appended.
  EXPECT_EQ(context()->GetContext(), "timed chunkuntimed chunk");
}

TEST_F(GlicMediaContextTest, OnResult_MultipleFinalResultsWithoutTiming) {
  context()->OnResult(CreateSpeechRecognitionResult("first chunk", true));

  // Add a new chunk without timing information
  media::SpeechRecognitionResult result =
      CreateSpeechRecognitionResult("second chunk", true);

  EXPECT_TRUE(context()->OnResult(result));
  // Untimed chunks are currently just appended.
  EXPECT_EQ(context()->GetContext(), "first chunksecond chunk");
}

TEST_F(GlicMediaContextTest, OnResult_NonFinalResult) {
  media::SpeechRecognitionResult result =
      CreateSpeechRecognitionResult("non-final text", false);  // Not final

  EXPECT_TRUE(context()->OnResult(result));
  EXPECT_EQ(context()->GetContext(), "non-final text");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultClearsNonFinal) {
  // Add a non-final result first
  media::SpeechRecognitionResult non_final_result =
      CreateSpeechRecognitionResult("non-final text", false);
  context()->OnResult(non_final_result);
  EXPECT_EQ(context()->GetContext(), "non-final text");

  // Add a final result
  media::SpeechRecognitionResult final_result = CreateSpeechRecognitionResult(
      "final text", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))});

  EXPECT_TRUE(context()->OnResult(final_result));
  EXPECT_EQ(context()->GetContext(), "final text");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_MultipleMediaTimestamps) {
  // Add a final chunk using OnResult
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Add another chunk with multiple timestamps.  It should return true but
  // ignore the chunk.
  EXPECT_TRUE(context()->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2)),
       media::MediaTimestampRange(base::Seconds(3), base::Seconds(4))})));

  EXPECT_EQ(context()->GetContext(), "chunk one");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_NonfinalChunkAppended) {
  // Add initial chunks using OnResult. `last_insertion_it_` will point to
  // "chunk three" after the second call, which is also the chronologically last
  // entry in the list of chunks.  The nonfinal entry will be appended to the
  // end of the list.
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Insert a non-final chunk. It should be placed after "chunk three".
  context()->OnResult(CreateSpeechRecognitionResult("chunk two", false));

  EXPECT_EQ(context()->GetContext(), "chunk onechunk threechunk two");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_NonfinalChunkFollowsLastFinalChunk) {
  // Add initial chunks using OnResult. `last_insertion_it_` will point to
  // "chunk three" after the second call.  However, it is not last
  // chronologically, so the next nonfinal chunk should end up after it but
  // before "chunk one".
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Insert a non-final chunk. It should be placed after "chunk three".
  context()->OnResult(CreateSpeechRecognitionResult("chunk two", false));

  EXPECT_EQ(context()->GetContext(), "chunk threechunk twochunk one");
}

TEST_F(GlicMediaContextTest,
       FinalChunkNoTimestamp_IsInsertedAfterLastFinalChunk) {
  // Add initial chunks out of chronological order to ensure insertion is based
  // on the last *added* final chunk, not the chronologically last one.
  // `last_insertion_it_` will point to "Final One" after the second call.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Final Three. ", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "Final One. ", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // The context should be sorted by time.
  EXPECT_EQ(context()->GetContext(), "Final One. Final Three. ");

  // Add a final chunk without a timestamp. It should be inserted after
  // "Final One", which was the last final chunk added.
  context()->OnResult(
      CreateSpeechRecognitionResult("Final Two (no time). ", true));

  // The final context should show the untimed chunk inserted after the last
  // added final chunk.
  EXPECT_EQ(context()->GetContext(),
            "Final One. Final Two (no time). Final Three. ");
}

TEST_F(GlicMediaContextTest, GetTranscriptChunks_ReturnsCorrectChunks) {
  // Add initial chunks using OnResult. `last_insertion_it_` will point to
  // "chunk three" after the second call. However, it is not last
  // chronologically, so the next nonfinal chunk should end up after it but
  // before "chunk one".
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Insert a non-final chunk. It should be placed after "chunk three".
  context()->OnResult(CreateSpeechRecognitionResult("chunk two", false));

  // Verify GetTranscriptChunks().
  auto chunks = context()->GetTranscriptChunks();
  ASSERT_EQ(chunks.size(), 3u);
  auto it = chunks.begin();
  EXPECT_EQ(it->text, "chunk three");
  ASSERT_TRUE(it->HasMediaTimestamps());
  EXPECT_EQ(it->GetStartTime(), base::Seconds(0));
  EXPECT_EQ(it->GetEndTime(), base::Seconds(1));
  ++it;
  EXPECT_EQ(it->text, "chunk two");
  EXPECT_FALSE(it->HasMediaTimestamps());
  ++it;
  EXPECT_EQ(it->text, "chunk one");
  ASSERT_TRUE(it->HasMediaTimestamps());
  EXPECT_EQ(it->GetStartTime(), base::Seconds(2));
  EXPECT_EQ(it->GetEndTime(), base::Seconds(3));
}

TEST_F(GlicMediaContextTest, ContextShouldTruncateLeastRecentlyAdded) {
  // Send a long string, then a short one. The context should truncate the long
  // one first, even though it has a later timestamp.
  std::string long_cap(1000000, 'A');
  context()->OnResult(CreateSpeechRecognitionResult(
      long_cap, /*is_final=*/true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "B", /*is_final=*/true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // The long one should have been evicted.
  EXPECT_EQ(context()->GetContext(), "B");
}

TEST_F(GlicMediaContextTest,
       NonFinalChunkNoTimestamp_IsInsertedAfterLastFinalChunk) {
  context()->OnResult(CreateSpeechRecognitionResult(
      "Final One. ", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  context()->OnResult(CreateSpeechRecognitionResult(
      "Final Two. ", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  EXPECT_EQ(context()->GetContext(), "Final One. Final Two. ");

  // Add a non-final chunk without a timestamp. It should be inserted after the
  // most recently added final chunk ("Final Two").
  context()->OnResult(CreateSpeechRecognitionResult("Non-final. ", false));
  EXPECT_EQ(context()->GetContext(), "Final One. Final Two. Non-final. ");

  // Add another final chunk.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Final Three. ", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));

  // The previous non-final chunk should be gone, and the context should be
  // sorted correctly.
  EXPECT_EQ(context()->GetContext(), "Final One. Final Three. Final Two. ");

  // Add another non-final chunk. It should be inserted after "Final Three"
  // because that was the last one added.
  context()->OnResult(CreateSpeechRecognitionResult("Non-final Two.", false));
  EXPECT_EQ(context()->GetContext(),
            "Final One. Final Three. Non-final Two.Final Two. ");
}

TEST_F(GlicMediaContextTest, NonFinalChunkWithTimestamp_ReplacesExisting) {
  // Add a non-final chunk with a timestamp.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Non-final One. ", false,
      {media::MediaTimestampRange(base::Seconds(10), base::Seconds(11))}));
  EXPECT_EQ(context()->GetContext(), "Non-final One. ");

  // Add another non-final chunk with a different timestamp.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Non-final Two. ", false,
      {media::MediaTimestampRange(base::Seconds(20), base::Seconds(21))}));

  // The first non-final chunk should be replaced by the second one.
  EXPECT_EQ(context()->GetContext(), "Non-final Two. ");
}

TEST_F(GlicMediaContextTest, NonFinalChunkWithTimestamp_UpdatesInPlace) {
  // Add a non-final chunk with a timestamp.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Hello", false,
      {media::MediaTimestampRange(base::Seconds(10), base::Seconds(11))}));
  EXPECT_EQ(context()->GetContext(), "Hello");

  // Add another non-final chunk with the same timestamp but different text.
  context()->OnResult(CreateSpeechRecognitionResult(
      "Hello world", false,
      {media::MediaTimestampRange(base::Seconds(10), base::Seconds(13))}));

  // The chunk should be updated in place.
  EXPECT_EQ(context()->GetContext(), "Hello world");
  auto chunks = context()->GetTranscriptChunks();
  EXPECT_EQ(chunks.size(), 1u);
}

TEST_F(GlicMediaContextTest, TranscriptSwitchesWithMediaSessionTitle) {
  media_session::MediaMetadata metadata;
  metadata.title = u"Title One";
  SetMetadata(metadata);

  context()->OnResult(CreateSpeechRecognitionResult("Transcript one", true));
  EXPECT_EQ(context()->GetContext(), "Transcript one");

  metadata.title = u"Title Two";
  SetMetadata(metadata);

  EXPECT_EQ(context()->GetContext(), "");
  context()->OnResult(CreateSpeechRecognitionResult("Transcript two", true));
  EXPECT_EQ(context()->GetContext(), "Transcript two");

  metadata.title = u"Title One";
  SetMetadata(metadata);
  EXPECT_EQ(context()->GetContext(), "Transcript one");
}

TEST_F(GlicMediaContextTest, GetMediaSessionIfExists_FiltersByRoutedFrame) {
  // If the routed frame is not `rfh()`, then `OnResult` should do nothing.
  // nullptr, in this case, is just something that is not rfh().
  ON_CALL(mock_media_session(), GetRoutedFrame).WillByDefault(Return(nullptr));
  EXPECT_TRUE(context()->OnResult(CreateSpeechRecognitionResult("test", true)));
  EXPECT_EQ(context()->GetContext(), "");

  // If the routed frame is `rfh()`, then it should return the session.
  ON_CALL(mock_media_session(), GetRoutedFrame).WillByDefault(Return(rfh()));
  EXPECT_TRUE(context()->OnResult(CreateSpeechRecognitionResult("test", true)));
  EXPECT_EQ(context()->GetContext(), "test");
}

}  // namespace glic
