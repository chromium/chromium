// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_

#include <memory>

#include "base/memory/raw_ptr.h"

class Profile;

namespace glic {
class WebUIContentsContainer;

// A pool for pre-warming Glic WebContents.
// This is used to reduce the perceived latency when opening the Glic UI by
// creating a WebContents in the background before it's actually needed.
class GlicWebContentsWarmingPool {
 public:
  explicit GlicWebContentsWarmingPool(Profile* profile);
  ~GlicWebContentsWarmingPool();

  // Retrieves a warmed WebUIContentsContainer from the pool. If no warmed
  // container is available, one will be created and then returned. A new
  // container is then preloaded in the background to replace the taken one.
  std::unique_ptr<WebUIContentsContainer> TakeContainer();
  // Ensures that a WebUIContentsContainer is preloaded. If the existing one is
  // crashed, it will be replaced.
  void EnsurePreload();
  // Shuts down the warming pool and destroys any warmed WebContents.
  void Shutdown();

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<WebUIContentsContainer> warmed_container_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CONTENTS_WARMING_POOL_H_
