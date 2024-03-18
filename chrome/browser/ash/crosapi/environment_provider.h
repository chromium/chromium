// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_

#include "base/no_destructor.h"
#include "chromeos/crosapi/mojom/crosapi.mojom-forward.h"

namespace crosapi {

// Provides ash-chrome specific flags/configurations (like session type).
class EnvironmentProvider {
 public:
  static EnvironmentProvider* Get();

  EnvironmentProvider(const EnvironmentProvider&) = delete;
  EnvironmentProvider& operator=(const EnvironmentProvider&) = delete;

  // Returns the default paths, such as Downloads, Documents (MyFiles) and the
  // mount point for Drive. These are provided by ash because they are part of
  // the device account, not the Lacros profile.
  crosapi::mojom::DefaultPathsPtr GetDefaultPaths();

 private:
  friend class base::NoDestructor<EnvironmentProvider>;

  EnvironmentProvider();
  ~EnvironmentProvider();
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ENVIRONMENT_PROVIDER_H_
