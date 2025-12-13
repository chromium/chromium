// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "url/origin.h"

namespace optimization_guide {
namespace proto {
class ContentNode;
}  // namespace proto
}  // namespace optimization_guide

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace glic {

class GlicMediaIntegration {
 public:
  using ContextCallback = base::OnceCallback<void(const std::string&)>;

  GlicMediaIntegration() = default;
  virtual ~GlicMediaIntegration() = default;

  // May return null if no integration is needed.
  static GlicMediaIntegration* GetFor(content::WebContents*);

  // Use `context_root` to store our context information.  `context_root` will
  // be overwritten.  This selects a frame that has a transcript, if any.
  //
  // This is deprecated in favor of the RFH variant below.
  virtual void AppendContext(
      content::WebContents* web_contents,
      optimization_guide::proto::ContentNode* context_root) = 0;

  // Per-frame version of `AppendContext`.
  virtual void AppendContextForFrame(
      content::RenderFrameHost* rfh,
      optimization_guide::proto::ContentNode* context_root) = 0;

  // Pretend that a peer connection has been added / removed.
  virtual void OnPeerConnectionAddedForTesting(content::RenderFrameHost*) = 0;
  virtual void OnPeerConnectionRemovedForTesting(content::RenderFrameHost*) = 0;

  // Set the possibly empty list of origins to exclude from transcription.
  virtual void SetExcludedOrigins(
      const std::vector<url::Origin>& excluded_origins) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
