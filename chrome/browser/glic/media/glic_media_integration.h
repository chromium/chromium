// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace optimization_guide {
namespace proto {
class ContentNode;
}  // namespace proto
}  // namespace optimization_guide

namespace content {
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
  // be overwritten.
  virtual void AppendContext(
      content::WebContents* web_contents,
      optimization_guide::proto::ContentNode* context_root) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
