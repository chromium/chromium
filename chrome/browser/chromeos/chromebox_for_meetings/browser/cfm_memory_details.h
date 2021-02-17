// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_MEMORY_DETAILS_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_MEMORY_DETAILS_H_

#include "base/macros.h"
#include "chrome/browser/memory_details.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_browser.mojom.h"
#include "content/public/browser/render_process_host.h"

namespace chromeos {
namespace cfm {

// Log details about all Chrome processes.
class CfmMemoryDetails final : public MemoryDetails {
 public:
  // Collects the memory details asynchronously.
  static void Collect(mojom::CfmBrowser::GetMemoryDetailsCallback callback);

  CfmMemoryDetails(const CfmMemoryDetails&) = delete;
  CfmMemoryDetails& operator=(const CfmMemoryDetails&) = delete;

 private:
  explicit CfmMemoryDetails(
      mojom::CfmBrowser::GetMemoryDetailsCallback callback);
  ~CfmMemoryDetails() override;

  // MemoryDetails overrides:
  void OnDetailsAvailable() override;

  void CollectProcessInformation();
  void CollectExtensionsInformation();
  void UpdateGpuInfo();
  void FinishFetch();

  base::GraphicsMemoryInfoKB gpu_meminfo_;
  std::vector<mojom::ProcessDataPtr> proc_data_list_;
  std::map<base::ProcessId, mojom::ProcessMemoryInformationPtr*>
      proc_mem_info_map_;
  mojom::CfmBrowser::GetMemoryDetailsCallback callback_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_MEMORY_DETAILS_H_
