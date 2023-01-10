// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_
#define CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#endif

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

// Used to obtain the Android package name of a Trusted Web Activity app window.
class TwaPackageHelper {
 public:
  using GetTwaPackageNameCallback =
      base::OnceCallback<void(const std::string& twa_package_name)>;

  // Pass null `render_frame_host` if the Payment feature is disabled.
  explicit TwaPackageHelper(content::RenderFrameHost* render_frame_host);
  TwaPackageHelper(const TwaPackageHelper&) = delete;
  TwaPackageHelper& operator=(const TwaPackageHelper&) = delete;
  ~TwaPackageHelper();

  // Obtains the Android package name of the Trusted Web Activity that invoked
  // this browser, if any.
  void GetTwaPackageName(GetTwaPackageNameCallback callback) const;

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnGetAssociatedAndroidPackage(
      crosapi::mojom::WebAppAndroidPackagePtr package);

  void GetCachedTwaPackageName(GetTwaPackageNameCallback callback) const;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::string twa_package_name_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::OneShotEvent on_twa_package_name_ready_;
  base::WeakPtrFactory<TwaPackageHelper> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_
