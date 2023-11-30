// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_TAB_HELPER_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/dips/dips_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}

// TODO(rtarpine): remove dependence on DIPSService.
class DIPSState;

enum class OptionalBool {
  kUnknown = 0,
  kFalse = 1,
  kTrue = 2,
};

inline OptionalBool ToOptionalBool(bool b) {
  return b ? OptionalBool::kTrue : OptionalBool::kFalse;
}

// Observers a WebContents to detect pop-ups with user interaction, in order to
// grant storage access.
class OpenerHeuristicTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OpenerHeuristicTabHelper> {
 public:
  // Observes a WebContents which *is* a pop-up with opener access, to detect
  // user interaction and (TODO:) communicate with its opener. This is only
  // public so tests can see it.
  class PopupObserver : public content::WebContentsObserver {
   public:
    PopupObserver(content::WebContents* web_contents,
                  const GURL& initial_url,
                  base::WeakPtr<OpenerHeuristicTabHelper> opener);
    ~PopupObserver() override;

    // Set the time that the user previously interacted with this pop-up's site.
    void SetPastInteractionTime(base::Time time);

   private:
    // Emit the OpenerHeuristic.PopupPastInteraction UKM event if we have all
    // the necessary information, and create a storage access grant if
    // supported.
    void EmitPastInteractionIfReady();
    // Emit the OpenerHeuristic.TopLevel UKM event.
    void EmitTopLevel(const GURL& tracker_url,
                      OptionalBool has_iframe,
                      bool is_current_interaction);
    // Create a storage access grant, if eligible per experiment flags.
    void MaybeCreateOpenerHeuristicGrant(const GURL& url,
                                         base::TimeDelta grant_duration);
    // See if the opener page has an iframe from the same site.
    OptionalBool GetOpenerHasSameSiteIframe(const GURL& popup_url);

    // WebContentsObserver overrides:
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;
    void FrameReceivedUserActivation(
        content::RenderFrameHost* render_frame_host) override;

    const int32_t popup_id_;
    // The URL originally passed to window.open().
    const GURL initial_url_;
    // The top-level WebContents that opened this pop-up.
    base::WeakPtr<OpenerHeuristicTabHelper> opener_;
    const size_t opener_page_id_;
    // A UKM source id for the page that opened the pop-up.
    const ukm::SourceId opener_source_id_;
    // The URL of the page that opened the pop-up.
    const GURL opener_url_;
    // How long after the user last interacted with the site until the pop-up
    // opened.
    absl::optional<base::TimeDelta> time_since_interaction_;
    // A source ID for `initial_url_`.
    absl::optional<ukm::SourceId> initial_source_id_;
    absl::optional<base::Time> commit_time_;
    size_t url_index_ = 0;
    bool interaction_reported_ = false;
    bool toplevel_reported_ = false;

    // Whether the last committed navigation in this popup WCO is ad-tagged.
    // Used for UKM metrics and for gating storage access grant creation (under
    // a flag).
    bool is_last_navigation_ad_tagged_ = false;

    scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  };

  ~OpenerHeuristicTabHelper() override;

  size_t page_id() const { return page_id_; }

  static base::Clock* SetClockForTesting(base::Clock* clock);

  PopupObserver* popup_observer_for_testing() { return popup_observer_.get(); }

 private:
  // To allow use of WebContentsUserData::CreateForWebContents()
  friend class content::WebContentsUserData<OpenerHeuristicTabHelper>;

  explicit OpenerHeuristicTabHelper(content::WebContents* web_contents);

  // Called when the observed WebContents is a popup.
  void InitPopup(const GURL& popup_url,
                 base::WeakPtr<OpenerHeuristicTabHelper> opener);
  // Asynchronous callback for reading past interaction timestamps from the
  // DIPSService.
  void GotPopupDipsState(const DIPSState& state);
  // Record a OpenerHeuristic.PostPopupCookieAccess event, if there has been a
  // corresponding popup event for the provided source and target sites.
  void OnCookiesAccessed(const ukm::SourceId& source_id,
                         const content::CookieAccessDetails& details);
  void EmitPostPopupCookieAccess(const ukm::SourceId& source_id,
                                 const content::CookieAccessDetails& details,
                                 absl::optional<PopupsStateValue> value);
  // Check whether `source_render_frame_host` is a valid popup initiator frame,
  // per the experiment flags.
  bool PassesIframeInitiatorCheck(
      content::RenderFrameHost* source_render_frame_host);

  // WebContentsObserver overrides:
  void PrimaryPageChanged(content::Page& page) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  // To detect whether the user navigated away from the opener page before
  // interacting with a popup, we increment this ID on each committed
  // navigation, and compare at the time of the interaction.
  size_t page_id_ = 0;
  // Populated only when the observed WebContents is a pop-up.
  std::unique_ptr<PopupObserver> popup_observer_;
  base::WeakPtrFactory<OpenerHeuristicTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_TAB_HELPER_H_
