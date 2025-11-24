// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "components/safe_browsing/buildflags.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/api/web_contents_capture_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/user_script.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "url/gurl.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class GURL;
class SessionID;
class SkBitmap;
class TabStripModel;

namespace base {
class TaskRunner;
}

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace ui {
class ListSelectionModel;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

namespace api::windows {
enum class WindowState;
}

// This namespace includes a collection of conceptually-internal helper methods
// and constants that are currently here because they are used by both
// tabs_api.cc and tabs_api_non_android.cc. Eventually, they should only be
// used by tabs_api.cc, and we can move them to an anonymous namespace in
// tabs_api.cc.
// TODO(devlin): Do that. ^^
namespace tabs_internal {

inline constexpr char kMissingLockWindowFullscreenPrivatePermission[] =
    "Cannot lock window to fullscreen or close a locked fullscreen window "
    "without lockWindowFullscreenPrivate manifest permission";

// A helper class to extract popular properties from different arguments.
template <typename T>
class ApiParameterExtractor {
 public:
  explicit ApiParameterExtractor(std::optional<T>& params) : params_(*params) {}
  ~ApiParameterExtractor() = default;

  bool populate_tabs() {
    if (params_->query_options && params_->query_options->populate) {
      return *params_->query_options->populate;
    }
    return false;
  }

  WindowController::TypeFilter type_filters() {
    if (params_->query_options && params_->query_options->window_types) {
      return WindowController::GetFilterFromWindowTypes(
          *params_->query_options->window_types);
    }
    return WindowController::kNoWindowFilter;
  }

 private:
  raw_ref<T> params_;
};

// Returns true if the given `extension` has API access to the locked
// fullscreen permission.
bool ExtensionHasLockedFullscreenPermission(const Extension* extension);

// Helper method to generate a new tab object for the given `contents`,
// appropriately scrubbed of data for the given `extension`.
api::tabs::Tab CreateTabObjectHelper(content::WebContents* contents,
                                     const Extension* extension,
                                     mojom::ContextType context,
                                     BrowserWindowInterface* browser,
                                     int tab_index);

// Retrieves the tab associated with the given `tab_id`, populating
// `contents_out`, `window_out`, and `index_out` with the result. If the tab
// isn't found and `error_out` is non-null, populates `error_out` with an
// appropriate error.
// Returns true if the tab was found.
bool GetTabById(int tab_id,
                content::BrowserContext* context,
                bool include_incognito,
                WindowController** window_out,
                content::WebContents** contents_out,
                int* index_out,
                std::string* error_out);

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Notifies the safe browsing telemetry service of a relevant extension action.
void NotifyExtensionTelemetry(Profile* profile,
                              const Extension* extension,
                              safe_browsing::TabsApiInfo::ApiMethod api_method,
                              const std::string& current_url,
                              const std::string& new_url,
                              const std::optional<StackTrace>& js_callstack);
#endif

// Gets the WebContents for `tab_id` if it is specified. Otherwise get the
// WebContents for the active tab in the `function`'s current window.
// Returns nullptr and fills `error` if failed.
content::WebContents* GetTabsAPIDefaultWebContents(ExtensionFunction* function,
                                                   int tab_id,
                                                   std::string* error);

// Converts the given `state` to the mojom::WindowShowState equivalent.
ui::mojom::WindowShowState ConvertToWindowShowState(
    api::windows::WindowState state);

// Returns whether the given `bounds` intersect with at least 50% of all the
// displays.
bool WindowBoundsIntersectDisplays(const gfx::Rect& bounds);

// Moves the given tab to the `target_browser`. On success, returns the
// new index of the tab in the target tabstrip. On failure, returns -1.
// Assumes that the caller has already checked whether the target window is
// different from the source.
int MoveTabToWindow(ExtensionFunction* function,
                    int tab_id,
                    BrowserWindowInterface* target_browser,
                    int new_index,
                    std::string* error);

}  // namespace tabs_internal

// Converts a ZoomMode to its ZoomSettings representation.
void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings);

// Windows
class WindowsGetFunction : public ExtensionFunction {
  ~WindowsGetFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.get", WINDOWS_GET)
};
class WindowsGetCurrentFunction : public ExtensionFunction {
  ~WindowsGetCurrentFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getCurrent", WINDOWS_GETCURRENT)
};
class WindowsGetLastFocusedFunction : public ExtensionFunction {
  ~WindowsGetLastFocusedFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getLastFocused", WINDOWS_GETLASTFOCUSED)
};
class WindowsGetAllFunction : public ExtensionFunction {
  ~WindowsGetAllFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.getAll", WINDOWS_GETALL)
};
class WindowsCreateFunction : public ExtensionFunction {
  ~WindowsCreateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.create", WINDOWS_CREATE)

 private:
  // Ensures the tab for the window is valid. Returns an error string, or the
  // empty string if the tab is valid.
  static std::string ValidateTab(WindowController* source_window,
                                 Profile* window_profile,
                                 Profile* calling_profile,
                                 content::WebContents* web_contents,
                                 bool is_locked_fullscreen,
                                 const std::vector<GURL>& urls);

  // Uses `create_data` to set the window position and size in `window_bounds`.
  // Returns an error string, or the empty string if the bounds are valid.
  static std::string SetWindowBounds(
      const api::windows::Create::Params::CreateData& create_data,
      gfx::Rect& window_bounds);

#if BUILDFLAG(IS_CHROMEOS)
  void OnWindowCreatedAsynchronously(const SessionID& session_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
};
class WindowsUpdateFunction : public ExtensionFunction {
  ~WindowsUpdateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.update", WINDOWS_UPDATE)

 private:
  // Applies the updates from `params` to the `browser` window.
  void UpdateWindowState(const api::windows::Update::Params& params,
                         BrowserWindowInterface* browser,
                         WindowController* window_controller,
                         ui::mojom::WindowShowState show_state,
                         bool set_window_bounds,
                         const gfx::Rect& window_bounds);
};
class WindowsRemoveFunction : public ExtensionFunction {
  ~WindowsRemoveFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("windows.remove", WINDOWS_REMOVE)
};

// Tabs
class TabsGetFunction : public ExtensionFunction {
  ~TabsGetFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};
class TabsGetCurrentFunction : public ExtensionFunction {
  ~TabsGetCurrentFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getCurrent", TABS_GETCURRENT)
};
class TabsGetSelectedFunction : public ExtensionFunction {
  ~TabsGetSelectedFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getSelected", TABS_GETSELECTED)
};
class TabsGetAllInWindowFunction : public ExtensionFunction {
  ~TabsGetAllInWindowFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.getAllInWindow", TABS_GETALLINWINDOW)
};
class TabsQueryFunction : public ExtensionFunction {
 public:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.query", TABS_QUERY)

 private:
  ~TabsQueryFunction() override = default;

  // Builds the list of tab objects to return.
  base::Value::List BuildTabList(BrowserWindowInterface* current_browser,
                                 BrowserWindowInterface* last_active_browser,
                                 const URLPatternSet& url_patterns,
                                 const std::string& window_type,
                                 int window_id,
                                 int tab_index);

  // Returns true if the given `candidate_profile` matches the calling
  // extension's profile (taking into account incognito access).
  bool MatchesProfile(Profile* candidate_profile);

  bool MatchesWindow(BrowserWindowInterface* candidate_browser,
                     BrowserWindowInterface* current_browser,
                     BrowserWindowInterface* last_active_browser,
                     const std::string& target_window_type,
                     int target_window_id);

  bool MatchesTab(tabs::TabInterface* candidate_tab,
                  const URLPatternSet& target_url_patterns);

  // The query parameters passed by the extension.
  api::tabs::Query::Params::QueryInfo query_info_;
};
class TabsCreateFunction : public ExtensionFunction {
  ~TabsCreateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)
};
class TabsDuplicateFunction : public ExtensionFunction {
  ~TabsDuplicateFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.duplicate", TABS_DUPLICATE)
};
class TabsHighlightFunction : public ExtensionFunction {
  ~TabsHighlightFunction() override = default;
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
  ~TabsUpdateFunction() override = default;
  bool UpdateURL(content::WebContents* web_contents,
                 const std::string& url,
                 int tab_id,
                 std::string* error);
  ResponseValue GetResult(content::WebContents* web_contents);

 private:
  ResponseAction Run() override;

  // Returns true on success. Out parameters are the ID of the default tab to
  // update and its web contents, or an error string.
  bool ComputeDefaultTabId(int& tab_id,
                           content::WebContents*& contents,
                           std::string& error);

  // Updates the active or selected tab. Returns true on success or if there was
  // nothing to do. Returns false on failure with an error message.
  bool UpdateActiveTab(const api::tabs::Update::Params& params,
                       TabStripModel* tab_strip,
                       int tab_index,
                       const content::WebContents* contents,
                       std::string& error);

  // Updates the highlight state of the given tab. Returns true on success or if
  // there was nothing to do. Returns false on failure with an error.
  bool UpdateHighlightedTab(const api::tabs::Update::Params& params,
                            TabStripModel* tab_strip,
                            int tab_index,
                            std::string& error);

  DECLARE_EXTENSION_FUNCTION("tabs.update", TABS_UPDATE)
};
class TabsMoveFunction : public ExtensionFunction {
  ~TabsMoveFunction() override = default;
  ResponseAction Run() override;
  bool MoveTab(int tab_id,
               int* new_index,
               base::Value::List& tab_values,
               const std::optional<int>& window_id,
               std::string* error);
  DECLARE_EXTENSION_FUNCTION("tabs.move", TABS_MOVE)
};
class TabsReloadFunction : public ExtensionFunction {
  ~TabsReloadFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tabs.reload", TABS_RELOAD)
};
class TabsRemoveFunction : public ExtensionFunction {
 public:
  TabsRemoveFunction();
  void TabDestroyed();

 private:
  ~TabsRemoveFunction() override;
  ResponseAction Run() override;
  bool RemoveTab(int tab_id, std::string* error);

  int remaining_tabs_count_ = 0;
  bool triggered_all_tab_removals_ = false;

  class WebContentsDestroyedObserver;
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
  ~TabsDetectLanguageFunction() override = default;
  ResponseAction Run() override;

  // Starts the language detection process, which is asynchronous.
  ResponseAction StartLanguageDetection(content::WebContents* contents);

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

  // translate::TranslateDriver::LanguageDetectionObserver:
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Resolves the API call with the detected `language`.
  void RespondWithLanguage(const std::string& language);

  // Indicates if this instance is observing the tabs' WebContents and the
  // ContentTranslateDriver, in which case the observers must be unregistered.
  bool is_observing_ = false;

  DECLARE_EXTENSION_FUNCTION("tabs.detectLanguage", TABS_DETECTLANGUAGE)
};

class TabsCaptureVisibleTabFunction :
    public extensions::WebContentsCaptureClient,
    public ExtensionFunction {
 public:
  TabsCaptureVisibleTabFunction();

  TabsCaptureVisibleTabFunction(const TabsCaptureVisibleTabFunction&) = delete;
  TabsCaptureVisibleTabFunction& operator=(
      const TabsCaptureVisibleTabFunction&) = delete;

  static void set_disable_throttling_for_tests(
      bool disable_throttling_for_test) {
    disable_throttling_for_test_ = disable_throttling_for_test;
  }

  // ExtensionFunction implementation.
  ResponseAction Run() override;
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  bool ShouldSkipQuotaLimiting() const override;

 protected:
  ~TabsCaptureVisibleTabFunction() override = default;

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
  void OnBitmapEncodedOnUIThread(std::optional<std::string> base64_result);

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

  // Initializes `execute_tab_id_` and `details_`.
  InitResult Init() override;
  bool ShouldInsertCSS() const override;
  bool ShouldRemoveCSS() const override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  ScriptExecutor* GetScriptExecutor(std::string* error) override;
  bool IsWebView() const override;
  int GetRootFrameId() const override;
  const GURL& GetWebViewSrc() const override;

 private:
  const ChromeExtensionFunctionDetails chrome_details_{this};

  // Id of tab which executes code.
  int execute_tab_id_ = -1;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsExecuteScriptFunction() override = default;

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override = default;

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
  ~TabsRemoveCSSFunction() override = default;

  bool ShouldRemoveCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.removeCSS", TABS_REMOVECSS)
};

class TabsSetZoomFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomFunction() override = default;

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomFunction() override = default;

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsSetZoomSettingsFunction() override = default;

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ExtensionFunction {
 private:
  ~TabsGetZoomSettingsFunction() override = default;

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

  TabsGoForwardFunction() = default;

  TabsGoForwardFunction(const TabsGoForwardFunction&) = delete;
  TabsGoForwardFunction& operator=(const TabsGoForwardFunction&) = delete;

 private:
  ~TabsGoForwardFunction() override = default;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class TabsGoBackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabs.goBack", TABS_GOBACK)

  TabsGoBackFunction() = default;

  TabsGoBackFunction(const TabsGoBackFunction&) = delete;
  TabsGoBackFunction& operator=(const TabsGoBackFunction&) = delete;

 private:
  ~TabsGoBackFunction() override = default;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
