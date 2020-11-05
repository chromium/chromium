// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_PATHS_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_PATHS_PROVIDER_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "chromeos/components/scanning/scanning_paths_provider.h"

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {

class ScanningPathsProviderImpl : public ScanningPathsProvider {
 public:
  ScanningPathsProviderImpl();
  ~ScanningPathsProviderImpl() override;

  ScanningPathsProviderImpl(const ScanningPathsProviderImpl&) = delete;
  ScanningPathsProviderImpl& operator=(const ScanningPathsProviderImpl&) =
      delete;

  // ScanningPathsProvider:
  std::string GetBaseNameFromPath(content::WebUI* web_ui,
                                  const base::FilePath& path) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_PATHS_PROVIDER_IMPL_H_
