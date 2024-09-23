// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_

#include "base/auto_reset.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/gfx/native_widget_types.h"

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
  // An action to take for a permissions prompt, if any. This allows tests to
  // override prompt behavior.
  enum class DialogAction {
    // The dialog will show normally.
    kDefault,
    // The dialog will not show and the grant will be auto-accepted.
    kAutoConfirm,
    // The dialog will not show and the grant will be auto-rejected.
    kAutoReject,
    // The dialog will not show and the grant can be resolved via
    // the `ResolvePendingDialogForTests()` method.
    kProgrammatic,
  };

  DECLARE_EXTENSION_FUNCTION("permissions.request", PERMISSIONS_REQUEST)

  PermissionsRequestFunction();

  PermissionsRequestFunction(const PermissionsRequestFunction&) = delete;
  PermissionsRequestFunction& operator=(const PermissionsRequestFunction&) =
      delete;

  // FOR TESTS ONLY to bypass the confirmation UI.
  [[nodiscard]] static base::AutoReset<DialogAction> SetDialogActionForTests(
      DialogAction dialog_action);

  // The callback fired when the `DialogAction` is `kProgrammatic`.
  using ShowDialogCallback = base::RepeatingCallback<void(gfx::NativeWindow)>;

  [[nodiscard]] static base::AutoReset<ShowDialogCallback*>
  SetShowDialogCallbackForTests(ShowDialogCallback* callback);

  static void ResolvePendingDialogForTests(bool accept_dialog);
  static void SetIgnoreUserGestureForTests(bool ignore);

  // Returns the set of permissions that the user was prompted for, if any.
  std::unique_ptr<const PermissionSet> TakePromptedPermissionsForTesting();

 protected:
  ~PermissionsRequestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  bool ShouldKeepWorkerAliveIndefinitely() override;

 private:
  void OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);
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
};

// chrome.permissions.addSiteAccessRequest
class PermissionsAddSiteAccessRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.addSiteAccessRequest",
                             PERMISSIONS_ADDSITEACCESSREQUEST)

 protected:
  ~PermissionsAddSiteAccessRequestFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// chrome.permissions.removeSiteAccessRequest
class PermissionsRemoveSiteAccessRequestFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("permissions.removeSiteAccessRequest",
                             PERMISSIONS_REMOVESITEACCESSREQUEST)

 protected:
  ~PermissionsRemoveSiteAccessRequestFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_API_H_
