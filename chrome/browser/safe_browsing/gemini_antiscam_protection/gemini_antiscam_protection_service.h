// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

class GeminiAntiscamProtectionService : public KeyedService {
 public:
  explicit GeminiAntiscamProtectionService(
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ~GeminiAntiscamProtectionService() override;

 private:
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  base::WeakPtrFactory<GeminiAntiscamProtectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_GEMINI_ANTISCAM_PROTECTION_GEMINI_ANTISCAM_PROTECTION_SERVICE_H_
