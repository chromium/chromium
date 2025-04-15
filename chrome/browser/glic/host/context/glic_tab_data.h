// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_

#include <optional>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

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
      bool observe_current_page_only,
      base::RepeatingCallback<void(glic::mojom::TabDataPtr)> tab_data_changed);
  ~TabDataObserver() override;
  TabDataObserver(const TabDataObserver&) = delete;
  TabDataObserver& operator=(const TabDataObserver&) = delete;

  // Returns the web contents being observed. Returns null if the web contents
  // was null originally, the web contents has been destroyed, or the primary
  // page has changed, and observe_current_page_only is true.
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

  bool observe_current_page_only_ = false;
  base::RepeatingCallback<void(glic::mojom::TabDataPtr)> tab_data_changed_;
};

// Data provided when there is no focused tab.
// The browser-side type corresponding to mojom::NoFocusedTabData.
struct NoFocusedTabData {
  explicit NoFocusedTabData(std::string_view reason,
                            content::WebContents* tab = nullptr);
  NoFocusedTabData();
  ~NoFocusedTabData();
  NoFocusedTabData(const NoFocusedTabData& src);
  NoFocusedTabData& operator=(const NoFocusedTabData& src);
  bool IsSame(const NoFocusedTabData& new_data) const;

  // The active tab that could not be focused, may be null.
  base::WeakPtr<content::WebContents> active_tab;
  // Human readable debug message about why there is no focused tab.
  std::string_view no_focus_reason;
};

// Either a focused web contents, or a NoFocusedTabData.
class FocusedTabData : public std::variant<base::WeakPtr<content::WebContents>,
                                           NoFocusedTabData> {
 public:
  FocusedTabData() = delete;  // Disallow the empty state.
  using variant::variant;

  bool is_focus() const {
    return std::holds_alternative<base::WeakPtr<content::WebContents>>(*this);
  }

  // Returns the focused tab web contents. Note that if FocusedTabData
  // represents a valid focus, this can still return nullptr if the web contents
  // has been deleted.
  content::WebContents* focus() const {
    const base::WeakPtr<content::WebContents>* focus = std::get_if<0>(this);
    return focus ? focus->get() : nullptr;
  }

  // Whether this FocusedTabData is the same as `new_data`. Note that this
  // returns true if both FocusedTabData point to two different invalidated web
  // contents.
  bool IsSame(const FocusedTabData& new_data) const;

  // Returns the focused web contents, or a human-readable message indicating
  // why there is none.
  base::expected<content::WebContents*, std::string_view> GetFocus() const;
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
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_H_
