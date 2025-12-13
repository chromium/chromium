// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_

#include <optional>
#include <ostream>
#include <variant>

#include "base/callback_list.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;
namespace base {
class SequencedTaskRunner;
}
namespace glic {

// Indicates what caused the tab data to change. This can be used for debugging,
// and for deciding how to forward changes.
enum class TabDataChangeCause : uint32_t {
  kMinValue = 0,
  // Tab / web contents properties that changed.
  kFavicon = 0,
  kTitle = 1,
  kAudioState = 2,
  kVisibility = 3,

  // Events.
  kSameDocNavigation = 4,
  kCrossDocNavigation = 5,
  // It's a different tab being returned.
  kTabChanged = 6,

  kMaxValue = kTabChanged,
};
using TabDataChangeCauseSet = base::EnumSet<TabDataChangeCause,
                                            TabDataChangeCause::kMinValue,
                                            TabDataChangeCause::kMaxValue>;

std::ostream& operator<<(std::ostream& os, const TabDataChangeCause& cause);

struct TabDataChange {
  TabDataChange();
  TabDataChange(TabDataChangeCauseSet causes, glic::mojom::TabDataPtr tab_data);
  ~TabDataChange();
  TabDataChange(TabDataChange&& src);
  TabDataChange& operator=(TabDataChange&& src);

  TabDataChangeCauseSet causes;
  glic::mojom::TabDataPtr tab_data;
};
std::ostream& operator<<(std::ostream& os, const TabDataChange& change);

// TODO: Detect changes to windowID.
class TabDataObserver : public content::WebContentsObserver,
                        public favicon::FaviconDriverObserver {
 public:
  // Observes `web_contents` for changes that would modify the result of
  // `CreateTabData(web_contents)`. `tab_data_changed` is called any time tab
  // data may have changed.
  // If `observe_current_page_only` is true, TabDataObserver will automatically
  // stop providing updates if the primary page changes.
  TabDataObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void(TabDataChange)> tab_data_changed);

  TabDataObserver(
      tabs::TabInterface* tab,
      content::WebContents* web_contents,
      base::RepeatingCallback<void(TabDataChange)> tab_data_changed);

  ~TabDataObserver() override;
  TabDataObserver(const TabDataObserver&) = delete;
  TabDataObserver& operator=(const TabDataObserver&) = delete;

  // Returns the web contents being observed. Returns null if the web contents
  // was null originally or the web contents has been destroyed.
  content::WebContents* web_contents() {
    // const_cast is safe because a non-const WebContents is passed in this
    // class's constructor.
    return const_cast<content::WebContents*>(
        content::WebContentsObserver::web_contents());
  }

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSetForMainFrame(
      content::RenderFrameHost* render_frame_host) override;

  // favicon::FaviconDriverObserver.
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void ReportUpdatesPerNavigation();
  void SendRateLimitedUpdate();
  void SendUpdate();
  void ClearObservation();

  // Handler for TabInterface callback subscription.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  base::OneShotTimer deferred_update_;
  size_t updates_since_navigation_ = 0;
  TabDataChangeCauseSet change_causes_;
  base::RepeatingCallback<void(TabDataChange)> tab_data_changed_;

  // Subscription to TabInterface detach callback.
  base::CallbackListSubscription tab_detach_subscription_;
};

// Either a focused tab, or an error string.
class FocusedTabData {
 public:
  // Creates FocusedTabData for a tab that can be focused.
  explicit FocusedTabData(tabs::TabInterface* tab);
  // Creates FocusedTabData when a tab cannot be focused. If `unfocused_tab` is
  // provided, it represents the tab that would be focused but for some reason
  // cannot be. If `unfocused_tab` is null there is no tab that could be
  // focused.
  FocusedTabData(const std::string& error, tabs::TabInterface* unfocused_tab);
  ~FocusedTabData();
  FocusedTabData(const FocusedTabData& src) = delete;
  FocusedTabData& operator=(const FocusedTabData& src) = delete;
  bool is_focus() const {
    return std::holds_alternative<tabs::TabInterface*>(data_);
  }

  // Returns the focused tab or nullptr.
  tabs::TabInterface* focus() const {
    return is_focus() ? std::get<0>(data_) : nullptr;
  }

  // Returns the focused web contents, or a human-readable message indicating
  // why there is none.
  base::expected<tabs::TabInterface*, std::string> GetFocus() const;
  tabs::TabInterface* unfocused_tab() const { return unfocused_tab_.get(); }

 private:
  std::variant<tabs::TabInterface*, std::string> data_;

  // Only see if `data_` is string variant.
  raw_ptr<tabs::TabInterface> unfocused_tab_;
};

// Helper function to extract the Tab Id from the current web contents.
int GetTabId(content::WebContents* web_contents);

// Helper function to extract the Tab url from the current web contents.
const GURL& GetTabUrl(content::WebContents* web_contents);

// Populates and returns a TabDataPtr from a given WebContents, or null if
// web_contents is null.
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents);

// Populates and returns a FocusedTabDataPtr from a given FocusedTabData.
glic::mojom::FocusedTabDataPtr CreateFocusedTabData(
    const FocusedTabData& focused_tab_data);

// Checks if two SkBitmap images -- used for favicons -- are visually the same.
// This is not a highly optimized comparison but should be good enough for
// comparing (small) favicon images.
bool FaviconEquals(const ::SkBitmap& a, const ::SkBitmap& b);

std::string TabDataDebugString(const mojom::TabData& tab_data);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
