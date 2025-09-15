// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_PROVIDER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_PROVIDER_H_

#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

namespace glic {

// Interface for classes that provide a sharing manager. Sharing managers can be
// instance-bound, or cross-instance (to retain subscriptions), but this is an
// implementation detail hidden from the consumer via this provider interface.
class GlicSharingManagerProvider {
 public:
  GlicSharingManagerProvider() = default;
  virtual ~GlicSharingManagerProvider() = default;
  GlicSharingManagerProvider(const GlicSharingManagerProvider&) = delete;
  GlicSharingManagerProvider& operator=(const GlicSharingManagerProvider&) =
      delete;

  virtual GlicSharingManager& sharing_manager() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_PROVIDER_H_
