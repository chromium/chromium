// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/webstore_install_helper.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/api/webstore_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/browser/active_install_data.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"
#endif  // !BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace content {
class GpuFeatureChecker;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;
struct InstallApproval;
class ScopedActiveInstall;

class WebstorePrivateApi {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnExtensionInstallSuccess(const std::string& id) {}
    virtual void OnExtensionInstallFailure(
        const std::string& id,
        const std::string& error,
        WebstoreInstaller::FailureReason reason) {}
  };

  // Sets a delegate for testing.
  static base::AutoReset<Delegate*> SetDelegateForTesting(Delegate* delegate);

  // Gets the pending approval for the `extension_id` in `profile`. Pending
  // approvals are held between the calls to beginInstallWithManifest and
  // completeInstall. This should only be used for testing.
  static std::unique_ptr<InstallApproval> PopApprovalForTesting(
      Profile* profile,
      const std::string& extension_id);

  // Clear the pending approvals. This should only be used for testing.
  static void ClearPendingApprovalsForTesting();

  // Get the count of pending approvals. This should only be used for testing.
  static int GetPendingApprovalsCountForTesting();
};

class WebstorePrivateBeginInstallWithManifest3Function
    : public ExtensionFunction,
      public WebstoreInstallHelper::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.beginInstallWithManifest3",
                             WEBSTOREPRIVATE_BEGININSTALLWITHMANIFEST3)

  WebstorePrivateBeginInstallWithManifest3Function();

  std::u16string GetBlockedByPolicyErrorMessageForTesting() const;
  bool GetFrictionDialogShownForTesting() const {
    return friction_dialog_shown_;
  }

 private:
  using Params = api::webstore_private::BeginInstallWithManifest3::Params;

  ~WebstorePrivateBeginInstallWithManifest3Function() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstallHelper::Delegate:
  void OnWebstoreParseSuccess(const std::string& id,
                              const SkBitmap& icon,
                              base::DictValue parsed_manifest) override;
  void OnWebstoreParseFailure(const std::string& id,
                              InstallHelperResultCode result,
                              const std::string& error_message) override;

  // Handles the result of GetWebstoreExtensionInstallStatus.
  void OnInstallStatusCheckDone(
      extensions::ExtensionInstallStatus install_status);

  void RequestExtensionApproval(content::WebContents* web_contents);

#if BUILDFLAG(IS_ANDROID)
  // Called when the parent access flow on Android desktop completes to show the
  // browser parent approval permission dialog.
  void OnParentAuthenticationDone(content::WebContents* web_contents,
                                  SupervisedExtensionApprovalResult result);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Handles the result of the extension approval flow.
  void OnExtensionApprovalDone(SupervisedExtensionApprovalResult result);

  void OnExtensionApprovalApproved();

  void OnExtensionApprovalCanceled();

  void OnExtensionApprovalFailed();

  void OnExtensionApprovalBlocked();

  // Returns true if the parental approval prompt was shown, false if there was
  // an error showing it.
  bool PromptForParentApproval();

  void OnFrictionPromptDone(bool result);
  void OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);
  void OnRequestPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);
  void OnBlockByPolicyPromptDone();
  void OnRequestParentApprovalPromptCancelled();

  // Permissions are granted by default.
  void HandleInstallProceed(bool withhold_permissions = false);
  void HandleInstallAbort(bool user_initiated);

  ExtensionFunction::ResponseValue BuildResponse(
      api::webstore_private::Result result,
      const std::string& error);

  bool ShouldShowFrictionDialog(Profile* profile);
  void ShowInstallFrictionDialog(content::WebContents* contents);
  void ShowInstallDialog(content::WebContents* contents);

  // Shows block dialog when `extension` is blocked by policy on the Window that
  // `contents` belongs to. `done_callback` will be invoked once the dialog is
  // closed by user.
  // Custom error message will be appended if it's set by the policy.
  void ShowBlockedByPolicyDialog(const Extension* extension,
                                 const SkBitmap& icon,
                                 content::WebContents* contents,
                                 base::OnceClosure done_callback);

  // Adds friction accepted events to Safe Browsing metrics collector for
  // further metrics logging. Called when a user decides to accept the friction
  // prompt. Note that the extension may not be eventually installed.
  void ReportFrictionAcceptedEvent();

  const Params::Details& details() const { return params_->details; }

  std::optional<Params> params_;

  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<ScopedActiveInstall> scoped_active_install_;

  std::optional<base::DictValue> parsed_manifest_;
  SkBitmap icon_;

  // A dummy Extension object we create for the purposes of using
  // ExtensionInstallPrompt to prompt for confirmation of the install.
  scoped_refptr<Extension> dummy_extension_;

  std::u16string blocked_by_policy_error_message_;

  SupervisedUserExtensionsMetricsRecorder
      supervised_user_extensions_metrics_recorder_;

  std::unique_ptr<ExtensionInstallPrompt> install_prompt_;

  bool friction_dialog_shown_ = false;
};

class WebstorePrivateCompleteInstallFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.completeInstall",
                             WEBSTOREPRIVATE_COMPLETEINSTALL)

  WebstorePrivateCompleteInstallFunction();

 private:
  ~WebstorePrivateCompleteInstallFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // WebstoreInstaller::Delegate callbacks
  void OnExtensionInstallSuccess(const std::string& id);
  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error,
                                 WebstoreInstaller::FailureReason reason);

  void OnInstallSuccess(const std::string& id);

  std::unique_ptr<InstallApproval> approval_;
  std::unique_ptr<ScopedActiveInstall> scoped_active_install_;
};

class WebstorePrivateEnableAppLauncherFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.enableAppLauncher",
                             WEBSTOREPRIVATE_ENABLEAPPLAUNCHER)

  WebstorePrivateEnableAppLauncherFunction();

 private:
  ~WebstorePrivateEnableAppLauncherFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetBrowserLoginFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getBrowserLogin",
                             WEBSTOREPRIVATE_GETBROWSERLOGIN)

  WebstorePrivateGetBrowserLoginFunction();

 private:
  ~WebstorePrivateGetBrowserLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetStoreLoginFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getStoreLogin",
                             WEBSTOREPRIVATE_GETSTORELOGIN)

  WebstorePrivateGetStoreLoginFunction();

 private:
  ~WebstorePrivateGetStoreLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateSetStoreLoginFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.setStoreLogin",
                             WEBSTOREPRIVATE_SETSTORELOGIN)

  WebstorePrivateSetStoreLoginFunction();

 private:
  ~WebstorePrivateSetStoreLoginFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetWebGLStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getWebGLStatus",
                             WEBSTOREPRIVATE_GETWEBGLSTATUS)

  WebstorePrivateGetWebGLStatusFunction();

 private:
  ~WebstorePrivateGetWebGLStatusFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  void OnFeatureCheck(bool feature_allowed);

  scoped_refptr<content::GpuFeatureChecker> feature_checker_;
};

class WebstorePrivateGetIsLauncherEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getIsLauncherEnabled",
                             WEBSTOREPRIVATE_GETISLAUNCHERENABLED)

  WebstorePrivateGetIsLauncherEnabledFunction();

 private:
  ~WebstorePrivateGetIsLauncherEnabledFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  void OnIsLauncherCheckCompleted(bool is_enabled);
};

class WebstorePrivateIsInIncognitoModeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.isInIncognitoMode",
                             WEBSTOREPRIVATE_ISININCOGNITOMODEFUNCTION)

  WebstorePrivateIsInIncognitoModeFunction();

 private:
  ~WebstorePrivateIsInIncognitoModeFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateIsPendingCustodianApprovalFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.isPendingCustodianApproval",
                             WEBSTOREPRIVATE_ISPENDINGCUSTODIANAPPROVAL)

  WebstorePrivateIsPendingCustodianApprovalFunction();

 private:
  ~WebstorePrivateIsPendingCustodianApprovalFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  ExtensionFunction::ResponseValue BuildResponse(bool result);
};

class WebstorePrivateGetReferrerChainFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getReferrerChain",
                             WEBSTOREPRIVATE_GETREFERRERCHAIN)

  WebstorePrivateGetReferrerChainFunction();

  WebstorePrivateGetReferrerChainFunction(
      const WebstorePrivateGetReferrerChainFunction&) = delete;
  WebstorePrivateGetReferrerChainFunction& operator=(
      const WebstorePrivateGetReferrerChainFunction&) = delete;

 private:
  ~WebstorePrivateGetReferrerChainFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetExtensionStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getExtensionStatus",
                             WEBSTOREPRIVATE_GETEXTENSIONSTATUS)

  WebstorePrivateGetExtensionStatusFunction();

  WebstorePrivateGetExtensionStatusFunction(
      const WebstorePrivateGetExtensionStatusFunction&) = delete;
  WebstorePrivateGetExtensionStatusFunction& operator=(
      const WebstorePrivateGetExtensionStatusFunction&) = delete;

 private:
  ~WebstorePrivateGetExtensionStatusFunction() override;

  ExtensionFunction::ResponseValue BuildResponseWithoutManifest(
      const ExtensionId& extension_id);
  void OnManifestParsed(const ExtensionId& extension_id,
                        data_decoder::DataDecoder::ValueOrError result);
  void OnInstallStatusCheckDone(ExtensionInstallStatus status);

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetFullChromeVersionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getFullChromeVersion",
                             WEBSTOREPRIVATE_GETFULLCHROMEVERSION)

  WebstorePrivateGetFullChromeVersionFunction();

  WebstorePrivateGetFullChromeVersionFunction(
      const WebstorePrivateGetFullChromeVersionFunction&) = delete;
  WebstorePrivateGetFullChromeVersionFunction& operator=(
      const WebstorePrivateGetFullChromeVersionFunction&) = delete;

 private:
  ~WebstorePrivateGetFullChromeVersionFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class WebstorePrivateGetMV2DeprecationStatusFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.getMV2DeprecationStatus",
                             WEBSTOREPRIVATE_GETMV2DEPRECATIONSTATUS)

  WebstorePrivateGetMV2DeprecationStatusFunction();

  WebstorePrivateGetMV2DeprecationStatusFunction(
      const WebstorePrivateGetMV2DeprecationStatusFunction&) = delete;
  WebstorePrivateGetMV2DeprecationStatusFunction& operator=(
      const WebstorePrivateGetMV2DeprecationStatusFunction&) = delete;

 private:
  ~WebstorePrivateGetMV2DeprecationStatusFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

#if !BUILDFLAG(IS_ANDROID)
class WebstorePrivateShouldShowEnterprisePromotionBannerFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "webstorePrivate.shouldShowEnterprisePromotionBanner",
      WEBSTOREPRIVATE_SHOULDSHOWENTERPRISEPROMOTIONBANNER)

  WebstorePrivateShouldShowEnterprisePromotionBannerFunction();

  WebstorePrivateShouldShowEnterprisePromotionBannerFunction(
      const WebstorePrivateShouldShowEnterprisePromotionBannerFunction&) =
      delete;
  WebstorePrivateShouldShowEnterprisePromotionBannerFunction& operator=(
      const WebstorePrivateShouldShowEnterprisePromotionBannerFunction&) =
      delete;

  void SetFakePromotionEligibilityCheckerForTesting(
      std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
          checker);

 protected:
  ~WebstorePrivateShouldShowEnterprisePromotionBannerFunction() override;

  ResponseAction Run() override;

  void OnPromotionEligibilityDetermined(
      enterprise_management::GetUserEligiblePromotionsResponse response);

 private:
  std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
      promotion_eligibility_checker_;
};

class WebstorePrivateLogEnterprisePromoShownFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.logEnterprisePromoShown",
                             WEBSTOREPRIVATE_LOGENTERPRISEPROMOSHOWN)

  WebstorePrivateLogEnterprisePromoShownFunction();

  WebstorePrivateLogEnterprisePromoShownFunction(
      const WebstorePrivateLogEnterprisePromoShownFunction&) = delete;
  WebstorePrivateLogEnterprisePromoShownFunction& operator=(
      const WebstorePrivateLogEnterprisePromoShownFunction&) = delete;

 protected:
  ~WebstorePrivateLogEnterprisePromoShownFunction() override = default;

  ResponseAction Run() override;
};

class WebstorePrivateOnEnterprisePromoClickFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webstorePrivate.onEnterprisePromoClick",
                             WEBSTOREPRIVATE_ONENTERPRISEPROMOCLICK)

  WebstorePrivateOnEnterprisePromoClickFunction();

  WebstorePrivateOnEnterprisePromoClickFunction(
      const WebstorePrivateOnEnterprisePromoClickFunction&) = delete;
  WebstorePrivateOnEnterprisePromoClickFunction& operator=(
      const WebstorePrivateOnEnterprisePromoClickFunction&) = delete;

 protected:
  ~WebstorePrivateOnEnterprisePromoClickFunction() override = default;

  ResponseAction Run() override;
};

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_H_
