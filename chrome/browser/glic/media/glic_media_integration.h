// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/glic/host/glic.mojom.h"

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

  // This may call back synchronously.
  virtual void ComputeContext(content::WebContents*,
                              size_t max_size_bytes,
                              ContextCallback) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_INTEGRATION_H_
