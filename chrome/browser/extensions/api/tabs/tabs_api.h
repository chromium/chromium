// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/api/web_contents_capture_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

class GURL;
class SkBitmap;
class TabStripModel;

namespace base {
class TaskRunner;
}

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

// Converts a ZoomMode to its ZoomSettings representation.
void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings);

// Windows
class WindowsGetFunction : public ExtensionFunction {
  ~WindowsGetFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ExtensionFunction {
  ~WindowsGetCurrentFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ExtensionFunction {
  ~WindowsGetLastFocusedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ExtensionFunction {
  ~WindowsGetAllFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ExtensionFunction {
  ~WindowsCreateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)
};
class WindowsUpdateFunction : public ExtensionFunction {
  ~WindowsUpdateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)
};
class WindowsRemoveFunction : public ExtensionFunction {
  ~WindowsRemoveFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public ExtensionFunction {
  ~TabsGetFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public ExtensionFunction {
  ~TabsGetCurrentFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public ExtensionFunction {
  ~TabsGetSelectedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public ExtensionFunction {
  ~TabsGetAllInWindowFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public ExtensionFunction {
  ~TabsQueryFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)
};
class TabsCreateFunction : public ExtensionFunction {
  ~TabsCreateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public ExtensionFunction {
  ~TabsDuplicateFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public ExtensionFunction {
  ~TabsHighlightFunction() override {}
  ResponseAction Run() override;
  bool HighlightTab(TabStripModel* tabstrip,
                    ui::ListSelectionModel* selection,
                    std::optional<size_t>* active_index,
                    int index,
                    std::string* error);
  DECLARE_EXTENSION_FUNCTION("tabs.highlight", TABS_HIGHLIGHT)
};
class TabsUpdateFunction : public ExtensionFunction {
 public:
  TabsUpdateFunction();

 protected:
  ~TabsUpdateFunction() override {}
  bool UpdateURL(const std::string& url,
                 int tab_id,
                 std::string* error);
  ResponseValue GetResult();

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

 private:
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public ExtensionFunction {
  ~TabsMoveFunction() override {}
  ResponseAction Run() override;
  bool MoveTab(int tab_id,
               int* new_index,
               base::Value::List& tab_values,
               const std::optional<int>& window_id,
               std::string* error);
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public ExtensionFunction {
  ~TabsReloadFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public ExtensionFunction {
 public:
  TabsRemoveFunction();
  void TabDestroyed();

 private:
  class WebContentsDestroyedObserver;
  ~TabsRemoveFunction() override;
  ResponseAction Run() override;
  bool RemoveTab(int tab_id, std::string* error);

  int remaining_tabs_count_ = 0;
  bool triggered_all_tab_removals_ = false;
  std::vector<std::unique_ptr<WebContentsDestroyedObserver>>
      web_contents_destroyed_observers_;
  DECLARE_EXTENSION_FUNCTION("tabs.remove", TABS_REMOVE)
};
class TabsGroupFunction : public ExtensionFunction {
  ~TabsGroupFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.group", TABS_GROUP)
};
class TabsUngroupFunction : public ExtensionFunction {
  ~TabsUngroupFunction() override = default;
  ResponseAction Run() override;
  bool UngroupTab(int tab_id, std::string* error);
  DECLARE_EXTENSION_FUNCTION("tabs.ungroup", TABS_UNGROUP)
};
class TabsDetectLanguageFunction
    : public ExtensionFunction,
      public content::WebContentsObserver,
      public translate::TranslateDriver::LanguageDetectionObserver {
 private:
  ~TabsDetectLanguageFunction() override {}
  ResponseAction Run() override;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Resolves the API call with the detected |language|.
  void RespondWithLanguage(const std::string& language);

  // Indicates if this instance is observing the tabs' WebContents and the
  // ContentTranslateDriver, in which case the observers must be unregistered.
  bool is_observing_ = false;

  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};

class TabsCaptureVisibleTabFunction
    : public extensions::WebContentsCaptureClient,
      public ExtensionFunction {
 public:
  TabsCaptureVisibleTabFunction();

  TabsCaptureVisibleTabFunction(const TabsCaptureVisibleTabFunction&) = delete;
  TabsCaptureVisibleTabFunction& operator=(
      const TabsCaptureVisibleTabFunction&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void set_disable_throttling_for_tests(
      bool disable_throttling_for_test) {
    disable_throttling_for_test_ = disable_throttling_for_test;
  }

  // ExtensionFunction implementation.
  ResponseAction Run() override;
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  bool ShouldSkipQuotaLimiting() const override;

 protected:
  ~TabsCaptureVisibleTabFunction() override {}

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  content::WebContents* GetWebContentsForID(int window_id, std::string* error);

  // extensions::WebContentsCaptureClient:
  ScreenshotAccess GetScreenshotAccess(
      content::WebContents* web_contents) const override;
  bool ClientAllowsTransparency() override;
  void OnCaptureSuccess(const SkBitmap& bitmap) override;
  void OnCaptureFailure(CaptureResult result) override;

  void EncodeBitmapOnWorkerThread(
      scoped_refptr<base::TaskRunner> reply_task_runner,
      const SkBitmap& bitmap);
  void OnBitmapEncodedOnUIThread(bool success, std::string base64_result);

 private:
  DECLARE_EXTENSION_FUNCTION("tabs.captureVisibleTab", TABS_CAPTUREVISIBLETAB)

  static std::string CaptureResultToErrorMessage(CaptureResult result);

  static bool disable_throttling_for_test_;
};

// Implement API calls tabs.executeScript, tabs.insertCSS, and tabs.removeCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  ~ExecuteCodeInTabFunction() override;

  // Initializes |execute_tab_id_| and |details_|.
  InitResult Init() override;
  bool ShouldInsertCSS() const override;
  bool ShouldRemoveCSS() const override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  ScriptExecutor* GetScriptExecutor(std::string* error) override;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;

 private:
  const ChromeExtensionFunctionDetails chrome_details_;

  // Id of tab which executes code.
  int execute_tab_id_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsExecuteScriptFunction() override {}

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

// TODO(https://crrev.com/c/608854): When a file URL is passed, this will do
// more work than needed: since the key is created based on the file URL in
// that case, we don't actually need to
//
// a) load the file or
// b) localize it
//
// ... hence, it could just go straight to the ScriptExecutor.
class TabsRemoveCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsRemoveCSSFunction() override {}

  bool ShouldRemoveCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.removeCSS", TABS_REMOVECSS)
};

class TabsSetZoomFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomSettingsFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

class TabsDiscardFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.discard", TABS_DISCARD)

  TabsDiscardFunction();

  TabsDiscardFunction(const TabsDiscardFunction&) = delete;
  TabsDiscardFunction& operator=(const TabsDiscardFunction&) = delete;

 private:
  ~TabsDiscardFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class TabsGoForwardFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.goForward", TABS_GOFORWARD)

  TabsGoForwardFunction() {}

  TabsGoForwardFunction(const TabsGoForwardFunction&) = delete;
  TabsGoForwardFunction& operator=(const TabsGoForwardFunction&) = delete;

 private:
  ~TabsGoForwardFunction() override {}

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class TabsGoBackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.goBack", TABS_GOBACK)

  TabsGoBackFunction() {}

  TabsGoBackFunction(const TabsGoBackFunction&) = delete;
  TabsGoBackFunction& operator=(const TabsGoBackFunction&) = delete;

 private:
  ~TabsGoBackFunction() override {}

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
