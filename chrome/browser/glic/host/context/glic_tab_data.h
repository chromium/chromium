// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_

#include <optional>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;
namespace glic {

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
      base::RepeatingCallback<void(glic::mojom::TabDataPtr)> tab_data_changed);
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
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSetForMainFrame(
      content::RenderFrameHost* render_frame_host) override;

  // favicon::FaviconDriverObserver.
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  void SendUpdate();
  void ClearObservation();

  // Handler for TabInterface callback subscription.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  base::RepeatingCallback<void(glic::mojom::TabDataPtr)> tab_data_changed_;

  // Subscription to TabInterface detach callback.
  base::CallbackListSubscription tab_detach_subscription_;
};

// Either a focused tab, or an error string.
class FocusedTabData {
 public:
  explicit FocusedTabData(tabs::TabInterface* tab);
  // `unfocused_tab` can be nullptr. If it is not nullptr, it is the tab that
  // would be focused but for some reason cannot be.
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

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
