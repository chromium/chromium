// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_MEDIA_TRANSCRIPT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_GLIC_MEDIA_MEDIA_TRANSCRIPT_PROVIDER_IMPL_H_

#include "components/optimization_guide/content/browser/media_transcript_provider.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace glic {

// An implementation of MediaTranscriptProvider that gets transcripts from
// GlicMediaContext.
class MediaTranscriptProviderImpl
    : public optimization_guide::MediaTranscriptProvider {
 public:
  MediaTranscriptProviderImpl();
  ~MediaTranscriptProviderImpl() override;

  // optimization_guide::MediaTranscriptProvider:
  std::vector<optimization_guide::proto::MediaTranscript>
  GetTranscriptsForFrame(content::RenderFrameHost* rfh) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_MEDIA_TRANSCRIPT_PROVIDER_IMPL_H_
