// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/glic.mojom.h"
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

struct FocusedTabCandidate {
  FocusedTabCandidate(
      content::WebContents* const web_contents,
      glic::mojom::InvalidCandidateError invalid_candidate_error);
  ~FocusedTabCandidate();
  FocusedTabCandidate(const FocusedTabCandidate& other);
  FocusedTabCandidate(FocusedTabCandidate&& other) noexcept;
  FocusedTabCandidate& operator=(const FocusedTabCandidate& other);
  FocusedTabCandidate& operator=(FocusedTabCandidate&& other) noexcept;
  bool operator==(const FocusedTabCandidate& other) const;

  base::WeakPtr<content::WebContents> focused_tab_candidate_contents = nullptr;
  glic::mojom::InvalidCandidateError invalid_candidate_error;
};

struct FocusedTabData {
  FocusedTabData(
      content::WebContents* const web_contents,
      std::optional<glic::mojom::InvalidCandidateError> invalid_candidate_error,
      std::optional<glic::mojom::NoCandidateTabError> no_candidate_tab_error);
  ~FocusedTabData();
  FocusedTabData(const FocusedTabData& other);
  FocusedTabData(FocusedTabData&& other) noexcept;
  FocusedTabData& operator=(const FocusedTabData& other);
  FocusedTabData& operator=(FocusedTabData&& other) noexcept;
  bool operator==(const FocusedTabData& other) const;

  base::WeakPtr<content::WebContents> focused_tab_contents = nullptr;
  std::optional<FocusedTabCandidate> focused_tab_candidate;
  std::optional<glic::mojom::NoCandidateTabError> no_candidate_tab_error;
};

// Populates and returns a TabDataPtr from a given WebContents, or null if
// web_contents is null.
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents);

// Populates and returns a FocusedTabDataPtr from a given FocusedTabData.
glic::mojom::FocusedTabDataPtr CreateFocusedTabData(
    FocusedTabData focused_tab_data);
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
