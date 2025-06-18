// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace {
constexpr char kGlicMediaContextName[] = "GlicMediaContext";
}  // namespace

namespace glic {

GlicMediaContext::GlicMediaContext(content::Page* page) : page_(page) {}

GlicMediaContext::~GlicMediaContext() = default;

// static
GlicMediaContext* GlicMediaContext::GetOrCreateFor(
    content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetPrimaryMainFrame()) {
    return nullptr;
  }

  auto& page = web_contents->GetPrimaryMainFrame()->GetPage();

  if (auto* media_context = static_cast<GlicMediaContext*>(
          page.GetUserData(kGlicMediaContextName))) {
    return media_context;
  }

  auto new_media_context = std::make_unique<GlicMediaContext>(&page);
  auto* media_context = new_media_context.get();
  page.SetUserData(kGlicMediaContextName, std::move(new_media_context));

  return media_context;
}

// static
GlicMediaContext* GlicMediaContext::GetIfExistsFor(
    content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetPrimaryMainFrame()) {
    return nullptr;
  }

  auto& page = web_contents->GetPrimaryMainFrame()->GetPage();

  return static_cast<GlicMediaContext*>(
      page.GetUserData(kGlicMediaContextName));
}

bool GlicMediaContext::OnResult(const media::SpeechRecognitionResult& result) {
  if (IsExcludedFromTranscript()) {
    return false;
  }

  if (!result.is_final) {
    most_recent_nonfinal_ = result.transcription;
    return true;
  }
  text_context_ += result.transcription;
  most_recent_nonfinal_.clear();

  // Trim to `max_size`.  Note that we should utf8-trim, but this is easier.
  constexpr size_t max_size = 20000;
  if (size_t text_context_size = text_context_.length()) {
    if (text_context_size > max_size) {
      // Remove the beginning of the context, leaving the end.
      text_context_ = text_context_.substr(text_context_size - max_size);
    }
  }

  return true;
}

std::string GlicMediaContext::GetContext() const {
  if (IsExcludedFromTranscript()) {
    return "";
  }
  return text_context_ + most_recent_nonfinal_;
}

void GlicMediaContext::OnPeerConnectionAdded() {
  is_excluded_from_transcript_ = true;
}

bool GlicMediaContext::IsExcludedFromTranscript() const {
  if (is_excluded_from_transcript_) {
    // Skip checking if it's already excluded.
    return true;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page_->GetMainDocument());
  is_excluded_from_transcript_ |= MediaCaptureDevicesDispatcher::GetInstance()
                                      ->GetMediaStreamCaptureIndicator()
                                      ->IsCapturingUserMedia(web_contents);

  return is_excluded_from_transcript_;
}

}  // namespace glic
