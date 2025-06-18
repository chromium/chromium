// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace glic {

class GlicMediaContextTest : public ChromeRenderViewHostTestHarness {};

TEST_F(GlicMediaContextTest, GetWithNullReturnsNull) {
  EXPECT_EQ(GlicMediaContext::GetOrCreateFor(nullptr), nullptr);
  EXPECT_EQ(GlicMediaContext::GetIfExistsFor(nullptr), nullptr);
}

TEST_F(GlicMediaContextTest, InitialContextIsEmpty) {
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());
  EXPECT_EQ(context->GetContext(), "");
}

TEST_F(GlicMediaContextTest, GetVariantsWork) {
  EXPECT_EQ(GlicMediaContext::GetIfExistsFor(web_contents()), nullptr);
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());
  EXPECT_NE(context, nullptr);
  EXPECT_EQ(GlicMediaContext::GetIfExistsFor(web_contents()), context);
  EXPECT_EQ(GlicMediaContext::GetOrCreateFor(web_contents()), context);
}

TEST_F(GlicMediaContextTest, ContextContainsTranscript) {
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());

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
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());

  // Send in a very long string, and expect that it comes back shorter with
  // the end of the string retained.  We don't care exactly how long it is, as
  // long as there's some limit to prevent it from going forever.
  std::string long_cap(30000, 'A');
  long_cap.back() = 'B';
  context->OnResult(
      media::SpeechRecognitionResult(long_cap, /*is_final=*/true));

  const std::string actual_cap = context->GetContext();
  EXPECT_LT(actual_cap.length(), long_cap.length());
  EXPECT_EQ(actual_cap.back(), long_cap.back());
}

TEST_F(GlicMediaContextTest, ContextContainsButReplacesNonFinal) {
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());

  // Send the string in pieces, mixing final and non-final ones.
  const std::string test_cap_1("ABC");
  context->OnResult(
      media::SpeechRecognitionResult(test_cap_1, /*is_final=*/true));
  EXPECT_EQ(context->GetContext(), test_cap_1);

  const std::string test_cap_2("DEF");
  context->OnResult(
      media::SpeechRecognitionResult(test_cap_2, /*is_final=*/false));
  EXPECT_EQ(context->GetContext(), test_cap_1 + test_cap_2);

  // Should replace cap_2.
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
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());
  EXPECT_FALSE(context->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context->GetContext(), "");

  stream.reset();
}

TEST_F(GlicMediaContextTest, PeerConnectionStopsTranscription) {
  // Send a transcription and verify that it is ignored once a peer connection
  // is added to the WebContents.
  auto* context = GlicMediaContext::GetOrCreateFor(web_contents());
  context->OnPeerConnectionAdded();
  EXPECT_FALSE(context->OnResult(
      media::SpeechRecognitionResult("ABC", /*is_final=*/true)));
  EXPECT_EQ(context->GetContext(), "");
}

}  // namespace glic
