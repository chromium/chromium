// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_
#define CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_

#include <string>

#include "base/functional/callback.h"

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
  std::string twa_package_name_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_WEBAPPS_TWA_PACKAGE_HELPER_H_
