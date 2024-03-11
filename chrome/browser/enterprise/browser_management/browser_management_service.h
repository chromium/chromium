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
#include "ui/gfx/image/image.h"

class Profile;

namespace image_fetcher {
struct RequestMetadata;
}

namespace policy {

class BrowserManagementMetadata {
 public:
  explicit BrowserManagementMetadata(Profile* profile);
  ~BrowserManagementMetadata();

  const gfx::Image& GetManagementLogo() const;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
 private:
  void UpdateManagementLogo(Profile* profile);
  void SetManagementLogo(
      const gfx::Image& management_logo,
      const image_fetcher::RequestMetadata& request_metadata);

  PrefChangeRegistrar pref_change_registrar_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  gfx::Image management_logo_;
  base::WeakPtrFactory<BrowserManagementMetadata> weak_ptr_factory_{this};
};

// This class gives information related to the browser's management state.
// For more imformation please read
// //components/policy/core/common/management/management_service.md
class BrowserManagementService : public ManagementService, public KeyedService {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;

  const BrowserManagementMetadata& GetMetadata();

 private:
  BrowserManagementMetadata metadata_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
