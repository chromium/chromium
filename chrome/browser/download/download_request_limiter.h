// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

// DownloadRequestLimiter is responsible for determining whether a download
// should be allowed or not. It is designed to keep pages from downloading
// multiple files without user interaction. DownloadRequestLimiter is invoked
// from ResourceDispatcherHost any time a download begins
// (CanDownload). The request is processed on the UI thread, and the
// request is notified (back on the IO thread) as to whether the download should
// be allowed or denied.
//
// Invoking CanDownload notifies the callback and may update the
// download status. The following details the various states:
// . Each NavigationController initially starts out allowing a download
//   (ALLOW_ONE_DOWNLOAD).
// . The first time CanDownload is invoked the download is allowed and
//   the state changes to PROMPT_BEFORE_DOWNLOAD.
// . If the state is PROMPT_BEFORE_DOWNLOAD and the user clicks the mouse,
//   presses enter, the space bar or navigates to another page the state is
//   reset to ALLOW_ONE_DOWNLOAD.
// . If a download is attempted and the state is PROMPT_BEFORE_DOWNLOAD the user
//   is prompted as to whether the download is allowed or disallowed. The users
//   choice stays until the user navigates to a different host. For example, if
//   the user allowed the download, multiple downloads are allowed without any
//   user intervention until the user navigates to a different host.
//
// The DownloadUiStatus indicates whether omnibox UI should be shown for the
// current download status. We do not show UI if there has not yet been
// a download attempt on the page regardless of the internal download status.
class DownloadRequestLimiter
    : public base::RefCountedThreadSafe<DownloadRequestLimiter> {
 public:
  // Download status for a particular page. See class description for details.
  enum DownloadStatus {
    ALLOW_ONE_DOWNLOAD,
    PROMPT_BEFORE_DOWNLOAD,
    ALLOW_ALL_DOWNLOADS,
    DOWNLOADS_NOT_ALLOWED
  };

  // Download UI state given the current download status for a page.
  enum DownloadUiStatus {
    DOWNLOAD_UI_DEFAULT,
    DOWNLOAD_UI_ALLOWED,
    DOWNLOAD_UI_BLOCKED
  };

  // Max number of downloads before a "Prompt Before Download" Dialog is shown.
  static const size_t kMaxDownloadsAtOnce = 50;

  // The callback from CanDownload. This is invoked on the IO thread.
  // The boolean parameter indicates whether or not the download is allowed.
  using Callback = base::OnceCallback<void(bool /*allow*/)>;

  // TabDownloadState maintains the download state for a particular tab.
  // TabDownloadState prompts the user with an infobar as necessary.
  // TabDownloadState deletes itself (by invoking
  // DownloadRequestLimiter::Remove) as necessary.
  // TODO(gbillock): just make this class implement
  // permissions::PermissionRequest.
  class TabDownloadState : public content_settings::Observer,
                           public content::WebContentsObserver {
   public:
    // Creates a new TabDownloadState. |host| is DownloadRequestLimiter object
    // that owns this object. This object will listen to all the navigations
    // and downloads happening on the |web_contents| to determine the new
    // download status.
    TabDownloadState(DownloadRequestLimiter* host,
                     content::WebContents* web_contents);

    TabDownloadState(const TabDownloadState&) = delete;
    TabDownloadState& operator=(const TabDownloadState&) = delete;

    ~TabDownloadState() override;

    // Sets the current limiter state and the underlying automatic downloads
    // content setting. Sends a notification that the content setting has been
    // changed (if it has changed).
    void SetDownloadStatusAndNotify(const url::Origin& request_origin,
                                    DownloadStatus status);

    // Status of the download.
    DownloadStatus download_status() const { return status_; }

    // The omnibox UI to be showing (or none if we shouldn't show any).
    DownloadUiStatus download_ui_status() const { return ui_status_; }

    // Number of "ALLOWED" downloads.
    void increment_download_count() {
      download_count_++;
    }
    size_t download_count() const {
      return download_count_;
    }

    const url::Origin& origin() const { return origin_; }

    bool download_seen() const { return download_seen_; }
    void set_download_seen() { download_seen_ = true; }

    // content::WebContentsObserver overrides.
    void DidStartNavigation(
        content::NavigationHandle* navigation_handle) override;
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;
    void DidGetUserInteraction(const blink::WebInputEvent& event) override;
    void WebContentsDestroyed() override;

    // Asks the user if they really want to allow the download.
    // See description above CanDownload for details on lifetime of callback.
    void PromptUserForDownload(DownloadRequestLimiter::Callback callback,
                               const url::Origin& request_origin);

    // Invoked from DownloadRequestDialogDelegate. Notifies the delegates and
    // changes the status appropriately. Virtual for testing.
    virtual void Cancel(const url::Origin& request_origin);
    virtual void CancelOnce(const url::Origin& request_origin);
    virtual void Accept(const url::Origin& request_origin);

    DownloadStatus GetDownloadStatus(const url::Origin& request_origin);

   protected:
    // Used for testing.
    TabDownloadState();

   private:
    // Are we showing a prompt to the user?  Determined by whether
    // we have an outstanding weak pointer--weak pointers are only
    // given to the info bar delegate or permission bubble request.
    bool is_showing_prompt() const;

    // This may result in invoking Remove on DownloadRequestLimiter.
    void OnUserInteraction();

    // content_settings::Observer overrides.
    void OnContentSettingChanged(
        const ContentSettingsPattern& primary_pattern,
        const ContentSettingsPattern& secondary_pattern,
        ContentSettingsTypeSet content_type_set) override;

    // Remember to either block or allow automatic downloads from
    // |request_origin|.
    void SetContentSetting(ContentSetting setting,
                           const url::Origin& request_origin);

    // Notifies the callbacks as to whether the download is allowed or not.
    // Returns false if it didn't notify all callbacks.
    bool NotifyCallbacks(bool allow);

    // Set the download limiter state and notify if it has changed. Callers must
    // guarantee that |status| and |setting| correspond to each other.
    void SetDownloadStatusAndNotifyImpl(const url::Origin& request_origin,
                                        DownloadStatus status,
                                        ContentSetting setting);

    // Check if the navigation should clear the download state. If an origin is
    // in a limited state, history forward/backward shouldn't clear the download
    // state.
    bool shouldClearDownloadState(content::NavigationHandle* navigation_handle);

    raw_ptr<content::WebContents> web_contents_;

    raw_ptr<DownloadRequestLimiter> host_;

    // Current tab status and UI status. Renderer initiated navigations will
    // not change these values if the current tab state is restricted.
    DownloadStatus status_;
    DownloadUiStatus ui_status_;

    // Origin for initiating the current download. The value was kept for
    // updating the omnibox decoration.
    url::Origin origin_;

    size_t download_count_;

    // True if a download has been seen on the current page load.
    bool download_seen_;

    // Callbacks we need to notify. This is only non-empty if we're showing a
    // dialog.
    // See description above CanDownload for details on lifetime of callbacks.
    std::vector<DownloadRequestLimiter::Callback> callbacks_;

    // Origins that have non-default download state.
    using DownloadStatusMap = std::map<url::Origin, DownloadStatus>;
    DownloadStatusMap download_status_map_;

    base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
        observation_{this};

    // Weak pointer factory for generating a weak pointer to pass to the
    // infobar.  User responses to the throttling prompt will be returned
    // through this channel, and it can be revoked if the user prompt result
    // becomes moot.
    base::WeakPtrFactory<DownloadRequestLimiter::TabDownloadState> factory_{
        this};
  };

  DownloadRequestLimiter();

  DownloadRequestLimiter(const DownloadRequestLimiter&) = delete;
  DownloadRequestLimiter& operator=(const DownloadRequestLimiter&) = delete;

  // Returns the download status for a page. This does not change the state in
  // anyway.
  DownloadStatus GetDownloadStatus(content::WebContents* web_contents);

  // Returns the download UI status for a page for the purposes of showing an
  // omnibox decoration.
  DownloadUiStatus GetDownloadUiStatus(content::WebContents* web_contents);

  // Returns the download origin that is associated with the current UI status
  // for the purposes of showing an omnibox decoration.
  GURL GetDownloadOrigin(content::WebContents* web_contents);

  // Check if download can proceed and notifies the callback on UI thread.
  void CanDownload(const content::WebContents::Getter& web_contents_getter,
                   const GURL& url,
                   const std::string& request_method,
                   std::optional<url::Origin> request_initiator,
                   bool from_download_cross_origin_redirect,
                   Callback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadTest, DownloadResourceThrottleCancels);
  FRIEND_TEST_ALL_PREFIXES(DownloadTest,
                           DownloadRequestLimiterDisallowsAnchorDownloadTag);
  FRIEND_TEST_ALL_PREFIXES(DownloadTest,
                           CrossOriginRedirectDownloadFromAnchorDownload);
  FRIEND_TEST_ALL_PREFIXES(
      DownloadTest,
      MultipleCrossOriginRedirectDownloadsFromAnchorDownload);
  FRIEND_TEST_ALL_PREFIXES(DownloadTest, MultipleDownloadsFromIframeSrcdoc);
  FRIEND_TEST_ALL_PREFIXES(ContentSettingBubbleControllerTest, Init);
  FRIEND_TEST_ALL_PREFIXES(ContentSettingImageModelBrowserTest,
                           CreateBubbleModel);
  FRIEND_TEST_ALL_PREFIXES(PrerenderDownloadTest,
                           DownloadRequestLimiterIsUnaffectedByPrerendering);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameDownloadTest,
                           DownloadRequestLimiterIsUnaffectedByFencedFrame);
  friend class base::RefCountedThreadSafe<DownloadRequestLimiter>;
  friend class BackgroundFetchBrowserTest;
  friend class ContentSettingBubbleDialogTest;
  friend class DownloadRequestLimiterTest;
  friend class TabDownloadState;

  ~DownloadRequestLimiter();

  // Gets the download state for the specified controller. If the
  // TabDownloadState does not exist and |create| is true, one is created.
  // See TabDownloadState's constructor description for details on the two
  // controllers.
  //
  // The returned TabDownloadState is owned by the DownloadRequestLimiter and
  // deleted when no longer needed (the Remove method is invoked).
  TabDownloadState* GetDownloadState(content::WebContents* web_contents,
                                     bool create);

  // Does the work of updating the download status on the UI thread and
  // potentially prompting the user.
  void CanDownloadImpl(const GURL& url,
                       content::WebContents* originating_contents,
                       const std::string& request_method,
                       std::optional<url::Origin> request_initiator,
                       bool from_download_cross_origin_redirect,
                       Callback callback);

  // Invoked when decision to download has been made.
  void OnCanDownloadDecided(
      const GURL& url,
      const content::WebContents::Getter& web_contents_getter,
      const std::string& request_method,
      std::optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      Callback orig_callback,
      bool allow);

  // Removes the specified TabDownloadState from the internal map and deletes
  // it. This has the effect of resetting the status for the tab to
  // ALLOW_ONE_DOWNLOAD.
  void Remove(TabDownloadState* state, content::WebContents* contents);

  static HostContentSettingsMap* GetContentSettings(
      content::WebContents* contents);

  // Gets the content setting for a particular request initiator.
  static ContentSetting GetAutoDownloadContentSetting(
      content::WebContents* contents,
      const GURL& request_initiator);

  // Sets the callback for tests to know the result of OnCanDownloadDecided().
  using CanDownloadDecidedCallback =
      base::RepeatingCallback<void(bool /*allow*/)>;
  void SetOnCanDownloadDecidedCallbackForTesting(
      CanDownloadDecidedCallback callback);

  // TODO(bauerb): Change this to use WebContentsUserData.
  // Maps from tab to download state. The download state for a tab only exists
  // if the state is other than ALLOW_ONE_DOWNLOAD. Similarly once the state
  // transitions from anything but ALLOW_ONE_DOWNLOAD back to ALLOW_ONE_DOWNLOAD
  // the TabDownloadState is removed and deleted (by way of Remove).
  typedef std::map<content::WebContents*,
                   raw_ptr<TabDownloadState, CtnExperimental>>
      StateMap;
  StateMap state_map_;

  CanDownloadDecidedCallback on_can_download_decided_callback_;

  // Weak ptr factory used when |CanDownload| asks the delegate asynchronously
  // about the download.
  base::WeakPtrFactory<DownloadRequestLimiter> factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_REQUEST_LIMITER_H_
