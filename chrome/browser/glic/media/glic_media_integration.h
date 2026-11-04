// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "url/origin.h"

class Profile;


namespace optimization_guide::proto {
class ContentNode;
} // namespace optimization_guide::proto


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
  static GlicMediaIntegration* GetFor(Profile*);

  // Ensures active WebRTC peer connections are tracked while `profile` is
  // alive, even before `GlicMediaIntegration` is lazily initialized.
  static void EnsurePeerConnectionTrackerInitialized(Profile* profile);

  // Returns true if `web_contents` has an active WebRTC peer connection in any
  // of its frames.
  static bool HasActivePeerConnection(content::WebContents* web_contents);

  // Simulates a peer connection being added or removed on `rfh` with local ID
  // `lid`.
  static void SimulatePeerConnectionAddedForTesting(
      content::RenderFrameHost* rfh,
      int lid = 0);
  static void SimulatePeerConnectionRemovedForTesting(
      content::RenderFrameHost* rfh,
      int lid = 0);

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

  // Returns true if the integration has been initialized.
  virtual bool IsInitializedForTesting() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
