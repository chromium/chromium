// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/media_transcript_provider_impl.h"

#include "chrome/browser/glic/media/glic_media_context.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"

namespace glic {

MediaTranscriptProviderImpl::MediaTranscriptProviderImpl() = default;
MediaTranscriptProviderImpl::~MediaTranscriptProviderImpl() = default;

std::vector<optimization_guide::proto::MediaTranscript>
MediaTranscriptProviderImpl::GetTranscriptsForFrame(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return {};
  }

  auto* context = GlicMediaContext::GetForCurrentDocument(rfh);
  if (!context) {
    return {};
  }

  const std::list<GlicMediaContext::TranscriptChunk> chunks =
      context->GetTranscriptChunks();
  if (chunks.empty()) {
    return {};
  }

  std::vector<optimization_guide::proto::MediaTranscript> transcripts;
  for (const auto& chunk : chunks) {
    auto& transcript = transcripts.emplace_back();
    transcript.set_text(chunk.text);
    if (chunk.media_timestamp_range) {
      transcript.set_start_timestamp_milliseconds(
          chunk.media_timestamp_range->start.InMilliseconds());
    }
  }
  return transcripts;
}

}  // namespace glic
