// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace gfx {
class Image;
}

namespace policy {

// This class gives information related to the browser's management state.
// For more imformation please read
// //components/policy/core/common/management/management_service.md
class BrowserManagementService : public ManagementService, public KeyedService {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;
  ui::ImageModel* GetManagementIcon() override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
 private:
  void UpdateManagementIcon(Profile* profile);
  void SetManagementIcon(const gfx::Image& management_icon);

  PrefChangeRegistrar pref_change_registrar_;
  ui::ImageModel management_icon_;
  base::WeakPtrFactory<BrowserManagementService> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
