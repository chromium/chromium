// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_

#include <string>

#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/snapshot/screenshot_grabber.h"

namespace message_center {
class Notification;
}

namespace crostini {
enum class CrostiniResult;
}

namespace extensions {

class AutotestPrivateLogoutFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.logout", AUTOTESTPRIVATE_LOGOUT)

 private:
  ~AutotestPrivateLogoutFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRestartFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.restart", AUTOTESTPRIVATE_RESTART)

 private:
  ~AutotestPrivateRestartFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateShutdownFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.shutdown",
                             AUTOTESTPRIVATE_SHUTDOWN)

 private:
  ~AutotestPrivateShutdownFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLoginStatusFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.loginStatus",
                             AUTOTESTPRIVATE_LOGINSTATUS)

 private:
  ~AutotestPrivateLoginStatusFunction() override;
  ResponseAction Run() override;

  void OnIsReadyForPassword(bool is_ready);
};

class AutotestPrivateLockScreenFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.lockScreen",
                             AUTOTESTPRIVATE_LOCKSCREEN)

 private:
  ~AutotestPrivateLockScreenFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetExtensionsInfoFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getExtensionsInfo",
                             AUTOTESTPRIVATE_GETEXTENSIONSINFO)

 private:
  ~AutotestPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSimulateAsanMemoryBugFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.simulateAsanMemoryBug",
                             AUTOTESTPRIVATE_SIMULATEASANMEMORYBUG)

 private:
  ~AutotestPrivateSimulateAsanMemoryBugFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTouchpadSensitivityFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTouchpadSensitivity",
                             AUTOTESTPRIVATE_SETTOUCHPADSENSITIVITY)

 private:
  ~AutotestPrivateSetTouchpadSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapToClickFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapToClick",
                             AUTOTESTPRIVATE_SETTAPTOCLICK)

 private:
  ~AutotestPrivateSetTapToClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetThreeFingerClickFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setThreeFingerClick",
                             AUTOTESTPRIVATE_SETTHREEFINGERCLICK)

 private:
  ~AutotestPrivateSetThreeFingerClickFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetTapDraggingFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setTapDragging",
                             AUTOTESTPRIVATE_SETTAPDRAGGING)

 private:
  ~AutotestPrivateSetTapDraggingFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetNaturalScrollFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setNaturalScroll",
                             AUTOTESTPRIVATE_SETNATURALSCROLL)

 private:
  ~AutotestPrivateSetNaturalScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseSensitivityFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseSensitivity",
                             AUTOTESTPRIVATE_SETMOUSESENSITIVITY)

 private:
  ~AutotestPrivateSetMouseSensitivityFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPrimaryButtonRightFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPrimaryButtonRight",
                             AUTOTESTPRIVATE_SETPRIMARYBUTTONRIGHT)

 private:
  ~AutotestPrivateSetPrimaryButtonRightFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetMouseReverseScrollFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setMouseReverseScroll",
                             AUTOTESTPRIVATE_SETMOUSEREVERSESCROLL)

 private:
  ~AutotestPrivateSetMouseReverseScrollFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetVisibleNotificationsFunction
    : public UIThreadExtensionFunction {
 public:
  AutotestPrivateGetVisibleNotificationsFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getVisibleNotifications",
                             AUTOTESTPRIVATE_GETVISIBLENOTIFICATIONS)

 private:
  ~AutotestPrivateGetVisibleNotificationsFunction() override;
  ResponseAction Run() override;

  void OnGotNotifications(
      const std::vector<message_center::Notification>& notifications);

  ash::mojom::AshMessageCenterControllerPtr controller_;
};

class AutotestPrivateGetPlayStoreStateFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPlayStoreState",
                             AUTOTESTPRIVATE_GETPLAYSTORESTATE)

 private:
  ~AutotestPrivateGetPlayStoreStateFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetPlayStoreEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setPlayStoreEnabled",
                             AUTOTESTPRIVATE_SETPLAYSTOREENABLED)

 private:
  ~AutotestPrivateSetPlayStoreEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateGetHistogramFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getHistogram",
                             AUTOTESTPRIVATE_GETHISTOGRAM)

 private:
  ~AutotestPrivateGetHistogramFunction() override;
  ResponseAction Run() override;

  // Sends an asynchronous response containing data for the histogram named
  // |name|. Passed to content::FetchHistogramsAsynchronously() to be run after
  // new data from other processes has been collected.
  void RespondOnHistogramsFetched(const std::string& name);

  // Creates a response with current data for the histogram named |name|.
  ResponseValue GetHistogram(const std::string& name);
};

class AutotestPrivateIsAppShownFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.isAppShown",
                             AUTOTESTPRIVATE_ISAPPSHOWN)

 private:
  ~AutotestPrivateIsAppShownFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateLaunchAppFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.launchApp",
                             AUTOTESTPRIVATE_LAUNCHAPP)

 private:
  ~AutotestPrivateLaunchAppFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateSetCrostiniEnabledFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.setCrostiniEnabled",
                             AUTOTESTPRIVATE_SETCROSTINIENABLED)

 private:
  ~AutotestPrivateSetCrostiniEnabledFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRunCrostiniInstallerFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniInstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniInstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRestarted(crostini::CrostiniResult);
};

class AutotestPrivateRunCrostiniUninstallerFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.runCrostiniUninstaller",
                             AUTOTESTPRIVATE_RUNCROSTINIUNINSTALLER)

 private:
  ~AutotestPrivateRunCrostiniUninstallerFunction() override;
  ResponseAction Run() override;

  void CrostiniRemoved(crostini::CrostiniResult);
};

class AutotestPrivateTakeScreenshotFunction : public UIThreadExtensionFunction {
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

class AutotestPrivateGetPrinterListFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.getPrinterList",
                             AUTOTESTPRIVATE_GETPRINTERLIST)

 private:
  ~AutotestPrivateGetPrinterListFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateUpdatePrinterFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.updatePrinter",
                             AUTOTESTPRIVATE_UPDATEPRINTER)

 private:
  ~AutotestPrivateUpdatePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateRemovePrinterFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.removePrinter",
                             AUTOTESTPRIVATE_REMOVEPRINTER)

 private:
  ~AutotestPrivateRemovePrinterFunction() override;
  ResponseAction Run() override;
};

class AutotestPrivateBootstrapMachineLearningServiceFunction
    : public UIThreadExtensionFunction {
 public:
  AutotestPrivateBootstrapMachineLearningServiceFunction();
  DECLARE_EXTENSION_FUNCTION("autotestPrivate.bootstrapMachineLearningService",
                             AUTOTESTPRIVATE_BOOTSTRAPMACHINELEARNINGSERVICE)

 private:
  ~AutotestPrivateBootstrapMachineLearningServiceFunction() override;
  ResponseAction Run() override;

  // Callbacks for a basic Mojo call to MachineLearningService.LoadModel.
  void ModelLoaded(chromeos::machine_learning::mojom::LoadModelResult result);
  void ConnectionError();

  chromeos::machine_learning::mojom::ModelPtr model_;
};

// The profile-keyed service that manages the autotestPrivate extension API.
class AutotestPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
  GetFactoryInstance();

  // TODO(achuith): Replace these with a mock object for system calls.
  bool test_mode() const { return test_mode_; }
  void set_test_mode(bool test_mode) { test_mode_ = test_mode; }

 private:
  friend class BrowserContextKeyedAPIFactory<AutotestPrivateAPI>;

  AutotestPrivateAPI();
  ~AutotestPrivateAPI() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "AutotestPrivateAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  bool test_mode_;  // true for AutotestPrivateApiTest.AutotestPrivate test.
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_AUTOTEST_PRIVATE_AUTOTEST_PRIVATE_API_H_
