// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-forward.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/display/display.h"
#include "ui/snapshot/screenshot_grabber.h"

namespace crostini {
enum class CrostiniResult;
}

namespace extensions {

class AssistantInteractionHelper;
class WindowStateChangeObserver;
class WindowBoundsChangeObserver;
class EventGenerator;

class AutotestPrivateInitializeEventsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.initializeEvents",
                             AUTOTESTPRIVATE_INITIALIZEEVENTS)

 private:
  ~AutotestPrivateInitializeEventsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLogoutFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.logout", AUTOTESTPRIVATE_LOGOUT)

 private:
  ~AutotestPrivateLogoutFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRestartFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.restart", AUTOTESTPRIVATE_RESTART)

 private:
  ~AutotestPrivateRestartFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateShutdownFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.shutdown",
                             AUTOTESTPRIVATE_SHUTDOWN)

 private:
  ~AutotestPrivateShutdownFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLoginStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loginStatus",
                             AUTOTESTPRIVATE_LOGINSTATUS)

 private:
  ~AutotestPrivateLoginStatusFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLockScreenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.lockScreen",
                             AUTOTESTPRIVATE_LOCKSCREEN)

 private:
  ~AutotestPrivateLockScreenFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetExtensionsInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getExtensionsInfo",
                             AUTOTESTPRIVATE_GETEXTENSIONSINFO)

 private:
  ~AutotestPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSimulateAsanMemoryBugFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.simulateAsanMemoryBug",
                             AUTOTESTPRIVATE_SIMULATEASANMEMORYBUG)

 private:
  ~AutotestPrivateSimulateAsanMemoryBugFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTouchpadSensitivityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTouchpadSensitivity",
                             AUTOTESTPRIVATE_SETTOUCHPADSENSITIVITY)

 private:
  ~AutotestPrivateSetTouchpadSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapToClickFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapToClick",
                             AUTOTESTPRIVATE_SETTAPTOCLICK)

 private:
  ~AutotestPrivateSetTapToClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetThreeFingerClickFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setThreeFingerClick",
                             AUTOTESTPRIVATE_SETTHREEFINGERCLICK)

 private:
  ~AutotestPrivateSetThreeFingerClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapDraggingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapDragging",
                             AUTOTESTPRIVATE_SETTAPDRAGGING)

 private:
  ~AutotestPrivateSetTapDraggingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetNaturalScrollFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setNaturalScroll",
                             AUTOTESTPRIVATE_SETNATURALSCROLL)

 private:
  ~AutotestPrivateSetNaturalScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseSensitivityFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseSensitivity",
                             AUTOTESTPRIVATE_SETMOUSESENSITIVITY)

 private:
  ~AutotestPrivateSetMouseSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPrimaryButtonRightFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPrimaryButtonRight",
                             AUTOTESTPRIVATE_SETPRIMARYBUTTONRIGHT)

 private:
  ~AutotestPrivateSetPrimaryButtonRightFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseReverseScrollFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseReverseScroll",
                             AUTOTESTPRIVATE_SETMOUSEREVERSESCROLL)

 private:
  ~AutotestPrivateSetMouseReverseScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetVisibleNotificationsFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetVisibleNotificationsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getVisibleNotifications",
                             AUTOTESTPRIVATE_GETVISIBLENOTIFICATIONS)

 private:
  ~AutotestPrivateGetVisibleNotificationsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetPlayStoreStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPlayStoreState",
                             AUTOTESTPRIVATE_GETPLAYSTORESTATE)

 private:
  ~AutotestPrivateGetPlayStoreStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcStartTimeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcStartTime",
                             AUTOTESTPRIVATE_GETARCSTARTTIME)

 private:
  ~AutotestPrivateGetArcStartTimeFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcState",
                             AUTOTESTPRIVATE_GETARCSTATE)

 private:
  ~AutotestPrivateGetArcStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPlayStoreEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPlayStoreEnabled",
                             AUTOTESTPRIVATE_SETPLAYSTOREENABLED)

 private:
  ~AutotestPrivateSetPlayStoreEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateIsAppShownFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isAppShown",
                             AUTOTESTPRIVATE_ISAPPSHOWN)

 private:
  ~AutotestPrivateIsAppShownFunction() override;
  ResponseAction Run() override;
};

// Deprecated, use GetArcState instead.
class AutotestPrivateIsArcProvisionedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isArcProvisioned",
                             AUTOTESTPRIVATE_ISARCPROVISIONED)

 private:
  ~AutotestPrivateIsArcProvisionedFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcApp",
                             AUTOTESTPRIVATE_GETARCAPP)

 private:
  ~AutotestPrivateGetArcAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcPackageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcPackage",
                             AUTOTESTPRIVATE_GETARCPACKAGE)

 private:
  ~AutotestPrivateGetArcPackageFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateWaitForSystemWebAppsInstallFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateWaitForSystemWebAppsInstallFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autotestPrivate.waitForSystemWebAppsInstall",
      AUTOTESTPRIVATE_WAITFORSYSTEMWEBAPPSINSTALLFUNCTION)

 private:
  ~AutotestPrivateWaitForSystemWebAppsInstallFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetRegisteredSystemWebAppsFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetRegisteredSystemWebAppsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getRegisteredSystemWebApps",
                             AUTOTESTPRIVATE_GETREGISTEREDSYSTEMWEBAPPSFUNCTION)

 private:
  ~AutotestPrivateGetRegisteredSystemWebAppsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchArcAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchArcApp",
                             AUTOTESTPRIVATE_LAUNCHARCAPP)

 private:
  ~AutotestPrivateLaunchArcAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchApp",
                             AUTOTESTPRIVATE_LAUNCHAPP)

 private:
  ~AutotestPrivateLaunchAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchSystemWebAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchSystemWebApp",
                             AUTOTESTPRIVATE_LAUNCHSYSTEMWEBAPP)

 private:
  ~AutotestPrivateLaunchSystemWebAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateCloseAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.closeApp",
                             AUTOTESTPRIVATE_CLOSEAPP)

 private:
  ~AutotestPrivateCloseAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetClipboardTextDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getClipboardTextData",
                             AUTOTESTPRIVATE_GETCLIPBOARDTEXTDATA)

 private:
  ~AutotestPrivateGetClipboardTextDataFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetClipboardTextDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setClipboardTextData",
                             AUTOTESTPRIVATE_SETCLIPBOARDTEXTDATA)

 private:
  ~AutotestPrivateSetClipboardTextDataFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetCrostiniEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setCrostiniEnabled",
                             AUTOTESTPRIVATE_SETCROSTINIENABLED)

 private:
  ~AutotestPrivateSetCrostiniEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRunCrostiniInstallerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniInstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniInstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRestarted(crostini::CrostiniResult);
};

class AutotestPrivateRunCrostiniUninstallerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniUninstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIUNINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniUninstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRemoved(crostini::CrostiniResult);
};

class AutotestPrivateExportCrostiniFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.exportCrostini",
                             AUTOTESTPRIVATE_EXPORTCROSTINI)

 private:
  ~AutotestPrivateExportCrostiniFunction() override;
  ResponseAction Run() override;

  void CrostiniExported(crostini::CrostiniResult);
};

class AutotestPrivateImportCrostiniFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.importCrostini",
                             AUTOTESTPRIVATE_IMPORTCROSTINI)

 private:
  ~AutotestPrivateImportCrostiniFunction() override;
  ResponseAction Run() override;

  void CrostiniImported(crostini::CrostiniResult);
};

class AutotestPrivateSetPluginVMPolicyFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPluginVMPolicy",
                             AUTOTESTPRIVATE_SETPLUGINVMPOLICY)

 private:
  ~AutotestPrivateSetPluginVMPolicyFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateShowPluginVMInstallerFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.showPluginVMInstaller",
                             AUTOTESTPRIVATE_SHOWPLUGINVMINSTALLER)

 private:
  ~AutotestPrivateShowPluginVMInstallerFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRegisterComponentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.registerComponent",
                             AUTOTESTPRIVATE_REGISTERCOMPONENT)

 private:
  ~AutotestPrivateRegisterComponentFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateTakeScreenshotFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.takeScreenshot",
                             AUTOTESTPRIVATE_TAKESCREENSHOT)

 private:
  ~AutotestPrivateTakeScreenshotFunction() override;
  ResponseAction Run() override;

  void ScreenshotTaken(std::unique_ptr<ui::ScreenshotGrabber> grabber,
                       ui::ScreenshotResult screenshot_result,
                       scoped_refptr<base::RefCountedMemory> png_data);
};

class AutotestPrivateTakeScreenshotForDisplayFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.takeScreenshotForDisplay",
                             AUTOTESTPRIVATE_TAKESCREENSHOTFORDISPLAY)

 private:
  ~AutotestPrivateTakeScreenshotForDisplayFunction() override;
  ResponseAction Run() override;

  void ScreenshotTaken(std::unique_ptr<ui::ScreenshotGrabber> grabber,
                       ui::ScreenshotResult screenshot_result,
                       scoped_refptr<base::RefCountedMemory> png_data);
};

class AutotestPrivateGetPrinterListFunction
    : public ExtensionFunction,
      public chromeos::CupsPrintersManager::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPrinterList",
                             AUTOTESTPRIVATE_GETPRINTERLIST)
  AutotestPrivateGetPrinterListFunction();

 private:
  ~AutotestPrivateGetPrinterListFunction() override;
  ResponseAction Run() override;

  void DestroyPrintersManager();
  void RespondWithTimeoutError();
  void RespondWithSuccess();

  // chromeos::CupsPrintersManager::Observer
  void OnEnterprisePrintersInitialized() override;

  std::unique_ptr<base::Value> results_;
  std::unique_ptr<chromeos::CupsPrintersManager> printers_manager_;
  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateUpdatePrinterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.updatePrinter",
                             AUTOTESTPRIVATE_UPDATEPRINTER)

 private:
  ~AutotestPrivateUpdatePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRemovePrinterFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removePrinter",
                             AUTOTESTPRIVATE_REMOVEPRINTER)

 private:
  ~AutotestPrivateRemovePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetAllEnterprisePoliciesFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAllEnterprisePolicies",
                             AUTOTESTPRIVATE_GETALLENTERPRISEPOLICIES)

 private:
  ~AutotestPrivateGetAllEnterprisePoliciesFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRefreshEnterprisePoliciesFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.refreshEnterprisePolicies",
                             AUTOTESTPRIVATE_REFRESHENTERPRISEPOLICIES)

 private:
  ~AutotestPrivateRefreshEnterprisePoliciesFunction() override;
  ResponseAction Run() override;

  // Called once all the policies have been refreshed.
  void RefreshDone();
};

class AutotestPrivateBootstrapMachineLearningServiceFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateBootstrapMachineLearningServiceFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.bootstrapMachineLearningService",
                             AUTOTESTPRIVATE_BOOTSTRAPMACHINELEARNINGSERVICE)

 private:
  ~AutotestPrivateBootstrapMachineLearningServiceFunction() override;
  ResponseAction Run() override;

  // Callbacks for a basic Mojo call to MachineLearningService.LoadModel.
  void ModelLoaded(chromeos::machine_learning::mojom::LoadModelResult result);
  void OnMojoDisconnect();

  mojo::Remote<chromeos::machine_learning::mojom::Model> model_;
};

// Enable/disable the Google Assistant feature. This toggles the Assistant user
// pref which will indirectly bring up or shut down the Assistant service.
class AutotestPrivateSetAssistantEnabledFunction
    : public ExtensionFunction,
      public ash::AssistantStateObserver {
 public:
  AutotestPrivateSetAssistantEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setAssistantEnabled",
                             AUTOTESTPRIVATE_SETASSISTANTENABLED)

 private:
  ~AutotestPrivateSetAssistantEnabledFunction() override;
  ResponseAction Run() override;

  // ash::AssistantStateObserver overrides:
  void OnAssistantStatusChanged(
      chromeos::assistant::AssistantStatus status) override;

  // Called when the Assistant service does not respond in a timely fashion. We
  // will respond with an error.
  void Timeout();

  base::Optional<bool> enabled_;
  base::OneShotTimer timeout_timer_;
};

// Bring up the Assistant service, and wait until the ready signal is received.
class AutotestPrivateEnableAssistantAndWaitForReadyFunction
    : public ExtensionFunction,
      public ash::AssistantStateObserver {
 public:
  AutotestPrivateEnableAssistantAndWaitForReadyFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.enableAssistantAndWaitForReady",
                             AUTOTESTPRIVATE_ENABLEASSISTANTANDWAITFORREADY)

 private:
  ~AutotestPrivateEnableAssistantAndWaitForReadyFunction() override;
  ResponseAction Run() override;

  void SubscribeToStatusChanges();

  // ash::AssistantStateObserver overrides:
  void OnAssistantStatusChanged(
      chromeos::assistant::AssistantStatus status) override;

  // A reference to keep |this| alive while waiting for the Assistant to
  // respond.
  scoped_refptr<ExtensionFunction> self_;
};

// Send text query to Assistant and return response.
class AutotestPrivateSendAssistantTextQueryFunction : public ExtensionFunction {
 public:
  AutotestPrivateSendAssistantTextQueryFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.sendAssistantTextQuery",
                             AUTOTESTPRIVATE_SENDASSISTANTTEXTQUERY)

 private:
  ~AutotestPrivateSendAssistantTextQueryFunction() override;
  ResponseAction Run() override;

  // Called when the interaction finished with non-empty response.
  void OnInteractionFinishedCallback(const base::Optional<std::string>& error);

  // Called when Assistant service fails to respond in a certain amount of
  // time. We will respond with an error.
  void Timeout();

  std::unique_ptr<AssistantInteractionHelper> interaction_helper_;
  base::OneShotTimer timeout_timer_;
};

// Wait for the next text/voice query interaction completed and respond with
// the query status if any valid response was caught before time out.
class AutotestPrivateWaitForAssistantQueryStatusFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateWaitForAssistantQueryStatusFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForAssistantQueryStatus",
                             AUTOTESTPRIVATE_WAITFORASSISTANTQUERYSTATUS)

 private:
  ~AutotestPrivateWaitForAssistantQueryStatusFunction() override;
  ResponseAction Run() override;

  // Called when the current interaction finished with non-empty response.
  void OnInteractionFinishedCallback(const base::Optional<std::string>& error);

  // Called when Assistant service fails to respond in a certain amount of
  // time. We will respond with an error.
  void Timeout();

  std::unique_ptr<AssistantInteractionHelper> interaction_helper_;
  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateIsArcPackageListInitialRefreshedFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateIsArcPackageListInitialRefreshedFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isArcPackageListInitialRefreshed",
                             AUTOTESTPRIVATE_ISARCPACKAGELISTINITIALREFRESHED)

 private:
  ~AutotestPrivateIsArcPackageListInitialRefreshedFunction() override;
  ResponseAction Run() override;
};

// Set user pref value in the pref tree.
class AutotestPrivateSetWhitelistedPrefFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setWhitelistedPref",
                             AUTOTESTPRIVATE_SETWHITELISTEDPREF)

 private:
  ~AutotestPrivateSetWhitelistedPrefFunction() override;
  ResponseAction Run() override;
};

// Enable/disable a Crostini app's "scaled" property.
// When an app is "scaled", it will use low display density.
class AutotestPrivateSetCrostiniAppScaledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setCrostiniAppScaled",
                             AUTOTESTPRIVATE_SETCROSTINIAPPSCALED)
 private:
  ~AutotestPrivateSetCrostiniAppScaledFunction() override;
  ResponseAction Run() override;
};

// The profile-keyed service that manages the autotestPrivate extension API.
class AutotestPrivateAPI : public BrowserContextKeyedAPI,
                           public ui::ClipboardObserver {
 public:
  static BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
  GetFactoryInstance();

  // TODO(achuith): Replace these with a mock object for system calls.
  bool test_mode() const { return test_mode_; }
  void set_test_mode(bool test_mode) { test_mode_ = test_mode; }

 private:
  friend class BrowserContextKeyedAPIFactory<AutotestPrivateAPI>;

  explicit AutotestPrivateAPI(content::BrowserContext* context);
  ~AutotestPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "AutotestPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // ui::ClipboardObserver
  void OnClipboardDataChanged() override;

  ScopedObserver<ui::ClipboardMonitor, ui::ClipboardObserver>
      clipboard_observer_;

  content::BrowserContext* const browser_context_;
  bool test_mode_;  // true for AutotestPrivateApiTest.AutotestPrivate test.
};

// Get the primary display's scale factor.
class AutotestPrivateGetPrimaryDisplayScaleFactorFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPrimaryDisplayScaleFactor",
                             AUTOTESTPRIVATE_GETPRIMARYDISPLAYSCALEFACTOR)
 private:
  ~AutotestPrivateGetPrimaryDisplayScaleFactorFunction() override;
  ResponseAction Run() override;
};

// Returns if tablet mode is enabled.
class AutotestPrivateIsTabletModeEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isTabletModeEnabled",
                             AUTOTESTPRIVATE_ISTABLETMODEENABLED)
 private:
  ~AutotestPrivateIsTabletModeEnabledFunction() override;
  ResponseAction Run() override;
};

// Enables/Disables tablet mode.
class AutotestPrivateSetTabletModeEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTabletModeEnabled",
                             AUTOTESTPRIVATE_SETTABLETMODEENABLED)

 private:
  ~AutotestPrivateSetTabletModeEnabledFunction() override;
  ResponseAction Run() override;
};

// Returns a list of all installed applications
class AutotestPrivateGetAllInstalledAppsFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetAllInstalledAppsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAllInstalledApps",
                             AUTOTESTPRIVATE_GETALLINSTALLEDAPPS)

 private:
  ~AutotestPrivateGetAllInstalledAppsFunction() override;
  ResponseAction Run() override;
};

// Returns a list of all shelf items
class AutotestPrivateGetShelfItemsFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfItemsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfItems",
                             AUTOTESTPRIVATE_GETSHELFITEMS)

 private:
  ~AutotestPrivateGetShelfItemsFunction() override;
  ResponseAction Run() override;
};

// Returns the shelf auto hide behavior.
class AutotestPrivateGetShelfAutoHideBehaviorFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfAutoHideBehaviorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfAutoHideBehavior",
                             AUTOTESTPRIVATE_GETSHELFAUTOHIDEBEHAVIOR)

 private:
  ~AutotestPrivateGetShelfAutoHideBehaviorFunction() override;
  ResponseAction Run() override;
};

// Sets shelf autohide behavior.
class AutotestPrivateSetShelfAutoHideBehaviorFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateSetShelfAutoHideBehaviorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setShelfAutoHideBehavior",
                             AUTOTESTPRIVATE_SETSHELFAUTOHIDEBEHAVIOR)

 private:
  ~AutotestPrivateSetShelfAutoHideBehaviorFunction() override;
  ResponseAction Run() override;
};

// Returns the shelf alignment.
class AutotestPrivateGetShelfAlignmentFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfAlignmentFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfAlignment",
                             AUTOTESTPRIVATE_GETSHELFALIGNMENT)

 private:
  ~AutotestPrivateGetShelfAlignmentFunction() override;
  ResponseAction Run() override;
};

// Sets shelf alignment.
class AutotestPrivateSetShelfAlignmentFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetShelfAlignmentFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setShelfAlignment",
                             AUTOTESTPRIVATE_SETSHELFALIGNMENT)

 private:
  ~AutotestPrivateSetShelfAlignmentFunction() override;
  ResponseAction Run() override;
};

// Waits until overview has finished animating to a certain state.
class AutotestPrivateWaitForOverviewStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateWaitForOverviewStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForOverviewState",
                             AUTOTESTPRIVATE_WAITFOROVERVIEWSTATE)

 private:
  ~AutotestPrivateWaitForOverviewStateFunction() override;
  ResponseAction Run() override;

  // Invoked when the animation has completed. |animation_succeeded| is whether
  // overview is in the target state.
  void Done(bool success);
};

// Returns the overview mode state.
class AutotestPrivateSetOverviewModeStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetOverviewModeStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setOverviewModeState",
                             AUTOTESTPRIVATE_SETOVERVIEWMODESTATE)

 private:
  ~AutotestPrivateSetOverviewModeStateFunction() override;
  ResponseAction Run() override;

  // Called when the overview mode changes.
  void OnOverviewModeChanged(bool for_start, bool finished);
};

class AutotestPrivateShowVirtualKeyboardIfEnabledFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateShowVirtualKeyboardIfEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.showVirtualKeyboardIfEnabled",
                             AUTOTESTPRIVATE_SHOWVIRTUALKEYBOARDIFENABLED)

 private:
  ~AutotestPrivateShowVirtualKeyboardIfEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetArcAppWindowFocusFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetArcAppWindowFocusFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setArcAppWindowFocus",
                             AUTOTESTPRIVATE_SETARCAPPWINDOWFOCUS)

 private:
  ~AutotestPrivateSetArcAppWindowFocusFunction() override;
  ResponseAction Run() override;
};

// Starts ARC app performance tracing for the current ARC app window.
class AutotestPrivateArcAppTracingStartFunction : public ExtensionFunction {
 public:
  AutotestPrivateArcAppTracingStartFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.arcAppTracingStart",
                             AUTOTESTPRIVATE_ARCAPPTRACINGSTART)

 private:
  ~AutotestPrivateArcAppTracingStartFunction() override;
  ResponseAction Run() override;
};

// Stops active ARC app performance tracing if it was active and analyzes
// results. Result is returned to the previously registered callback for
// traceActiveArcAppStart.
class AutotestPrivateArcAppTracingStopAndAnalyzeFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateArcAppTracingStopAndAnalyzeFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.arcAppTracingStopAndAnalyze",
                             AUTOTESTPRIVATE_ARCAPPTRACINGSTOPANDANALYZE)

 private:
  ~AutotestPrivateArcAppTracingStopAndAnalyzeFunction() override;
  ResponseAction Run() override;

  void OnTracingResult(bool success,
                       double fps,
                       double commit_deviation,
                       double render_quality);
};

class AutotestPrivateSwapWindowsInSplitViewFunction : public ExtensionFunction {
 public:
  AutotestPrivateSwapWindowsInSplitViewFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.swapWindowsInSplitView",
                             AUTOTESTPRIVATE_SWAPWINDOWSINSPLITVIEW)

 private:
  ~AutotestPrivateSwapWindowsInSplitViewFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateWaitForDisplayRotationFunction
    : public ExtensionFunction,
      public ash::ScreenRotationAnimatorObserver,
      public ash::ScreenOrientationController::Observer {
 public:
  AutotestPrivateWaitForDisplayRotationFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForDisplayRotation",
                             AUTOTESTPRIVATE_WAITFORDISPLAYROTATION)

  // ash::ScreenRotationAnimatorObserver:
  void OnScreenCopiedBeforeRotation() override;
  void OnScreenRotationAnimationFinished(ash::ScreenRotationAnimator* animator,
                                         bool canceled) override;

  // ash::ScreenOrientationController::Observer:
  void OnUserRotationLockChanged() override;

 private:
  ~AutotestPrivateWaitForDisplayRotationFunction() override;
  ResponseAction Run() override;

  ResponseValue CheckScreenRotationAnimation();

  int64_t display_id_ = display::kInvalidDisplayId;
  base::Optional<display::Display::Rotation> target_rotation_;
  // A reference to keep the instance alive while waiting for rotation.
  scoped_refptr<ExtensionFunction> self_;
};

class AutotestPrivateGetAppWindowListFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetAppWindowListFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAppWindowList",
                             AUTOTESTPRIVATE_GETAPPWINDOWLIST)

 private:
  ~AutotestPrivateGetAppWindowListFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetAppWindowStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetAppWindowStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setAppWindowState",
                             AUTOTESTPRIVATE_SETAPPWINDOWSTATE)

 private:
  ~AutotestPrivateSetAppWindowStateFunction() override;
  ResponseAction Run() override;

  void WindowStateChanged(ash::WindowStateType expected_type, bool success);

  std::unique_ptr<WindowStateChangeObserver> window_state_observer_;
};

class AutotestPrivateCloseAppWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.closeAppWindow",
                             AUTOTESTPRIVATE_CLOSEAPPWINDOW)

 private:
  ~AutotestPrivateCloseAppWindowFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateInstallPWAForCurrentURLFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateInstallPWAForCurrentURLFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.installPWAForCurrentURL",
                             AUTOTESTPRIVATE_INSTALLPWAFORCURRENTURL)

 private:
  class PWABannerObserver;
  class PWARegistrarObserver;
  ~AutotestPrivateInstallPWAForCurrentURLFunction() override;
  ResponseAction Run() override;

  // Called when a PWA is loaded from a URL.
  void PWALoaded();
  // Called when a PWA is installed.
  void PWAInstalled(const web_app::AppId& app_id);
  // Called when intalling a PWA times out.
  void PWATimeout();

  std::unique_ptr<PWABannerObserver> banner_observer_;
  std::unique_ptr<PWARegistrarObserver> registrar_observer_;
  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateActivateAcceleratorFunction : public ExtensionFunction {
 public:
  AutotestPrivateActivateAcceleratorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.activateAccelerator",
                             AUTOTESTPRIVATE_ACTIVATEACCELERATOR)

 private:
  ~AutotestPrivateActivateAcceleratorFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateWaitForLauncherStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateWaitForLauncherStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForLauncherState",
                             AUTOTESTPRIVATE_WAITFORLAUNCHERSTATE)

 private:
  ~AutotestPrivateWaitForLauncherStateFunction() override;
  ResponseAction Run() override;

  void Done();
};

class AutotestPrivateCreateNewDeskFunction : public ExtensionFunction {
 public:
  AutotestPrivateCreateNewDeskFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.createNewDesk",
                             AUTOTESTPRIVATE_CREATENEWDESK)

 private:
  ~AutotestPrivateCreateNewDeskFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateActivateDeskAtIndexFunction : public ExtensionFunction {
 public:
  AutotestPrivateActivateDeskAtIndexFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.activateDeskAtIndex",
                             AUTOTESTPRIVATE_ACTIVATEDESKATINDEX)

 private:
  ~AutotestPrivateActivateDeskAtIndexFunction() override;
  ResponseAction Run() override;

  void OnAnimationComplete();
};

class AutotestPrivateRemoveActiveDeskFunction : public ExtensionFunction {
 public:
  AutotestPrivateRemoveActiveDeskFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removeActiveDesk",
                             AUTOTESTPRIVATE_REMOVEACTIVEDESK)

 private:
  ~AutotestPrivateRemoveActiveDeskFunction() override;
  ResponseAction Run() override;

  void OnAnimationComplete();
};

class AutotestPrivateMouseClickFunction : public ExtensionFunction {
 public:
  AutotestPrivateMouseClickFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.mouseClick",
                             AUTOTESTPRIVATE_MOUSECLICK)

 private:
  ~AutotestPrivateMouseClickFunction() override;
  ResponseAction Run() override;

  std::unique_ptr<EventGenerator> event_generator_;
};

class AutotestPrivateMousePressFunction : public ExtensionFunction {
 public:
  AutotestPrivateMousePressFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.mousePress",
                             AUTOTESTPRIVATE_MOUSEPRESS)

 private:
  ~AutotestPrivateMousePressFunction() override;
  ResponseAction Run() override;

  std::unique_ptr<EventGenerator> event_generator_;
};

class AutotestPrivateMouseReleaseFunction : public ExtensionFunction {
 public:
  AutotestPrivateMouseReleaseFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.mouseRelease",
                             AUTOTESTPRIVATE_MOUSERELEASE)

 private:
  ~AutotestPrivateMouseReleaseFunction() override;
  ResponseAction Run() override;

  std::unique_ptr<EventGenerator> event_generator_;
};

class AutotestPrivateMouseMoveFunction : public ExtensionFunction {
 public:
  AutotestPrivateMouseMoveFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.mouseMove",
                             AUTOTESTPRIVATE_MOUSEMOVE)

 private:
  ~AutotestPrivateMouseMoveFunction() override;
  ResponseAction Run() override;

  void OnDone();

  std::unique_ptr<EventGenerator> event_generator_;
};

class AutotestPrivateSetMetricsEnabledFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetMetricsEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMetricsEnabled",
                             AUTOTESTPRIVATE_SETMETRICSENABLED)

 private:
  ~AutotestPrivateSetMetricsEnabledFunction() override;
  ResponseAction Run() override;

  void OnStatsReportingStateChanged();

  std::unique_ptr<chromeos::StatsReportingController::ObserverSubscription>
      stats_reporting_observer_subscription_;
  bool target_value_ = false;
};

class AutotestPrivateStartTracingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStartTracingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.startTracing",
                             AUTOTESTPRIVATE_STARTTRACING)

 private:
  ~AutotestPrivateStartTracingFunction() override;
  ResponseAction Run() override;

  void OnStartTracing();
};

class AutotestPrivateStopTracingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStopTracingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.stopTracing",
                             AUTOTESTPRIVATE_STOPTRACING)

 private:
  ~AutotestPrivateStopTracingFunction() override;
  ResponseAction Run() override;

  void OnTracingComplete(std::unique_ptr<std::string> trace);
};

class AutotestPrivateSetArcTouchModeFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetArcTouchModeFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setArcTouchMode",
                             AUTOTESTPRIVATE_SETARCTOUCHMODE)

 private:
  ~AutotestPrivateSetArcTouchModeFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivatePinShelfIconFunction : public ExtensionFunction {
 public:
  AutotestPrivatePinShelfIconFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.pinShelfIcon",
                             AUTOTESTPRIVATE_PINSHELFICON)
 private:
  ~AutotestPrivatePinShelfIconFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetScrollableShelfInfoForStateFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetScrollableShelfInfoForStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getScrollableShelfInfoForState",
                             AUTOTESTPRIVATE_GETSCROLLABLESHELFINFOFORSTATE)

 private:
  ~AutotestPrivateGetScrollableShelfInfoForStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetShelfUIInfoForStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetShelfUIInfoForStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getShelfUIInfoForState",
                             AUTOTESTPRIVATE_GETSHELFUIINFOFORSTATE)

 private:
  ~AutotestPrivateGetShelfUIInfoForStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetWindowBoundsFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetWindowBoundsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setWindowBounds",
                             AUTOTESTPRIVATE_SETWINDOWBOUNDS)

 private:
  ~AutotestPrivateSetWindowBoundsFunction() override;
  ResponseAction Run() override;

  void WindowBoundsChanged(const gfx::Rect& bounds_in_display,
                           int64_t display_id,
                           bool success);

  std::unique_ptr<WindowBoundsChangeObserver> window_bounds_observer_;
};

class AutotestPrivateStartSmoothnessTrackingFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.startSmoothnessTracking",
                             AUTOTESTPRIVATE_STARTSMOOTHNESSTRACKING)

 private:
  ~AutotestPrivateStartSmoothnessTrackingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStopSmoothnessTrackingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.stopSmoothnessTracking",
                             AUTOTESTPRIVATE_STOPSMOOTHNESSTRACKING)

 private:
  ~AutotestPrivateStopSmoothnessTrackingFunction() override;
  ResponseAction Run() override;

  void OnReportSmoothness(int smoothness);
};

class AutotestPrivateWaitForAmbientPhotoAnimationFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateWaitForAmbientPhotoAnimationFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForAmbientPhotoAnimation",
                             AUTOTESTPRIVATE_WAITFORAMBIENTPHOTOANIMATION)

 private:
  ~AutotestPrivateWaitForAmbientPhotoAnimationFunction() override;
  ResponseAction Run() override;

  // Called when photo transition animations completed.
  void OnPhotoTransitionAnimationCompleted();

  // Called when photo transition animations fail to finish in a certain amount
  // of time. We will respond with an error.
  void Timeout();

  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateDisableSwitchAccessDialogFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateDisableSwitchAccessDialogFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.disableSwitchAccessDialog",
                             AUTOTESTPRIVATE_DISABLESWITCHACCESSDIALOG)

 private:
  ~AutotestPrivateDisableSwitchAccessDialogFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateDisableAutomationFunction : public ExtensionFunction {
 public:
  AutotestPrivateDisableAutomationFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.disableAutomation",
                             AUTOTESTPRIVATE_DISABLEAUTOMATION)

 private:
  ~AutotestPrivateDisableAutomationFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStartThroughputTrackerDataCollectionFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateStartThroughputTrackerDataCollectionFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autotestPrivate.startThroughputTrackerDataCollection",
      AUTOTESTPRIVATE_STARTTHROUGHPUTTRACKERDATACOLLECTION)

 private:
  ~AutotestPrivateStartThroughputTrackerDataCollectionFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStopThroughputTrackerDataCollectionFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateStopThroughputTrackerDataCollectionFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autotestPrivate.stopThroughputTrackerDataCollection",
      AUTOTESTPRIVATE_STOPTHROUGHPUTTRACKERDATACOLLECTION)

 private:
  ~AutotestPrivateStopThroughputTrackerDataCollectionFunction() override;
  ResponseAction Run() override;
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
