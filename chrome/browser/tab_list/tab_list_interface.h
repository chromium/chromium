// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_H_
#define CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/scoped_observation_traits.h"
#include "build/android_buildflags.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;
class SessionID;
class TabListInterfaceObserver;

namespace content {
class WebContents;
}

namespace gfx {
class Range;
}

namespace tab_groups {
class TabGroupVisualData;
}

// Interface for supporting a basic set of tab operations on Android and
// Desktop.
class TabListInterface {
 public:
  DECLARE_USER_DATA(TabListInterface);

  TabListInterface() = default;
  virtual ~TabListInterface() = default;

  TabListInterface(const TabListInterface& other) = delete;
  void operator=(const TabListInterface& other) = delete;

  // Returns the TabListInterface associated with the given `browser`.
  // In unit tests, this will return a mock if one has been registered via
  // ui::ScopedUnownedUserData<TabListInterface> in the browser's
  // UnownedUserDataHost.
  static TabListInterface* From(BrowserWindowInterface* browser);

  // Adds / removes observers from this tab list.
  virtual void AddTabListInterfaceObserver(
      TabListInterfaceObserver* observer) = 0;
  virtual void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) = 0;

  // Returns the count of tabs within the tab list.
  virtual int GetTabCount() const = 0;

  // Returns the index of the currently-active tab. Note that this is different
  // from the selected tab (of which there may be multiple).
  virtual int GetActiveIndex() const = 0;

  // Returns the `TabInterface` for the currently-active tab.
  virtual tabs::TabInterface* GetActiveTab() = 0;

  // Activates the given `tab`. The `tab` must be present in this tab list.
  virtual void ActivateTab(tabs::TabHandle tab) = 0;

  // Opens a new tab to the given `url`, inserting it at `index` in the tab
  // strip. `index` may be ignored by the implementation if necessary.
  virtual tabs::TabInterface* OpenTab(const GURL& url, int index) = 0;

  // Sets the opener for the `target` tab to be the `opener` tab.
  virtual void SetOpenerForTab(tabs::TabHandle target,
                               tabs::TabHandle opener) = 0;

  // Get the `opener` tab from `target` tab.
  virtual tabs::TabInterface* GetOpenerForTab(tabs::TabHandle target) = 0;

  // Insert `web_contents` into a TabListInterface at target `index`.
  // If `should_pin` is true, the tab will be pinned. On desktop, this
  // corresponds to the `ADD_PINNED` flag. Other actions on insertion (like
  // making the tab active) are not supported by this method.
  // The tab can optionally be added to a `group`.
  // Returns the interface for the newly inserted tab.
  virtual tabs::TabInterface* InsertWebContentsAt(
      int index,
      std::unique_ptr<content::WebContents> web_contents,
      bool should_pin,
      std::optional<tab_groups::TabGroupId> group) = 0;

  // Attempts to discard the renderer for the `tab` from memory and return the
  // discarded WebContents if successful.
  //
  // For details refer to:
  // docs/website/site/chromium-os/chromiumos-design-docs/tab-discarding-and-reloading/index.md
  virtual content::WebContents* DiscardTab(tabs::TabHandle tab) = 0;

  // Duplicates the `tab` to the next adjacent index. Returns the newly-
  // created tab.
  virtual tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) = 0;

  // Returns the `TabInterface` for the tab at a given `index`. May be `nullptr`
  // if the index is out-of-bounds.
  virtual tabs::TabInterface* GetTab(int index) = 0;

  // Returns the index of the given `tab`, if it exists in the tab strip.
  // Otherwise, returns -1.
  virtual int GetIndexOfTab(tabs::TabHandle tab) = 0;

  // Highlights a set of tabs. This will clear any initially-selected tabs and
  // highlight the new set.  The `tab_to_activate` becomes the active tab and
  // must be present in `tabs`.
  virtual void HighlightTabs(tabs::TabHandle tab_to_activate,
                             const std::set<tabs::TabHandle>& tabs) = 0;

  // Moves the `tab` to `index`. The nearest valid index will be used.
  virtual void MoveTab(tabs::TabHandle tab, int index) = 0;

  // Closes the `tab`.
  virtual void CloseTab(tabs::TabHandle tab) = 0;

  // Returns an in-order list of all tabs in the tab strip.
  virtual std::vector<tabs::TabInterface*> GetAllTabs() = 0;

  // Pins the `tab`. Pinning a pinned tab has no effect. This may result in
  // moving the tab if necessary.
  virtual void PinTab(tabs::TabHandle tab) = 0;

  // Unpins the `tab`. Unpinning an unpinned tab has no effect. This may result
  // in moving the tab if necessary.
  virtual void UnpinTab(tabs::TabHandle tab) = 0;

  // Returns true if this tab list contains a tab group with `group_id`.
  virtual bool ContainsTabGroup(tab_groups::TabGroupId group_id) = 0;

  // Returns a list of tab groups in this tab strip. If the tab strip does not
  // support tab groups (e.g. legacy apps) returns an empty vector.
  virtual std::vector<tab_groups::TabGroupId> ListTabGroups() = 0;

  // Returns the visual data for a tab group, or nullopt on error.
  virtual std::optional<tab_groups::TabGroupVisualData> GetTabGroupVisualData(
      tab_groups::TabGroupId group_id) = 0;

  // Returns the range of tab model indices this group contains. The returned
  // range will never be a reverse range. It will always be a forward range or
  // the empty range (0,0) on error. See TabGroup::ListTabs() for details.
  virtual gfx::Range GetTabGroupTabIndices(tab_groups::TabGroupId group_id) = 0;

  // Creates a tab group from a list of tabs and returns the group ID. Returns
  // nullopt on error (for example, if the tab list is empty).
  virtual std::optional<tab_groups::TabGroupId> CreateTabGroup(
      const std::vector<tabs::TabHandle>& tabs) = 0;

  // Sets the visual data for a tab group. Implementations may choose to notify
  // observers of the change.
  virtual void SetTabGroupVisualData(
      tab_groups::TabGroupId group_id,
      const tab_groups::TabGroupVisualData& visual_data) = 0;

  // Adds `tabs` to the `group_id` if provided or creates a new tab group.
  // Returns the tab group ID of the created or added to group. Tabs will be
  // moved as necessary to make the group contiguous. Pinned tabs will no longer
  // be pinned, and tabs that were in other groups will be removed from those
  // groups. Will no-op and return nullopt if the provided `group_id` is not an
  // existing tab group.
  virtual std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) = 0;

  // Ungroups all `tabs`. Tabs will be moved to an index adjacent to the group
  // they were in.
  virtual void Ungroup(const std::set<tabs::TabHandle>& tabs) = 0;

  // Moves the tab group to `index`. The nearest valid index will be used.
  // The index assumes the group has already been removed from the tab strip.
  virtual void MoveGroupTo(tab_groups::TabGroupId group_id, int index) = 0;

  // Moves `tab` from this TabListInterface to the TabListInterface associated
  // with `destination_window_id`. The tab will be inserted at `index` in the
  // destination tab list. This will no-op if the tab is not present in this
  // TabListInterface or the destination window does not exist. `index` may be
  // adjusted as necessary to ensure the tab is in a valid position.
  virtual void MoveTabToWindow(tabs::TabHandle tab,
                               SessionID destination_window_id,
                               int destination_index) = 0;

  // Moves the tab group with `group_id` from this TabListInterface to the
  // TabListInterface associated with `destination_window_id`. The tab group
  // will be inserted with the first tab at `index` in the destination tab list.
  // This will no-op if the tab group is not present in this TabListInterface or
  // the destination window does not exist. `index` may be adjusted as necessary
  // to ensure the tab group is in a valid position.
  virtual void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                    SessionID destination_window_id,
                                    int destination_index) = 0;

  // Returns true if *this* tab list is currently editable according to its own
  // internal state.
  // NOTE: For most operations, you probably want to use the static
  // method `CanEditTabList()` below. See also comments there.
  virtual bool IsThisTabListEditable() = 0;

  // Returns true if all tabs in this tab list are in the process of closing.
  virtual bool IsClosingAllTabs() = 0;

  // Returns true if the tab list is currently considered editable. This will
  // return false if *any* tab list for the given `profile` has a tab being
  // dragged / dropped. This is because, even if the tab list doesn't have a
  // tab that's being dragged from it, a different drag could be placed into it,
  // affecting the list.
  static bool CanEditTabList(Profile& profile);
};

namespace base {

template <>
struct ScopedObservationTraits<TabListInterface, TabListInterfaceObserver> {
  static void AddObserver(TabListInterface* tab_list,
                          TabListInterfaceObserver* observer) {
    tab_list->AddTabListInterfaceObserver(observer);
  }
  static void RemoveObserver(TabListInterface* tab_list,
                             TabListInterfaceObserver* observer) {
    tab_list->RemoveTabListInterfaceObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_H_
