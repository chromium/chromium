// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_context.h"

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

void GlicMediaContext::OnResult(const media::SpeechRecognitionResult& result) {
  if (!result.is_final) {
    most_recent_nonfinal_ = result.transcription;
    return;
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
}

std::string GlicMediaContext::GetContext() const {
  return text_context_ + most_recent_nonfinal_;
}

}  // namespace glic
