// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_LACROS_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/common/api/system_display.h"

namespace extensions {

// DisplayInfoProvider used by lacros-chrome that uses crosapi to get
// DisplayUnitInfoList from ash-chrome, and handle potential version skew.
class DisplayInfoProviderLacros : public DisplayInfoProvider {
 public:
  DisplayInfoProviderLacros();
  ~DisplayInfoProviderLacros() override;
  DisplayInfoProviderLacros(const DisplayInfoProviderLacros&) = delete;
  const DisplayInfoProviderLacros& operator=(const DisplayInfoProviderLacros&) =
      delete;

  // DisplayInfoProvider:
  void GetAllDisplaysInfo(
      bool single_unified,
      base::OnceCallback<void(DisplayUnitInfoList)> callback) override;

 private:
  // Receiver for SystemDisplayAsh::GetDisplayUnitInfoList().
  void OnCrosapiResult(
      base::OnceCallback<void(DisplayUnitInfoList)> callback,
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> src_info_list);

  base::WeakPtrFactory<DisplayInfoProviderLacros> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYSTEM_DISPLAY_DISPLAY_INFO_PROVIDER_LACROS_H_
