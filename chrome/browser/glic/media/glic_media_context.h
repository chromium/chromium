// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_

#include <string>

#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

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
  bool IsExcludedFromTranscript() const;

  std::string text_context_;
  std::string most_recent_nonfinal_;
  mutable bool is_excluded_from_transcript_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
