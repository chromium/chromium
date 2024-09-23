// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ODFS_CONFIG_PRIVATE_ODFS_CONFIG_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ODFS_CONFIG_PRIVATE_ODFS_CONFIG_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class OdfsConfigPrivateGetMountFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("odfsConfigPrivate.getMount",
                             ODFSCONFIGPRIVATE_GETMOUNT)

  OdfsConfigPrivateGetMountFunction();

  OdfsConfigPrivateGetMountFunction(const OdfsConfigPrivateGetMountFunction&) =
      delete;
  OdfsConfigPrivateGetMountFunction& operator=(
      const OdfsConfigPrivateGetMountFunction&) = delete;

 private:
  ~OdfsConfigPrivateGetMountFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class OdfsConfigPrivateGetAccountRestrictionsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("odfsConfigPrivate.getAccountRestrictions",
                             ODFSCONFIGPRIVATE_GETACCOUNTRESTRICTIONS)

  OdfsConfigPrivateGetAccountRestrictionsFunction();

  OdfsConfigPrivateGetAccountRestrictionsFunction(
      const OdfsConfigPrivateGetAccountRestrictionsFunction&) = delete;
  OdfsConfigPrivateGetAccountRestrictionsFunction& operator=(
      const OdfsConfigPrivateGetAccountRestrictionsFunction&) = delete;

 private:
  ~OdfsConfigPrivateGetAccountRestrictionsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class OdfsConfigPrivateShowAutomatedMountErrorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("odfsConfigPrivate.showAutomatedMountError",
                             ODFSCONFIGPRIVATE_SHOWAUTOMATEDMOUNTERROR)

  OdfsConfigPrivateShowAutomatedMountErrorFunction();

  OdfsConfigPrivateShowAutomatedMountErrorFunction(
      const OdfsConfigPrivateShowAutomatedMountErrorFunction&) = delete;
  OdfsConfigPrivateShowAutomatedMountErrorFunction& operator=(
      const OdfsConfigPrivateShowAutomatedMountErrorFunction&) = delete;

 private:
  ~OdfsConfigPrivateShowAutomatedMountErrorFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class OdfsConfigPrivateIsCloudFileSystemEnabledFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "odfsConfigPrivate.isCloudFileSystemEnabled",
      ODFSCONFIGPRIVATE_ISFILESYSTEMPROVIDERCLOUDFILESYSTEMENABLED)

  OdfsConfigPrivateIsCloudFileSystemEnabledFunction();

  OdfsConfigPrivateIsCloudFileSystemEnabledFunction(
      const OdfsConfigPrivateIsCloudFileSystemEnabledFunction&) = delete;
  OdfsConfigPrivateIsCloudFileSystemEnabledFunction& operator=(
      const OdfsConfigPrivateIsCloudFileSystemEnabledFunction&) = delete;

 private:
  ~OdfsConfigPrivateIsCloudFileSystemEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class OdfsConfigPrivateIsContentCacheEnabledFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "odfsConfigPrivate.isContentCacheEnabled",
      ODFSCONFIGPRIVATE_ISFILESYSTEMPROVIDERCONTENTCACHEENABLED)

  OdfsConfigPrivateIsContentCacheEnabledFunction();

  OdfsConfigPrivateIsContentCacheEnabledFunction(
      const OdfsConfigPrivateIsContentCacheEnabledFunction&) = delete;
  OdfsConfigPrivateIsContentCacheEnabledFunction& operator=(
      const OdfsConfigPrivateIsContentCacheEnabledFunction&) = delete;

 private:
  ~OdfsConfigPrivateIsContentCacheEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ODFS_CONFIG_PRIVATE_ODFS_CONFIG_PRIVATE_API_H_
