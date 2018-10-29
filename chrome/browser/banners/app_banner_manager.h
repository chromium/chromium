// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "chrome/browser/installable/installable_ambient_badge_infobar_delegate.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/modules/app_banner/app_banner.mojom.h"

class InstallableManager;
class SkBitmap;
struct WebApplicationInfo;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

// This forward declaration exists solely for the DidFinishCreatingBookmarkApp
// callback, implemented and called on desktop platforms only.
namespace extensions {
class Extension;
}

namespace banners {

// Coordinates the creation of an app banner, from detecting eligibility to
// fetching data and creating the infobar. Sites declare that they want an app
// banner using the web app manifest. One web/native app may occupy the pipeline
// at a time; navigation resets the manager and discards any work in progress.
//
// This class contains the generic functionality shared between all platforms,
// as well as no-op callbacks that the platform-specific implementations pass to
// base::Bind. This allows a WeakPtrFactory to be housed in this class.
//
// The InstallableManager fetches and validates whether a site is eligible for
// banners. The manager is first called to fetch the manifest, so we can verify
// whether the site is already installed (and on Android, divert the flow to a
// native app banner if requested). The second call completes the checking for a
// web app banner (checking manifest validity, service worker, and icon).
class AppBannerManager : public content::WebContentsObserver,
                         public blink::mojom::AppBannerService,
                         public InstallableAmbientBadgeInfoBarDelegate::Client,
                         public SiteEngagementObserver {
 public:
  // A StatusReporter handles the reporting of |InstallableStatusCode|s.
  class StatusReporter;

  enum class State {
    // The pipeline has not yet been triggered for this page load.
    INACTIVE,

    // The pipeline is running for this page load.
    ACTIVE,

    // The pipeline is waiting for the web app manifest to be fetched.
    FETCHING_MANIFEST,

    // The pipeline is waiting for native app data to be fetched.
    FETCHING_NATIVE_DATA,

    // The pipeline is waiting for the installability criteria to be checked.
    // In this state, the pipeline could be paused while waiting for a service
    // worker to be registered..
    PENDING_INSTALLABLE_CHECK,

    // The pipeline has finished running, but is waiting for sufficient
    // engagement to trigger the banner.
    PENDING_ENGAGEMENT,

    // The beforeinstallprompt event has been sent and the pipeline is waiting
    // for the response.
    SENDING_EVENT,

    // The beforeinstallprompt event was sent, and the web page called prompt()
    // on the event while the event was being handled.
    SENDING_EVENT_GOT_EARLY_PROMPT,

    // The pipeline has finished running, but is waiting for the web page to
    // call prompt() on the event.
    PENDING_PROMPT,

    // The pipeline has finished running for this page load and no more
    // processing is to be done.
    COMPLETE,
  };

  // Installable describes whether a site satisifes the installablity
  // requirements.
  enum class Installable { INSTALLABLE_YES, INSTALLABLE_NO, UNKNOWN };

  // Returns the current time.
  static base::Time GetCurrentTime();

  // Fast-forwards the current time for testing.
  static void SetTimeDeltaForTesting(int days);

  // Sets the total engagement required for triggering the banner in testing.
  static void SetTotalEngagementToTrigger(double engagement);

  // Returns whether the new experimental flow and UI is enabled.
  static bool IsExperimentalAppBannersEnabled();

  // Returns the app name if the current page is installable, otherwise returns
  // the empty string.
  static base::string16 GetInstallableAppName(
      content::WebContents* web_contents);

  // Requests an app banner. If |is_debug_mode| is true, any failure in the
  // pipeline will be reported to the devtools console.
  virtual void RequestAppBanner(const GURL& validated_url, bool is_debug_mode);

  // Informs the page that it has been installed with appinstalled event and
  // performs logging related to the app installation. Appinstalled event is
  // redundant for the beforeinstallprompt event's promise being resolved, but
  // is required by the install event spec.
  void OnInstall(bool is_native, blink::WebDisplayMode display);

  // Sends a message to the renderer that the user accepted the banner.
  void SendBannerAccepted();

  // Sends a message to the renderer that the user dismissed the banner.
  virtual void SendBannerDismissed();

  // Returns a WeakPtr to this object. Exposed so subclasses/infobars may
  // may bind callbacks without needing their own WeakPtrFactory.
  base::WeakPtr<AppBannerManager> GetWeakPtr();

  // Overridden and passed through base::Bind on desktop platforms. Called when
  // the bookmark app install initiated by a banner has completed. Not used on
  // Android.
  virtual void DidFinishCreatingBookmarkApp(
      const extensions::Extension* extension,
      const WebApplicationInfo& web_app_info) {}

  // Overridden and passed through base::Bind on Android. Called when the
  // download of a native app's icon is complete, as native banners use an icon
  // provided from the Play Store rather than the web manifest. Not used on
  // desktop platforms.
  virtual void OnAppIconFetched(const SkBitmap& bitmap) {}

  // InstallableAmbientBadgeInfoBarDelegate::Client overrides. Further
  // overridden on Android.
  void AddToHomescreenFromBadge() override {}
  void BadgeDismissed() override {}

 protected:
  explicit AppBannerManager(content::WebContents* web_contents);
  ~AppBannerManager() override;

  // Returns true if the banner should be shown. Returns false if the banner has
  // been shown too recently, or if the app has already been installed.
  // GetAppIdentifier() must return a valid value for this method to work.
  bool CheckIfShouldShowBanner();

  // Called when the current site is eligible to show a banner. Returns true if
  // the banner should not be shown because the site is already installed, and
  // false if the banner should be shown because the site is not yet installed.
  // Overridden in platform-specific code to perform actions when it is
  // guaranteed that a site is banner-eligible, depending on whether the site is
  // installed (i.e. the ambient badge).
  virtual bool CheckIfInstalled();

  // Return a string identifying this app for metrics.
  virtual std::string GetAppIdentifier();

  // Return the name of the app for this page.
  virtual base::string16 GetAppName() const;

  // Return a string describing what type of banner is being created. Used when
  // alerting websites that a banner is about to be created.
  virtual std::string GetBannerType();

  // Returns true if |has_sufficient_engagement_| is true or IsDebugMode()
  // returns true.
  bool HasSufficientEngagement() const;

  // Returns true if |triggered_by_devtools_| is true or the
  // kBypassAppBannerEngagementChecks flag is set.
  bool IsDebugMode() const;

  // Returns true if the webapp at |start_url| has already been installed, or
  // should be considered installed. On Android, we rely on a heuristic that
  // may yield false negatives or false positives (crbug.com/786268).
  virtual bool IsWebAppConsideredInstalled(content::WebContents* web_contents,
                                           const GURL& validated_url,
                                           const GURL& start_url,
                                           const GURL& manifest_url);

  // Callback invoked by the InstallableManager once it has fetched the page's
  // manifest.
  virtual void OnDidGetManifest(const InstallableData& result);

  // Returns an InstallableParams object that requests all checks necessary for
  // a web app banner.
  virtual InstallableParams ParamsToPerformInstallableCheck();

  // Run at the conclusion of OnDidGetManifest. For web app banners, this calls
  // back to the InstallableManager to continue checking criteria. For native
  // app banners, this checks whether native apps are preferred in the manifest,
  // and calls to Java to verify native app details. If a native banner isn't or
  // can't be requested, it continues with the web app banner checks.
  virtual void PerformInstallableCheck();

  // Callback invoked by the InstallableManager once it has finished checking
  // all other installable properties.
  virtual void OnDidPerformInstallableCheck(const InstallableData& result);

  // Records that a banner was shown. The |event_name| corresponds to the RAPPOR
  // metric being recorded.
  void RecordDidShowBanner(const std::string& event_name);

  // Reports |code| via a UMA histogram or logs it to the console.
  void ReportStatus(InstallableStatusCode code);

  // Voids all outstanding service pointers.
  void ResetBindings();

  // Resets all fetched data for the current page.
  virtual void ResetCurrentPageData();

  // Stops the banner pipeline early.
  void Terminate();

  // Stops the banner pipeline, preventing any outstanding callbacks from
  // running and resetting the manager state. This method is virtual to allow
  // tests to intercept it and verify correct behaviour.
  virtual void Stop(InstallableStatusCode code);

  // Sends a message to the renderer that the page has met the requirements to
  // show a banner. The page can respond to cancel the banner (and possibly
  // display it later), or otherwise allow it to be shown.
  void SendBannerPromptRequest();

  // Updates the current state to |state|. Virtual to allow overriding in tests.
  virtual void UpdateState(State state);

  // content::WebContentsObserver overrides.
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;
  void WebContentsDestroyed() override;

  // SiteEngagementObserver overrides.
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         SiteEngagementService::EngagementType type) override;

  // Subclass accessors for private fields which should not be changed outside
  // this class.
  InstallableManager* manager() const { return manager_; }
  State state() const { return state_; }
  bool IsRunning() const;

  // The URL for which the banner check is being conducted.
  GURL validated_url_;

  // The URL of the manifest.
  GURL manifest_url_;

  // The manifest object.
  blink::Manifest manifest_;

  // The URL of the primary icon.
  GURL primary_icon_url_;

  // The primary icon object.
  SkBitmap primary_icon_;

  // The referrer string (if any) specified in the app URL. Used only for native
  // app banners.
  std::string referrer_;

  // The current banner pipeline state for this page load.
  State state_;

 private:
  friend class AppBannerManagerTest;

  // Retrieves the platform specific instance of AppBannerManager from
  // |web_contents|.
  static AppBannerManager* FromWebContents(content::WebContents* web_contents);

  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  void RecordCouldShowBanner();

  // Creates the app banner UI. Overridden by subclasses as the infobar is
  // platform-specific.
  virtual void ShowBannerUi(WebappInstallSource install_source) = 0;

  // Called after the manager sends a message to the renderer regarding its
  // intention to show a prompt. The renderer will send a message back with the
  // opportunity to cancel.
  virtual void OnBannerPromptReply(
      blink::mojom::AppBannerControllerPtr controller,
      blink::mojom::AppBannerPromptReply reply,
      const std::string& referrer);

  // Does the non-platform specific parts of showing the app banner.
  void ShowBanner();

  // blink::mojom::AppBannerService overrides.
  // Called when Blink has prevented a banner from being shown, and is now
  // requesting that it be shown later.
  void DisplayAppBanner() override;

  // Returns an InstallableStatusCode indicating whether a banner should be
  // shown.
  InstallableStatusCode ShouldShowBannerCode();

  // Returns a status code based on the current state, to log when terminating.
  InstallableStatusCode TerminationCode() const;

  // Fetches the data required to display a banner for the current page.
  InstallableManager* manager_;

  // We do not want to trigger a banner when the manager is attached to
  // a WebContents that is playing video. Banners triggering on a site in the
  // background will appear when the tab is reactivated.
  std::vector<MediaPlayerId> active_media_players_;

  // Mojo bindings and interface pointers.
  mojo::Binding<blink::mojom::AppBannerService> binding_;
  blink::mojom::AppBannerEventPtr event_;

  // If a banner is requested before the page has finished loading, defer
  // triggering the pipeline until the load is complete.
  bool has_sufficient_engagement_;
  bool load_finished_;

  // Whether the current flow was begun via devtools.
  bool triggered_by_devtools_;

  std::unique_ptr<StatusReporter> status_reporter_;
  Installable installable_;

  // The concrete subclasses of this class are expected to have their lifetimes
  // scoped to the WebContents which they are observing. This allows us to use
  // weak pointers for callbacks.
  base::WeakPtrFactory<AppBannerManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManager);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
