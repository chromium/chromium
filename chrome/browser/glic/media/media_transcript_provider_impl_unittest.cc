// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/media_transcript_provider_impl.h"

#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class MediaTranscriptProviderImplTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    provider_ = std::make_unique<MediaTranscriptProviderImpl>();
  }

  content::RenderFrameHost* rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  MediaTranscriptProviderImpl& provider() { return *provider_; }

 private:
  std::unique_ptr<MediaTranscriptProviderImpl> provider_;
};

TEST_F(MediaTranscriptProviderImplTest, GetTranscriptsForFrame) {
  auto* context = GlicMediaContext::GetOrCreateForCurrentDocument(rfh());

  // Send a final transcription with timing information.
  media::SpeechRecognitionResult timed_result("timed chunk", /*is_final=*/true);
  timed_result.timing_information = media::TimingInformation();
  timed_result.timing_information->originating_media_timestamps = {
      media::MediaTimestampRange(base::Seconds(1), base::Seconds(2))};
  context->OnResult(timed_result);

  // Send a final transcription without timing information.
  context->OnResult(
      media::SpeechRecognitionResult("untimed chunk", /*is_final=*/true));

  // Send a non-final transcription.
  context->OnResult(
      media::SpeechRecognitionResult("non-final chunk", /*is_final=*/false));

  // Verify the returned transcripts.
  auto transcripts = provider().GetTranscriptsForFrame(rfh());
  ASSERT_EQ(transcripts.size(), 3u);

  // The timed chunk should come first due to sorting.
  EXPECT_EQ(transcripts[0].text(), "timed chunk");
  EXPECT_EQ(transcripts[0].start_timestamp_milliseconds(), 1000);

  // The untimed chunk is appended next.
  EXPECT_EQ(transcripts[1].text(), "untimed chunk");
  EXPECT_FALSE(transcripts[1].has_start_timestamp_milliseconds());

  // The non-final chunk is appended after the last final chunk.
  EXPECT_EQ(transcripts[2].text(), "non-final chunk");
  EXPECT_FALSE(transcripts[2].has_start_timestamp_milliseconds());
}

TEST_F(MediaTranscriptProviderImplTest, GetTranscriptsForFrameEdgeCases) {
  EXPECT_TRUE(provider().GetTranscriptsForFrame(nullptr).empty());
  EXPECT_TRUE(provider().GetTranscriptsForFrame(rfh()).empty());

  // No transcripts are provided.
  GlicMediaContext::GetOrCreateForCurrentDocument(rfh());
  EXPECT_TRUE(provider().GetTranscriptsForFrame(rfh()).empty());
}

}  // namespace glic
