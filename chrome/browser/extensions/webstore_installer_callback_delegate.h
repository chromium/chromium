// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_CALLBACK_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_CALLBACK_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/webstore_installer.h"

namespace extensions {

// A utility class for binding callbacks to the WebstoreInstaller::Delegate
// events.
class WebstoreInstallerCallbackDelegate : public WebstoreInstaller::Delegate {
 public:
  using FailureReason = WebstoreInstaller::FailureReason;
  using SuccessCallback = base::OnceCallback<void(const std::string&)>;
  using FailureCallback = base::OnceCallback<
      void(const std::string&, const std::string&, FailureReason)>;

  WebstoreInstallerCallbackDelegate(SuccessCallback success_callback,
                                    FailureCallback failure_callback);
  ~WebstoreInstallerCallbackDelegate() override;

  // WebstoreInstaller::Delegate
  void OnExtensionInstallSuccess(const std::string& id) override;
  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error,
                                 FailureReason reason) override;

 private:
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALLER_CALLBACK_DELEGATE_H_
