// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_CHROME_BROWSER_MAIN_EXTRA_PARTS_GPU_H_
#define CHROME_BROWSER_GPU_CHROME_BROWSER_MAIN_EXTRA_PARTS_GPU_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {
class GpuDataManagerObserver;
}

class ChromeBrowserMainExtraPartsGpu : public ChromeBrowserMainExtraParts,
                                       public content::GpuDataManagerObserver {
 public:
  ChromeBrowserMainExtraPartsGpu();
  ~ChromeBrowserMainExtraPartsGpu() override;

  ChromeBrowserMainExtraPartsGpu(const ChromeBrowserMainExtraPartsGpu&) =
      delete;
  ChromeBrowserMainExtraPartsGpu& operator=(
      const ChromeBrowserMainExtraPartsGpu&) = delete;

  // ChromeBrowserMainExtraParts:
  void PreCreateThreads() override;

  // content::GpuDataManagerObserver:
  void OnGpuInfoUpdate() override;

 private:
  const char* GetSkiaBackendName() const;
};

#endif  // CHROME_BROWSER_GPU_CHROME_BROWSER_MAIN_EXTRA_PARTS_GPU_H_
