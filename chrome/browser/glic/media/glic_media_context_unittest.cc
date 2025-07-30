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
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

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

class GlicMediaContextTest : public ChromeRenderViewHostTestHarness {
 public:
  content::RenderFrameHost* rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }
};

TEST_F(GlicMediaContextTest, InitialContextIsEmpty) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_EQ(context->GetContext(), "");
}

TEST_F(GlicMediaContextTest, GetVariantsWork) {
  EXPECT_EQ(GlicMediaContext::GetForCurrentDocument(rfh()), nullptr);
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_NE(context, nullptr);
  EXPECT_EQ(GlicMediaContext::GetForCurrentDocument(rfh()), context);
  EXPECT_EQ(GlicMediaContext::GetOrCreateForCurrentDocument(rfh()), context);
}

TEST_F(GlicMediaContextTest, ContextContainsTranscript) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Send the string in pieces.
  const std::string test_cap_1("ABC");
  const std::string test_cap_2("DEF");
  const std::string test_cap_3("GHIJ");
  EXPECT_TRUE(context->OnResult(
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true)));
  EXPECT_TRUE(context->OnResult(
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/true)));
  EXPECT_TRUE(context->OnResult(
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true)));

  EXPECT_EQ(context->GetContext(), "ABCDEFGHIJ");
}

TEST_F(GlicMediaContextTest, ContextShouldTruncate) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Send in a very long string, and expect that it comes back shorter with
  // the end of the string retained.  We don't care exactly how long it is, as
  // long as there's some limit to prevent it from going forever.
  std::string long_cap(100000, 'A');
  constexpr int num_repeats = 15;
  for (int i = 0; i < num_repeats; ++i) {
    context->OnResult(
        media::SpeechRecognitionResult(long_cap, /*is_final=*/true));
  }

  const std::string actual_cap = context->GetContext();
  EXPECT_LT(actual_cap.length(), long_cap.length() * num_repeats);
}

TEST_F(GlicMediaContextTest, ContextContainsButReplacesNonFinal) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Send the string in pieces, mixing final and non-final ones.
  const std::string test_cap_1("ABC");
  context->OnResult(
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true));
  EXPECT_EQ(context->GetContext(), test_cap_1);

  const std::string test_cap_2("DEF");
  context->OnResult(
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/false));
  EXPECT_EQ(context->GetContext(), test_cap_1 + test_cap_2);

  // The final result "GHI" will be appended, and the non-final result "DEF"
  // will be cleared.
  const std::string test_cap_3("GHI");
  context->OnResult(
      media::SpeechRecognitionResult(test_cap_3, /*is_final=*/true));
  EXPECT_EQ(context->GetContext(), test_cap_1 + test_cap_3);
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
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_FALSE(context->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context->GetContext(), "");

  stream.reset();
}

TEST_F(GlicMediaContextTest, PeerConnectionStopsTranscription) {
  // Send a transcription and verify that it is ignored once a peer connection
  // is added to the WebContents.
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  context->OnPeerConnectionAdded();
  EXPECT_FALSE(context->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context->GetContext(), "");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithTiming_EmptyContext) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "hello world", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))});

  EXPECT_TRUE(media_context->OnResult(result));
  EXPECT_EQ(media_context->GetContext(), "hello world");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithTiming_NoOverlap) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add initial chunks using OnResult
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Add a new chunk that fits in between
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))});

  EXPECT_TRUE(media_context->OnResult(result));
  EXPECT_EQ(media_context->GetContext(), "chunk onechunk twochunk three");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_OverlapsSingleChunk) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add initial chunks using OnResult
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Add a new chunk that overlaps with "chunk two".
  // End time is exclusive, so we set it to end exactly where chunk two ends
  // and chunk three starts, to be sure that only chunk two is removed.
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "new chunk", true,
      {media::MediaTimestampRange(base::Seconds(1.2), base::Seconds(2))});

  EXPECT_TRUE(media_context->OnResult(result));
  EXPECT_EQ(media_context->GetContext(), "chunk onenew chunkchunk three");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_OverlapsMultipleChunks) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add initial chunks using OnResult
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk four", true,
      {media::MediaTimestampRange(base::Seconds(3), base::Seconds(4))}));

  // Add a new chunk that overlaps with "chunk two" and "chunk three"
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "overlapping new chunk", true,
      {media::MediaTimestampRange(base::Seconds(1.5), base::Seconds(2.5))});

  EXPECT_TRUE(media_context->OnResult(result));
  EXPECT_EQ(media_context->GetContext(),
            "chunk oneoverlapping new chunkchunk four");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultWithoutTiming) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add an initial timed chunk using OnResult
  media_context->OnResult(CreateSpeechRecognitionResult(
      "timed chunk", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Add a new chunk without timing information
  media::SpeechRecognitionResult result = CreateSpeechRecognitionResult(
      "untimed chunk", true);  // No timing intervals

  EXPECT_TRUE(media_context->OnResult(result));
  // Untimed chunks are currently just appended.
  EXPECT_EQ(media_context->GetContext(), "timed chunkuntimed chunk");
}

TEST_F(GlicMediaContextTest, OnResult_MultipleFinalResultsWithoutTiming) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  media_context->OnResult(CreateSpeechRecognitionResult("first chunk", true));

  // Add a new chunk without timing information
  media::SpeechRecognitionResult result =
      CreateSpeechRecognitionResult("second chunk", true);

  EXPECT_TRUE(media_context->OnResult(result));
  // Untimed chunks are currently just appended.
  EXPECT_EQ(media_context->GetContext(), "first chunksecond chunk");
}

TEST_F(GlicMediaContextTest, OnResult_NonFinalResult) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  media::SpeechRecognitionResult result =
      CreateSpeechRecognitionResult("non-final text", false);  // Not final

  EXPECT_TRUE(media_context->OnResult(result));
  EXPECT_EQ(media_context->GetContext(), "non-final text");
}

TEST_F(GlicMediaContextTest, OnResult_FinalResultClearsNonFinal) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add a non-final result first
  media::SpeechRecognitionResult non_final_result =
      CreateSpeechRecognitionResult("non-final text", false);
  media_context->OnResult(non_final_result);
  EXPECT_EQ(media_context->GetContext(), "non-final text");

  // Add a final result
  media::SpeechRecognitionResult final_result = CreateSpeechRecognitionResult(
      "final text", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))});

  EXPECT_TRUE(media_context->OnResult(final_result));
  EXPECT_EQ(media_context->GetContext(), "final text");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_MultipleMediaTimestamps) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add a final chunk using OnResult
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Add another chunk with multiple timestamps.  It should return true but
  // ignore the chunk.
  EXPECT_TRUE(media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk two", true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2)),
       media::MediaTimestampRange(base::Seconds(3), base::Seconds(4))})));

  EXPECT_EQ(media_context->GetContext(), "chunk one");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_NonfinalChunkAppended) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add initial chunks using OnResult. `last_insertion_it_` will point to
  // "chunk three" after the second call, which is also the chronologically last
  // entry in the list of chunks.  The nonfinal entry will be appended to the
  // end of the list.
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));

  // Insert a non-final chunk. It should be placed after "chunk three".
  media_context->OnResult(CreateSpeechRecognitionResult("chunk two", false));

  EXPECT_EQ(media_context->GetContext(), "chunk onechunk threechunk two");
}

TEST_F(GlicMediaContextTest,
       OnResult_FinalResultWithTiming_NonfinalChunkFollowsLastFinalChunk) {
  auto* media_context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Add initial chunks using OnResult. `last_insertion_it_` will point to
  // "chunk three" after the second call.  However, it is not last
  // chronologically, so the next nonfinal chunk should end up after it but
  // before "chunk one".
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk one", true,
      {media::MediaTimestampRange(base::Seconds(2), base::Seconds(3))}));
  media_context->OnResult(CreateSpeechRecognitionResult(
      "chunk three", true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // Insert a non-final chunk. It should be placed after "chunk three".
  media_context->OnResult(CreateSpeechRecognitionResult("chunk two", false));

  EXPECT_EQ(media_context->GetContext(), "chunk threechunk twochunk one");
}

TEST_F(GlicMediaContextTest, ContextShouldTruncateLeastRecentlyAdded) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Send a long string, then a short one. The context should truncate the long
  // one first, even though it has a later timestamp.
  std::string long_cap(1000000, 'A');
  context->OnResult(CreateSpeechRecognitionResult(
      long_cap, /*is_final=*/true,
      {media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))}));
  context->OnResult(CreateSpeechRecognitionResult(
      "B", /*is_final=*/true,
      {media::MediaTimestampRange(base::Seconds(0), base::Seconds(1))}));

  // The long one should have been evicted.
  EXPECT_EQ(context->GetContext(), "B");
}

}  // namespace glic
