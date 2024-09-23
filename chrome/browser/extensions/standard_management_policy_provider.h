// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

class Extension;
class ExtensionManagement;

// The standard management policy provider, which takes into account the
// extension block/allowlists and admin block/allowlists.
class StandardManagementPolicyProvider : public ManagementPolicy::Provider {
 public:
  explicit StandardManagementPolicyProvider(ExtensionManagement* settings,
                                            Profile* profile);

  ~StandardManagementPolicyProvider() override;

  // ManagementPolicy::Provider implementation.
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override;
  bool UserMayInstall(const Extension* extension,
                      std::u16string* error) const override;
  bool UserMayModifySettings(const Extension* extension,
                             std::u16string* error) const override;
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  std::u16string* error) const override;
  bool MustRemainEnabled(const Extension* extension,
                         std::u16string* error) const override;
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          std::u16string* error) const override;
  bool MustRemainInstalled(const Extension* extension,
                           std::u16string* error) const override;
  bool ShouldForceUninstall(const Extension* extension,
                            std::u16string* error) const override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<ExtensionManagement> settings_;
  bool ReturnLoadError(const extensions::Extension* extension,
                       std::u16string* error) const;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
