// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

// chrome.permissions.contains
class PermissionsContainsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.contains", PERMISSIONS_CONTAINS)

 protected:
  ~PermissionsContainsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// chrome.permissions.getAll
class PermissionsGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.getAll", PERMISSIONS_GETALL)

 protected:
  ~PermissionsGetAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// chrome.permissions.remove
class PermissionsRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.remove", PERMISSIONS_REMOVE)

 protected:
  ~PermissionsRemoveFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// chrome.permissions.request
class PermissionsRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.request", PERMISSIONS_REQUEST)

  PermissionsRequestFunction();

  // FOR TESTS ONLY to bypass the confirmation UI.
  static void SetAutoConfirmForTests(bool should_proceed);
  static void ResetAutoConfirmForTests();
  static void SetIgnoreUserGestureForTests(bool ignore);

  // Returns the set of permissions that the user was prompted for, if any.
  std::unique_ptr<const PermissionSet> TakePromptedPermissionsForTesting();

 protected:
  ~PermissionsRequestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);
  void OnRuntimePermissionsGranted();
  void OnOptionalPermissionsGranted();
  void RespondIfRequestsFinished();

  std::unique_ptr<ExtensionInstallPrompt> install_ui_;

  // Requested permissions that are currently withheld.
  std::unique_ptr<const PermissionSet> requested_withheld_;
  // Requested permissions that are currently optional, and not granted.
  std::unique_ptr<const PermissionSet> requested_optional_;

  bool requesting_withheld_permissions_ = false;
  bool requesting_optional_permissions_ = false;

  // The permissions, if any, that Chrome would prompt the user for. This will
  // be recorded if and only if the prompt is being bypassed for a test (see
  // also SetAutoConfirmForTests()).
  std::unique_ptr<const PermissionSet> prompted_permissions_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsRequestFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
