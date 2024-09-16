// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom-forward.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom-forward.h"
#include "services/viz/privileged/mojom/compositing/frame_sinks_metrics_recorder.mojom-forward.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/display/display.h"
#include "ui/snapshot/screenshot_grabber.h"

class GoogleServiceAuthError;

namespace ash {
class UserContext;
}

namespace crostini {
enum class CrostiniResult;
}

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace update_client {
enum class Error;
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

class AutotestPrivateWaitForLoginAnimationEndFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForLoginAnimationEnd",
                             AUTOTESTPRIVATE_WAITFORLOGINANIMATIONEND)

 private:
  ~AutotestPrivateWaitForLoginAnimationEndFunction() override;
  ResponseAction Run() override;

  void OnLoginAnimationEnd();
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

class AutotestPrivateRemoveAllNotificationsFunction : public ExtensionFunction {
 public:
  AutotestPrivateRemoveAllNotificationsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removeAllNotifications",
                             AUTOTESTPRIVATE_REMOVEALLNOTIFICATIONS)

 private:
  ~AutotestPrivateRemoveAllNotificationsFunction() override;
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

class AutotestPrivateGetLacrosInfoFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getLacrosInfo",
                             AUTOTESTPRIVATE_GETLACROSINFO)

 private:
  ~AutotestPrivateGetLacrosInfoFunction() override;
  ResponseAction Run() override;
  static api::autotest_private::LacrosState ToLacrosState(
      crosapi::BrowserManager::State state);
  static api::autotest_private::LacrosMode ToLacrosMode(bool is_enabled);
};

class AutotestPrivateGetArcAppFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcApp",
                             AUTOTESTPRIVATE_GETARCAPP)

 private:
  ~AutotestPrivateGetArcAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcAppKillsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcAppKills",
                             AUTOTESTPRIVATE_GETARCAPPKILLS)

 private:
  ~AutotestPrivateGetArcAppKillsFunction() override;
  ResponseAction Run() override;
  void OnKillCounts(arc::mojom::LowMemoryKillCountsPtr counts);
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

  void OnSystemWebAppsInstalled();
};

class AutotestPrivateIsSystemWebAppOpenFunction : public ExtensionFunction {
 public:
  AutotestPrivateIsSystemWebAppOpenFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isSystemWebAppOpen",
                             AUTOTESTPRIVATE_ISSYSTEMWEBAPPOPENFUNCTION)

 private:
  ~AutotestPrivateIsSystemWebAppOpenFunction() override;
  ResponseAction Run() override;

  void OnSystemWebAppsInstalled();
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

  void OnSystemWebAppsInstalled();
};

class AutotestPrivateLaunchFilesAppToPathFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchFilesAppToPath",
                             AUTOTESTPRIVATE_LAUNCHFILESAPPTOPATH)

 private:
  ~AutotestPrivateLaunchFilesAppToPathFunction() override;
  ResponseAction Run() override;
  void OnShowItemInFolder(platform_util::OpenOperationResult result);
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

class AutotestPrivateSetClipboardTextDataFunction
    : public ExtensionFunction,
      public ui::ClipboardObserver {
 public:
  AutotestPrivateSetClipboardTextDataFunction();

  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setClipboardTextData",
                             AUTOTESTPRIVATE_SETCLIPBOARDTEXTDATA)

 private:
  ~AutotestPrivateSetClipboardTextDataFunction() override;
  ResponseAction Run() override;

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

  base::ScopedObservation<ui::ClipboardMonitor, ui::ClipboardObserver>
      observation_{this};
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

class AutotestPrivateCouldAllowCrostiniFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.couldAllowCrostini",
                             AUTOTESTPRIVATE_COULDALLOWCROSTINI)

 private:
  ~AutotestPrivateCouldAllowCrostiniFunction() override;
  ResponseAction Run() override;
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

class AutotestPrivateInstallBorealisFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.installBorealis",
                             AUTOTESTPRIVATE_INSTALLBOREALIS)
  AutotestPrivateInstallBorealisFunction();

 private:
  class InstallationObserver;

  ~AutotestPrivateInstallBorealisFunction() override;
  ResponseAction Run() override;

  void Complete(std::string error_or_empty);

  std::unique_ptr<InstallationObserver> installation_observer_;
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
      public ash::CupsPrintersManager::Observer {
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

  // ash::CupsPrintersManager::Observer
  void OnEnterprisePrintersInitialized() override;

  base::Value::List results_;
  std::unique_ptr<ash::CupsPrintersManager> printers_manager_;
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

class AutotestPrivateRefreshRemoteCommandsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.refreshRemoteCommands",
                             AUTOTESTPRIVATE_REFRESHREMOTECOMMANDS)

 private:
  ~AutotestPrivateRefreshRemoteCommandsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLoadSmartDimComponentFunction : public ExtensionFunction {
 public:
  AutotestPrivateLoadSmartDimComponentFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loadSmartDimComponent",
                             AUTOTESTPRIVATE_LOADSMARTDIMCOMPONENT)

 private:
  ~AutotestPrivateLoadSmartDimComponentFunction() override;
  // ExtensionFunction:
  ResponseAction Run() override;

  void OnComponentUpdatedCallback(update_client::Error error);
  void TryRespond();

  base::RetainingOneShotTimer timer_;
  int timer_triggered_count_ = 0;
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
      ash::assistant::AssistantStatus status) override;

  // Called when the Assistant service does not respond in a timely fashion. We
  // will respond with an error.
  void Timeout();

  std::optional<bool> enabled_;
  base::OneShotTimer timeout_timer_;
};

// Bring up the Assistant service, and wait until the ready signal is received.
class AutotestPrivateEnableAssistantAndWaitForReadyFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateEnableAssistantAndWaitForReadyFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.enableAssistantAndWaitForReady",
                             AUTOTESTPRIVATE_ENABLEASSISTANTANDWAITFORREADY)

 private:
  ~AutotestPrivateEnableAssistantAndWaitForReadyFunction() override;
  ResponseAction Run() override;

  void OnInitializedInternal();
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
  void OnInteractionFinishedCallback(const std::optional<std::string>& error);

  // Called when Assistant service fails to respond in a certain amount of
  // time. We will respond with an error.
  void Timeout();

  // Convert session_manager::SessionState to string for error logging.
  std::string ToString(session_manager::SessionState session_state);

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
  void OnInteractionFinishedCallback(const std::optional<std::string>& error);

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
class AutotestPrivateSetAllowedPrefFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setAllowedPref",
                             AUTOTESTPRIVATE_SETALLOWEDPREF)

 private:
  ~AutotestPrivateSetAllowedPrefFunction() override;
  ResponseAction Run() override;
};

// Clear user pref value in the pref tree.
class AutotestPrivateClearAllowedPrefFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.clearAllowedPref",
                             AUTOTESTPRIVATE_CLEARALLOWEDPREF)

 private:
  ~AutotestPrivateClearAllowedPrefFunction() override;
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

  base::ScopedObservation<ui::ClipboardMonitor, ui::ClipboardObserver>
      clipboard_observation_{this};

  const raw_ptr<content::BrowserContext> browser_context_;
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

// Returns launcher's search box state.
class AutotestPrivateGetLauncherSearchBoxStateFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetLauncherSearchBoxStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getLauncherSearchBoxState",
                             AUTOTESTPRIVATE_GETLAUNCHERSSEARCHBOXSTATE)

 private:
  ~AutotestPrivateGetLauncherSearchBoxStateFunction() override;
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

// Gets the default pinned app IDs for the shelf.
class AutotestPrivateGetDefaultPinnedAppIdsFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetDefaultPinnedAppIdsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getDefaultPinnedAppIds",
                             AUTOTESTPRIVATE_GETDEFAULTPINNEDAPPIDS)

 private:
  ~AutotestPrivateGetDefaultPinnedAppIdsFunction() override;
  ResponseAction Run() override;
};

// Sends the overlay color of the system.
class AutotestPrivateSendArcOverlayColorFunction : public ExtensionFunction {
 public:
  // AutotestPrivateSendArcOverlayColorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.sendArcOverlayColor",
                             AUTOTESTPRIVATE_SENDARCOVERLAYCOLOR)
 private:
  ~AutotestPrivateSendArcOverlayColorFunction() override;
  ResponseAction Run() override;
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

// TODO(crbug.com/40207057): Replace this by introducing
// autotestPrivate.setVirtualKeyboardVisibilityIfEnabled().
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

  std::optional<ResponseValue> CheckScreenRotationAnimation();

  int64_t display_id_ = display::kInvalidDisplayId;
  std::optional<display::Display::Rotation> target_rotation_;
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

  void WindowStateChanged(chromeos::WindowStateType expected_type,
                          bool success);

  std::unique_ptr<WindowStateChangeObserver> window_state_observer_;
};

class AutotestPrivateActivateAppWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.activateAppWindow",
                             AUTOTESTPRIVATE_ACTIVATEAPPWINDOW)

 private:
  ~AutotestPrivateActivateAppWindowFunction() override;
  ResponseAction Run() override;
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
  class PWAInstallManagerObserver;
  ~AutotestPrivateInstallPWAForCurrentURLFunction() override;
  ResponseAction Run() override;

  // Called when a PWA is loaded from a URL.
  void PWALoaded();
  // Called when a PWA is installed.
  void PWAInstalled(const webapps::AppId& app_id);
  // Called when installing a PWA times out.
  void PWATimeout();

  std::unique_ptr<PWABannerObserver> banner_observer_;
  std::unique_ptr<PWAInstallManagerObserver> install_mananger_observer_;
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

class AutotestPrivateActivateAdjacentDesksToTargetIndexFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateActivateAdjacentDesksToTargetIndexFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autotestPrivate.activateAdjacentDesksToTargetIndex",
      AUTOTESTPRIVATE_ACTIVATEADJACENTDESKSTOTARGETINDEX)

 private:
  ~AutotestPrivateActivateAdjacentDesksToTargetIndexFunction() override;
  ResponseAction Run() override;

  void OnAnimationComplete();
};

class AutotestPrivateGetDeskCountFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetDeskCountFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getDeskCount",
                             AUTOTESTPRIVATE_GETDESKCOUNT)

 private:
  ~AutotestPrivateGetDeskCountFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetDesksInfoFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetDesksInfoFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getDesksInfo",
                             AUTOTESTPRIVATE_GETDESKSINFO)

 private:
  ~AutotestPrivateGetDesksInfoFunction() override;
  ResponseAction Run() override;
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

  void OnDeviceSettingsStored();

  bool target_value_ = false;
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

// Deprecated. Use `AutotestPrivateSetShelfIconPinFunction` instead.
class AutotestPrivatePinShelfIconFunction : public ExtensionFunction {
 public:
  AutotestPrivatePinShelfIconFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.pinShelfIcon",
                             AUTOTESTPRIVATE_PINSHELFICON)
 private:
  ~AutotestPrivatePinShelfIconFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetShelfIconPinFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetShelfIconPinFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setShelfIconPin",
                             AUTOTESTPRIVATE_SETSHELFICONPIN)

 private:
  ~AutotestPrivateSetShelfIconPinFunction() override;
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
  // Default sampling interval to collect display smoothness.
  static constexpr base::TimeDelta kDefaultThroughputInterval =
      base::Seconds(5);

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

  void OnReportData(
      base::TimeTicks start_time,
      const cc::FrameSequenceMetrics::CustomReportData& frame_data,
      std::vector<int>&& throughput);
  void OnTimeOut(int64_t display_id);

  base::OneShotTimer timeout_timer_;
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
};

class AutotestPrivateWaitForAmbientVideoFunction : public ExtensionFunction {
 public:
  AutotestPrivateWaitForAmbientVideoFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.waitForAmbientVideo",
                             AUTOTESTPRIVATE_WAITFORAMBIENTVIDEO)

 private:
  ~AutotestPrivateWaitForAmbientVideoFunction() override;
  ResponseAction Run() override;

  void RespondWithSuccess();
  void RespondWithError(std::string error_message);
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

class AutotestPrivateGetThroughputTrackerDataFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetThroughputTrackerDataFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getThroughputTrackerData",
                             AUTOTESTPRIVATE_GETTHROUGHPUTTRACKERDATA)

 private:
  ~AutotestPrivateGetThroughputTrackerDataFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetDisplaySmoothnessFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetDisplaySmoothnessFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getDisplaySmoothness",
                             AUTOTESTPRIVATE_GETDISPLAYSMOOTHNESS)

 private:
  ~AutotestPrivateGetDisplaySmoothnessFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateResetHoldingSpaceFunction : public ExtensionFunction {
 public:
  AutotestPrivateResetHoldingSpaceFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.resetHoldingSpace",
                             AUTOTESTPRIVATE_RESETHOLDINGSPACE)

 private:
  ~AutotestPrivateResetHoldingSpaceFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStartLoginEventRecorderDataCollectionFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateStartLoginEventRecorderDataCollectionFunction();
  DECLARE_EXTENSION_FUNCTION(
      "autotestPrivate.startLoginEventRecorderDataCollection",
      AUTOTESTPRIVATE_STARTLOGINEVENTRECORDERDATACOLLECTION)

 private:
  ~AutotestPrivateStartLoginEventRecorderDataCollectionFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetLoginEventRecorderLoginEventsFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetLoginEventRecorderLoginEventsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getLoginEventRecorderLoginEvents",
                             AUTOTESTPRIVATE_GETLOGINEVENTRECORDERLOGINEVENTS)

 private:
  ~AutotestPrivateGetLoginEventRecorderLoginEventsFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateAddLoginEventForTestingFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateAddLoginEventForTestingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.addLoginEventForTesting",
                             AUTOTESTPRIVATE_ADDLOGINEVENTFORTESTING)

 private:
  ~AutotestPrivateAddLoginEventForTestingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateForceAutoThemeModeFunction : public ExtensionFunction {
 public:
  AutotestPrivateForceAutoThemeModeFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.forceAutoThemeMode",
                             AUTOTESTPRIVATE_FORCEAUTOTHEMEMODE)

 private:
  ~AutotestPrivateForceAutoThemeModeFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetAccessTokenFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetAccessTokenFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getAccessToken",
                             AUTOTESTPRIVATE_GETACCESSTOKEN)

 private:
  ~AutotestPrivateGetAccessTokenFunction() override;
  ResponseAction Run() override;

  void RespondWithTimeoutError();

  void OnAccessToken(GoogleServiceAuthError error,
                     signin::AccessTokenInfo access_token_info);

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  base::OneShotTimer timeout_timer_;
};

class AutotestPrivateIsInputMethodReadyForTestingFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateIsInputMethodReadyForTestingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isInputMethodReadyForTesting",
                             AUTOTESTPRIVATE_ISINPUTMETHODREADYFORTESTING)

 private:
  ~AutotestPrivateIsInputMethodReadyForTestingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateOverrideOrcaResponseForTestingFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateOverrideOrcaResponseForTestingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.overrideOrcaResponseForTesting",
                             AUTOTESTPRIVATE_OVERRIDEORCARESPONSE)

 private:
  ~AutotestPrivateOverrideOrcaResponseForTestingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateMakeFuseboxTempDirFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.makeFuseboxTempDir",
                             AUTOTESTPRIVATE_MAKEFUSEBOXTEMPDIR)

 private:
  ~AutotestPrivateMakeFuseboxTempDirFunction() override;
  ResponseAction Run() override;

  void OnMakeTempDir(const std::string& error_message,
                     const std::string& fusebox_file_path,
                     const std::string& underlying_file_path);
};

class AutotestPrivateRemoveFuseboxTempDirFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removeFuseboxTempDir",
                             AUTOTESTPRIVATE_REMOVEFUSEBOXTEMPDIR)

 private:
  ~AutotestPrivateRemoveFuseboxTempDirFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRemoveComponentExtensionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removeComponentExtension",
                             AUTOTESTPRIVATE_REMOVECOMPONENTEXTENSION)

 private:
  ~AutotestPrivateRemoveComponentExtensionFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStartFrameCountingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStartFrameCountingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.startFrameCounting",
                             AUTOTESTPRIVATE_STARTFRAMECOUNTING)

 private:
  ~AutotestPrivateStartFrameCountingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStopFrameCountingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStopFrameCountingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.stopFrameCounting",
                             AUTOTESTPRIVATE_STOPFRAMECOUNTING)

 private:
  ~AutotestPrivateStopFrameCountingFunction() override;
  ResponseAction Run() override;

  void OnDataReceived(viz::mojom::FrameCountingDataPtr data_ptr);
};

class AutotestPrivateStartOverdrawTrackingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStartOverdrawTrackingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.startOverdrawTracking",
                             AUTOTESTPRIVATE_STARTOVERDRAWTRACKING)

 private:
  ~AutotestPrivateStartOverdrawTrackingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateStopOverdrawTrackingFunction : public ExtensionFunction {
 public:
  AutotestPrivateStopOverdrawTrackingFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.stopOverdrawTracking",
                             AUTOTESTPRIVATE_STOPOVERDRAWTRACKING)

 private:
  ~AutotestPrivateStopOverdrawTrackingFunction() override;
  ResponseAction Run() override;

  void OnDataReceived(viz::mojom::OverdrawDataPtr data_ptr);
};

class AutotestPrivateInstallBruschettaFunction : public ExtensionFunction {
 public:
  AutotestPrivateInstallBruschettaFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.installBruschetta",
                             AUTOTESTPRIVATE_INSTALLBRUSCHETTA)

 private:
  ~AutotestPrivateInstallBruschettaFunction() override;
  ResponseAction Run() override;

  void ClickAccept();
  void OnInstallerFinish(bruschetta::BruschettaInstallResult result);
};

class AutotestPrivateRemoveBruschettaFunction : public ExtensionFunction {
 public:
  AutotestPrivateRemoveBruschettaFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removeBruschetta",
                             AUTOTESTPRIVATE_REMOVEBRUSCHETTA)

 private:
  ~AutotestPrivateRemoveBruschettaFunction() override;
  ResponseAction Run() override;

  void OnRemoveVm(bool success);
};

class AutotestPrivateIsFeatureEnabledFunction : public ExtensionFunction {
 public:
  AutotestPrivateIsFeatureEnabledFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isFeatureEnabled",
                             AUTOTESTPRIVATE_ISFEATUREENABLED)

 private:
  ~AutotestPrivateIsFeatureEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetCurrentInputMethodDescriptorFunction
    : public ExtensionFunction {
 public:
  AutotestPrivateGetCurrentInputMethodDescriptorFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getCurrentInputMethodDescriptor",
                             AUTOTESTPRIVATE_GETCURRENTINPUTMETHODDESCRIPTOR)

 private:
  ~AutotestPrivateGetCurrentInputMethodDescriptorFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetArcInteractiveStateFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetArcInteractiveStateFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setArcInteractiveState",
                             AUTOTESTPRIVATE_SETARCINTERACTIVESTATE)

 private:
  ~AutotestPrivateSetArcInteractiveStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateIsFieldTrialActiveFunction : public ExtensionFunction {
 public:
  AutotestPrivateIsFieldTrialActiveFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isFieldTrialActive",
                             AUTOTESTPRIVATE_ISFIELDTRIALACTIVE)

 private:
  ~AutotestPrivateIsFieldTrialActiveFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetArcWakefulnessModeFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetArcWakefulnessModeFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getArcWakefulnessMode",
                             AUTOTESTPRIVATE_GETARCWAKEFULNESSMODE)

 private:
  ~AutotestPrivateGetArcWakefulnessModeFunction() override;
  ResponseAction Run() override;

  // Get return value from mojo call.
  void OnGetWakefulnessStateRespond(arc::mojom::WakefulnessMode mode);
};

class AutotestPrivateSetDeviceLanguageFunction : public ExtensionFunction {
 public:
  AutotestPrivateSetDeviceLanguageFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setDeviceLanguage",
                             AUTOTESTPRIVATE_SETDEVICELANGUAGE)

 private:
  ~AutotestPrivateSetDeviceLanguageFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetDeviceEventLogFunction : public ExtensionFunction {
 public:
  AutotestPrivateGetDeviceEventLogFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getDeviceEventLog",
                             AUTOTESTPRIVATE_GETDEVICEEVENTLOG)

 private:
  ~AutotestPrivateGetDeviceEventLogFunction() override;
  ResponseAction Run() override;
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
