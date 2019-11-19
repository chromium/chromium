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
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

enum class WebappInstallSource;
class InstallableManager;
class SkBitmap;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace banners {

// Coordinates the creation of an app banner, from detecting eligibility to
// fetching data and creating the infobar. Sites declare that they want an app
// banner using the web app manifest. One web/native app may occupy the pipeline
// at a time; navigation resets the manager and discards any work in progress.
//
// The InstallableManager fetches and validates whether a site is eligible for
// banners. The manager is first called to fetch the manifest, so we can verify
// whether the site is already installed (and on Android, divert the flow to a
// native app banner if requested). The second call completes the checking for a
// web app banner (checking manifest validity, service worker, and icon).
class AppBannerManager : public content::WebContentsObserver,
                         public blink::mojom::AppBannerService,
                         public SiteEngagementObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAppBannerManagerChanged(AppBannerManager* new_manager) = 0;
    virtual void OnInstallableWebAppStatusUpdated() = 0;
  };

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

  // Installable describes to what degree a site satisifes the installablity
  // requirements.
  enum class InstallableWebAppCheckResult {
    kUnknown,
    kNo,
    kByUserRequest,
    kPromotable,
  };

  // Retrieves the platform specific instance of AppBannerManager from
  // |web_contents|.
  static AppBannerManager* FromWebContents(content::WebContents* web_contents);

  // Returns the current time.
  static base::Time GetCurrentTime();

  // Fast-forwards the current time for testing.
  static void SetTimeDeltaForTesting(int days);

  // Sets the total engagement required for triggering the banner in testing.
  static void SetTotalEngagementToTrigger(double engagement);

  // TODO(https://crbug.com/930612): Move |GetInstallableAppName| and
  // |IsWebContentsInstallable| out into a more general purpose installability
  // check class.

  // Returns the app name if the current page is installable, otherwise returns
  // the empty string.
  static base::string16 GetInstallableWebAppName(
      content::WebContents* web_contents);

  // Returns whether installability checks satisfy promotion requirements
  // (e.g. having a service worker fetch event) or have passed previously within
  // the current manifest scope.
  bool IsProbablyPromotableWebApp() const;

  // Each successful installability check gets to show one animation prompt,
  // this returns and consumes the animation prompt if it is available.
  bool MaybeConsumeInstallAnimation();

  // Requests an app banner.
  virtual void RequestAppBanner(const GURL& validated_url);

  // Informs the page that it has been installed with appinstalled event and
  // performs logging related to the app installation. Appinstalled event is
  // redundant for the beforeinstallprompt event's promise being resolved, but
  // is required by the install event spec.
  // This is virtual for testing.
  virtual void OnInstall(blink::mojom::DisplayMode display);

  // Sends a message to the renderer that the user accepted the banner.
  void SendBannerAccepted();

  // Sends a message to the renderer that the user dismissed the banner.
  void SendBannerDismissed();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual base::WeakPtr<AppBannerManager> GetWeakPtr() = 0;

  // Used by test subclasses that replace the existing AppBannerManager
  // instance. The observer list must be transferred over to avoid dangling
  // pointers in the observers.
  void MigrateObserverListForTesting(content::WebContents* web_contents);

  // Returns whether the site can call "event.prompt()" to prompt the user to
  // install the site.
  bool IsPromptAvailableForTesting() const;

  InstallableWebAppCheckResult GetInstallableWebAppCheckResultForTesting();

 protected:
  explicit AppBannerManager(content::WebContents* web_contents);
  ~AppBannerManager() override;

  // Returns true if the banner should be shown. Returns false if the banner has
  // been shown too recently, or if the app has already been installed.
  // GetAppIdentifier() must return a valid value for this method to work.
  bool CheckIfShouldShowBanner();

  // Returns whether the site would prefer a related application be installed
  // instead of the PWA or a related application is already installed.
  bool ShouldDeferToRelatedApplication() const;

  // Return a string identifying this app for metrics.
  virtual std::string GetAppIdentifier();

  // Return the name of the app for this page.
  virtual base::string16 GetAppName() const;

  // Return a string describing what type of banner is being created. Used when
  // alerting websites that a banner is about to be created.
  virtual std::string GetBannerType();

  virtual void InvalidateWeakPtrs() = 0;

  // Returns true if |has_sufficient_engagement_| is true or
  // ShouldBypassEngagementChecks() returns true.
  bool HasSufficientEngagement() const;

  // Returns true if the kBypassAppBannerEngagementChecks flag is set.
  bool ShouldBypassEngagementChecks() const;

  // Returns whether installation of apps from |platform| is supported on the
  // current device.
  virtual bool IsSupportedAppPlatform(const base::string16& platform) const = 0;

  // Returns whether |related_app| is already installed.
  virtual bool IsRelatedAppInstalled(
      const blink::Manifest::RelatedApplication& related_app) const = 0;

  // Returns whether the current page is already installed as a web app, or
  // should be considered installed. On Android, we rely on a heuristic that
  // may yield false negatives or false positives (crbug.com/786268).
  virtual bool IsWebAppConsideredInstalled();

  // Returns whether the installed web app at the current page can be
  // reinstalled over the top of the existing installation.
  virtual bool ShouldAllowWebAppReplacementInstall();

  // Callback invoked by the InstallableManager once it has fetched the page's
  // manifest.
  virtual void OnDidGetManifest(const InstallableData& result);

  // Returns an InstallableParams object that requests all checks necessary for
  // a web app banner.
  virtual InstallableParams ParamsToPerformInstallableWebAppCheck();

  // Run at the conclusion of OnDidGetManifest. For web app banners, this calls
  // back to the InstallableManager to continue checking criteria. For native
  // app banners, this checks whether native apps are preferred in the manifest,
  // and calls to Java to verify native app details. If a native banner isn't or
  // can't be requested, it continues with the web app banner checks.
  virtual void PerformInstallableChecks();

  virtual void PerformInstallableWebAppCheck();

  // Callback invoked by the InstallableManager once it has finished checking
  // all other installable properties.
  virtual void OnDidPerformInstallableWebAppCheck(
      const InstallableData& result);

  // Records that a banner was shown.
  void RecordDidShowBanner();

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

  // Shows the ambient badge if the current page advertises a native app or is
  // a web app. By default this shows nothing, but platform-specific code might
  // override this to show UI (e.g. on Android).
  virtual void MaybeShowAmbientBadge();

  // Updates the current state to |state|. Virtual to allow overriding in tests.
  virtual void UpdateState(State state);

  // content::WebContentsObserver overrides.
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const content::MediaPlayerId& id,
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

  // Whether or not the primary icon is maskable.
  bool has_maskable_primary_icon_;

  // The current banner pipeline state for this page load.
  State state_;

 private:
  friend class AppBannerManagerTest;

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
      mojo::Remote<blink::mojom::AppBannerController> controller,
      blink::mojom::AppBannerPromptReply reply);

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

  void SetInstallableWebAppCheckResult(InstallableWebAppCheckResult result);

  // Fetches the data required to display a banner for the current page.
  InstallableManager* manager_;

  // We do not want to trigger a banner when the manager is attached to
  // a WebContents that is playing video. Banners triggering on a site in the
  // background will appear when the tab is reactivated.
  std::vector<content::MediaPlayerId> active_media_players_;

  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;

  // If a banner is requested before the page has finished loading, defer
  // triggering the pipeline until the load is complete.
  bool has_sufficient_engagement_;
  bool load_finished_;

  std::unique_ptr<StatusReporter> status_reporter_;
  bool install_animation_pending_;
  InstallableWebAppCheckResult installable_web_app_check_result_;

  // The scope of the most recent installability check that passes promotability
  // requirements, otherwise invalid.
  GURL last_promotable_web_app_scope_;

  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManager);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
