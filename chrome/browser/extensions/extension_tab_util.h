// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class ChromeExtensionFunctionDetails;
class ExtensionFunction;
class GURL;
class Profile;
class TabStripModel;
namespace content {
class BrowserContext;
class WebContents;
}

namespace blink::mojom {
class WindowFeatures;
}

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace extensions {
class Extension;
class WindowController;

// Provides various utility functions that help manipulate tabs.
class ExtensionTabUtil {
 public:
  static constexpr char kNoCrashBrowserError[] =
      "I'm sorry. I'm afraid I can't do that.";
  static constexpr char kCanOnlyMoveTabsWithinNormalWindowsError[] =
      "Tabs can only be moved to and from normal windows.";
  static constexpr char kCanOnlyMoveTabsWithinSameProfileError[] =
      "Tabs can only be moved between windows in the same profile.";
  static constexpr char kNoCurrentWindowError[] = "No current window";
  static constexpr char kWindowNotFoundError[] = "No window with id: *.";
  static constexpr char kTabNotFoundError[] = "No tab with id: *.";
  static constexpr char kTabStripNotEditableError[] =
      "Tabs cannot be edited right now (user may be dragging a tab).";
  static constexpr char kTabStripDoesNotSupportTabGroupsError[] =
      "Grouping is not supported by tabs in this window.";
  static constexpr char kJavaScriptUrlsNotAllowedInExtensionNavigations[] =
      "JavaScript URLs are not allowed in API based extension navigations. Use "
      "chrome.scripting.executeScript instead.";
  static constexpr char kBrowserWindowNotAllowed[] =
      "Browser windows not allowed.";
  static constexpr char kCannotNavigateToDevtools[] =
      "Cannot navigate to a devtools:// page without either the devtools or "
      "debugger permission.";
  static constexpr char kLockedFullscreenModeNewTabError[] =
      "You cannot create new tabs while in locked fullscreen mode.";
  static constexpr char kCannotNavigateToChromeUntrusted[] =
      "Cannot navigate to a chrome-untrusted:// page.";
  static constexpr char kFileUrlsNotAllowedInExtensionNavigations[] =
      "Cannot navigate to a file URL without local file access.";

  static constexpr char kTabsKey[] = "tabs";

  enum ScrubTabBehaviorType {
    kScrubTabFully,
    kScrubTabUrlToOrigin,
    kDontScrubTab,
  };

  struct ScrubTabBehavior {
    ScrubTabBehaviorType committed_info;
    ScrubTabBehaviorType pending_info;
  };

  struct OpenTabParams {
    OpenTabParams();
    ~OpenTabParams();

    bool create_browser_if_needed = false;
    std::optional<int> window_id;
    std::optional<int> opener_tab_id;
    std::optional<std::string> url;
    std::optional<bool> active;
    std::optional<bool> pinned;
    std::optional<int> index;
    std::optional<int> bookmark_id;
  };

  // Opens a new tab given an extension function |function| and creation
  // parameters |params|. If a tab can be produced, it will return a
  // base::Value::Dict representing the tab, otherwise it will optionally return
  // an error message, if any is appropriate.
  static base::expected<base::Value::Dict, std::string> OpenTab(
      ExtensionFunction* function,
      const OpenTabParams& params,
      bool user_gesture);

  static int GetWindowId(const Browser* browser);
  static int GetWindowIdOfTabStripModel(const TabStripModel* tab_strip_model);
  static int GetTabId(const content::WebContents* web_contents);
  static int GetWindowIdOfTab(const content::WebContents* web_contents);
  static base::Value::List CreateTabList(const Browser* browser,
                                         const Extension* extension,
                                         mojom::ContextType context);

  static WindowController* GetControllerFromWindowID(
      const ChromeExtensionFunctionDetails& details,
      int window_id,
      std::string* error_message);

  // Returns the Browser with the specified `window id` and the associated
  // `profile`. Optionally, this will also look at browsers associated with the
  // incognito version of `profile` if `also_match_incognito_profile` is true.
  // Populates `error_message` if no matching browser is found.
  static WindowController* GetControllerInProfileWithId(
      Profile* profile,
      int window_id,
      bool also_match_incognito_profile,
      std::string* error_message);

  // Returns the tabs:: API constant for the window type of the |browser|.
  static std::string GetBrowserWindowTypeText(const Browser& browser);

  // Creates a Tab object (see chrome/common/extensions/api/tabs.json) with
  // information about the state of a browser tab for the given |web_contents|.
  // This will scrub the tab of sensitive data (URL, favicon, title) according
  // to |scrub_tab_behavior| and |extension|'s permissions. A null extension is
  // treated as having no permissions.
  // By default, tab information should always be scrubbed (kScrubTab) for any
  // data passed to any extension.
  static api::tabs::Tab CreateTabObject(content::WebContents* web_contents,
                                        ScrubTabBehavior scrub_tab_behavior,
                                        const Extension* extension) {
    return CreateTabObject(web_contents, scrub_tab_behavior, extension, nullptr,
                           -1);
  }
  static api::tabs::Tab CreateTabObject(content::WebContents* web_contents,
                                        ScrubTabBehavior scrub_tab_behavior,
                                        const Extension* extension,
                                        TabStripModel* tab_strip,
                                        int tab_index);

  // Creates a base::Value::Dict representing the window for the given
  // |browser|, and scrubs any privacy-sensitive data that |extension| does not
  // have access to. |populate_tab_behavior| determines whether tabs will be
  // populated in the result. |context| is used to determine the
  // ScrubTabBehavior for the populated tabs data.
  // TODO(devlin): Convert this to a api::Windows::Window object.
  static base::Value::Dict CreateWindowValueForExtension(
      const Browser& browser,
      const Extension* extension,
      WindowController::PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context);

  // Creates a tab MutedInfo object (see chrome/common/extensions/api/tabs.json)
  // with information about the mute state of a browser tab.
  static api::tabs::MutedInfo CreateMutedInfo(content::WebContents* contents);

  // Gets the level of scrubbing of tab data that needs to happen for a given
  // extension and web contents. This is the preferred way to get
  // ScrubTabBehavior.
  static ScrubTabBehavior GetScrubTabBehavior(const Extension* extension,
                                              mojom::ContextType context,
                                              content::WebContents* contents);
  // Only use this if there is no access to a specific WebContents, such as when
  // the tab has been closed and there is no active WebContents anymore.
  static ScrubTabBehavior GetScrubTabBehavior(const Extension* extension,
                                              mojom::ContextType context,
                                              const GURL& url);

  // Removes any privacy-sensitive fields from a Tab object if appropriate,
  // given the permissions of the extension and the tab in question.  The
  // tab object is modified in place.
  static void ScrubTabForExtension(const Extension* extension,
                                   content::WebContents* contents,
                                   api::tabs::Tab* tab,
                                   ScrubTabBehavior scrub_tab_behavior);

  // Gets the |tab_strip_model| and |tab_index| for the given |web_contents|.
  static bool GetTabStripModel(const content::WebContents* web_contents,
                               TabStripModel** tab_strip_model,
                               int* tab_index);

  // Returns the active tab's WebContents if there is an active tab. Returns
  // null if there is no active tab.
  static content::WebContents* GetActiveTab(Browser* browser);

  // Any out parameter (|browser|, |tab_strip|, |contents|, & |tab_index|) may
  // be NULL and will not be set within the function.
  //
  // The output `*browser` value may be null if the tab is a prerender tab that
  // has no corresponding browser window.
  static bool GetTabById(int tab_id,
                         content::BrowserContext* browser_context,
                         bool include_incognito,
                         Browser** browser,
                         TabStripModel** tab_strip,
                         content::WebContents** contents,
                         int* tab_index);
  static bool GetTabById(int tab_id,
                         content::BrowserContext* browser_context,
                         bool include_incognito,
                         content::WebContents** contents);

  // Gets the extensions-specific Group ID.
  static int GetGroupId(const tab_groups::TabGroupId& id);

  // Gets the window ID that the group belongs to.
  static int GetWindowIdOfGroup(const tab_groups::TabGroupId& id);

  // Gets the metadata for the group with ID `group_id`. Sets the `error` if not
  // found. `browser`, `id`, or `visual_data` may be nullptr and will not be set
  // within the function if so.
  static bool GetGroupById(int group_id,
                           content::BrowserContext* browser_context,
                           bool include_incognito,
                           Browser** browser,
                           tab_groups::TabGroupId* id,
                           const tab_groups::TabGroupVisualData** visual_data,
                           std::string* error);

  // Creates a TabGroup object
  // (see chrome/common/extensions/api/tab_groups.json) with information about
  // the state of a tab group for the given group `id`. Most group metadata is
  // derived from the `visual_data`, which specifies group color, title, etc.
  static api::tab_groups::TabGroup CreateTabGroupObject(
      const tab_groups::TabGroupId& id,
      const tab_groups::TabGroupVisualData& visual_data);
  static std::optional<api::tab_groups::TabGroup> CreateTabGroupObject(
      const tab_groups::TabGroupId& id);

  // Conversions between the api::tab_groups::Color enum and the TabGroupColorId
  // enum.
  static api::tab_groups::Color ColorIdToColor(
      const tab_groups::TabGroupColorId& color_id);
  static tab_groups::TabGroupColorId ColorToColorId(
      api::tab_groups::Color color);

  // Returns all active web contents for the given |browser_context|.
  static std::vector<content::WebContents*> GetAllActiveWebContentsForContext(
      content::BrowserContext* browser_context,
      bool include_incognito);

  // Determines if the |web_contents| is in |browser_context| or it's OTR
  // BrowserContext if |include_incognito| is true.
  static bool IsWebContentsInContext(content::WebContents* web_contents,
                                     content::BrowserContext* browser_context,
                                     bool include_incognito);

  // Takes |url_string| and returns a GURL which is either valid and absolute
  // or invalid. If |url_string| is not directly interpretable as a valid (it is
  // likely a relative URL) an attempt is made to resolve it. When |extension|
  // is non-null, the URL is resolved relative to its extension base
  // (chrome-extension://<id>/). Using the source frame url would be more
  // correct, but because the api shipped with urls resolved relative to their
  // extension base, we decided it wasn't worth breaking existing extensions to
  // fix.
  static GURL ResolvePossiblyRelativeURL(const std::string& url_string,
                                         const Extension* extension);

  // Returns true if navigating to |url| could kill a page or the browser
  // itself, whether by simulating a crash, browser quit, thread hang, or
  // equivalent. Extensions should be prevented from navigating to such URLs.
  //
  // The caller should ensure that |url| has already been "fixed up" by calling
  // url_formatter::FixupURL.
  static bool IsKillURL(const GURL& url);

  // Resolves the URL and ensures the extension is allowed to navigate to it.
  // Returns the url if successful, otherwise returns an error string.
  static base::expected<GURL, std::string> PrepareURLForNavigation(
      const std::string& url_string,
      const Extension* extension,
      content::BrowserContext* browser_context);

  // Opens a tab for the specified |web_contents|.
  static void CreateTab(std::unique_ptr<content::WebContents> web_contents,
                        const std::string& extension_id,
                        WindowOpenDisposition disposition,
                        const blink::mojom::WindowFeatures& window_features,
                        bool user_gesture);

  // Executes the specified callback for all tabs in all browser windows.
  static void ForEachTab(
      base::RepeatingCallback<void(content::WebContents*)> callback);

  static WindowController* GetWindowControllerOfTab(
      const content::WebContents* web_contents);

  // Open the extension's options page. Returns true if an options page was
  // successfully opened (though it may not necessarily *load*, e.g. if the
  // URL does not exist). This call to open the options page is iniatiated by
  // the extension via chrome.runtime.openOptionsPage.
  static bool OpenOptionsPageFromAPI(const Extension* extension,
                                     content::BrowserContext* browser_context);

  // Open the extension's options page. Returns true if an options page was
  // successfully opened (though it may not necessarily *load*, e.g. if the
  // URL does not exist).
  static bool OpenOptionsPage(const Extension* extension, Browser* browser);

  // Returns true if the given Browser can report tabs to extensions.
  // Example of Browsers which don't support tabs include apps and devtools.
  static bool BrowserSupportsTabs(Browser* browser);

  // Determines the loading status of the given |contents|. This needs to access
  // some non-const member functions of |contents|, but actually leaves it
  // unmodified.
  static api::tabs::TabStatus GetLoadingStatus(content::WebContents* contents);

  // Clears the back-forward cache for all active tabs across all browser
  // contexts.
  static void ClearBackForwardCache();

  // Check TabStripModel editability in every browser because a drag session
  // could be running in another browser that reverts to the current browser. Or
  // a drag could be mid-handoff if from one browser to another.
  static bool IsTabStripEditable();

  // Retrieve a TabStripModel only if every browser is editable.
  static TabStripModel* GetEditableTabStripModel(Browser* browser);

  static bool TabIsInSavedTabGroup(content::WebContents* contents,
                                   TabStripModel* tab_strip_model);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_UTIL_H_
