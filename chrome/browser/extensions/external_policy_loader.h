// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_loader.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace extensions {

// A specialization of the ExternalProvider that uses extension management
// policies to look up which external extensions are registered.
class ExternalPolicyLoader : public ExternalLoader,
                             public ExtensionManagement::Observer {
 public:
  // Indicates the policies for installed extensions from this class, according
  // to management polices.
  enum InstallationType {
    // Installed extensions are not allowed to be disabled or removed.
    FORCED,
    // Installed extensions are allowed to be disabled but not removed.
    RECOMMENDED
  };

  ExternalPolicyLoader(Profile* profile,
                       ExtensionManagement* settings,
                       InstallationType type);

  // ExtensionManagement::Observer implementation
  void OnExtensionManagementSettingsChanged() override;

  // Adds an extension to be updated to the pref dictionary.
  static void AddExtension(base::DictionaryValue* dict,
                           const std::string& extension_id,
                           const std::string& update_url);

 protected:
  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  ~ExternalPolicyLoader() override;

  Profile* profile_;
  ExtensionManagement* settings_;
  InstallationType type_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPolicyLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_POLICY_LOADER_H_
