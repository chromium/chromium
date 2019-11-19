// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/installable/installable_logging.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class Profile;

// Utility class to record banner events for the given package or start url.
//
// These events are used to decide when banners should be shown, using a
// heuristic based on how many different days in a recent period of time (for
// example the past two weeks) the banner could have been shown, when it was
// last shown, when it was last blocked, and when it was last installed (for
// ServiceWorker style apps - native apps can query whether the app was
// installed directly).
//
// The desired effect is to have banners appear once a user has demonstrated
// an ongoing relationship with the app, and not to pester the user too much.
//
// For most events only the last event is recorded. The exception are the
// could show events. For these a list of the events is maintained. At most
// one event is stored per day, and events outside the window the heuristic
// uses are discarded. Local times are used to enforce these rules, to ensure
// what we count as a day matches what the user perceives to be days.
class AppBannerSettingsHelper {
 public:
  // An enum for determining the title to use for the add to homescreen / app
  // banner functionality.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.banners
  enum LanguageOption {
    LANGUAGE_OPTION_DEFAULT = 0,
    LANGUAGE_OPTION_MIN = LANGUAGE_OPTION_DEFAULT,
    LANGUAGE_OPTION_ADD = 1,
    LANGUAGE_OPTION_INSTALL = 2,
    LANGUAGE_OPTION_MAX = LANGUAGE_OPTION_INSTALL,
  };

  // The various types of banner events recorded as timestamps in the app banner
  // content setting per origin and package_name_or_start_url pair. This enum
  // corresponds to the kBannerEventsKeys array.
  // TODO(mariakhomenko): Rename events to reflect that they are used in more
  // contexts now.
  enum AppBannerEvent {
    // Records the first time that a site met the conditions to show a banner.
    // Used for computing the MinutesFromFirstVisitToBannerShown metric.
    APP_BANNER_EVENT_COULD_SHOW,
    // Records the latest time a banner was shown to the user. Used to suppress
    // the banner from being shown too often.
    APP_BANNER_EVENT_DID_SHOW,
    // Records the latest time a banner was dismissed by the user. Used to
    // suppress the banner for some time if the user explicitly didn't want it.
    APP_BANNER_EVENT_DID_BLOCK,
    // Records the latest time the user added a site to the homescreen from a
    // banner, or launched that site from homescreen. Used to ensure banners are
    // not shown for sites which were added, and to determine if sites were
    // launched recently.
    APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
    APP_BANNER_EVENT_NUM_EVENTS,
  };

  static const char kInstantAppsKey[];

  // The content setting basically records a simplified subset of history.
  // For privacy reasons this needs to be cleared. The ClearHistoryForURLs
  // function removes any information from the banner content settings for the
  // given URls.
  static void ClearHistoryForURLs(Profile* profile,
                                  const std::set<GURL>& origin_urls);

  // Record a banner installation event, for either a WEB or NATIVE app.
  static void RecordBannerInstallEvent(
      content::WebContents* web_contents,
      const std::string& package_name_or_start_url);

  // Record a banner dismissal event, for either a WEB or NATIVE app.
  static void RecordBannerDismissEvent(
      content::WebContents* web_contents,
      const std::string& package_name_or_start_url);

  // Record a banner event specified by |event|.
  static void RecordBannerEvent(content::WebContents* web_contents,
                                const GURL& origin_url,
                                const std::string& package_name_or_start_url,
                                AppBannerEvent event,
                                base::Time time);

  // Reports whether the app install banner was blocked by the user recently
  // enough with respect to |now| that another banner should not yet be shown.
  // |package_name_or_start_url| must be non-empty.
  static bool WasBannerRecentlyBlocked(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      base::Time now);

  // Reports whether the app install banner was ignored by the user recently
  // enough with respect to |now| that another banner should not yet be shown.
  // |package_name_or_start_url| must be non-empty.
  static bool WasBannerRecentlyIgnored(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      base::Time now);

  // Returns whether the supplied app has ever been installed from |origin_url|.
  static bool HasBeenInstalled(content::WebContents* web_contents,
                               const GURL& origin_url,
                               const std::string& package_name_or_start_url);

  // Get the time that |event| was recorded, or a null time if it has not yet
  // been recorded. Exposed for testing.
  static base::Time GetSingleBannerEvent(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      AppBannerEvent event);

  // Returns true if |total_engagement| is sufficiently high to warrant
  // triggering a banner, or if the command-line flag to bypass engagement
  // checking is true.
  static bool HasSufficientEngagement(double total_engagement);

  // Record a UMA statistic measuring the minutes between the first visit to the
  // site and the first showing of the banner.
  static void RecordMinutesFromFirstVisitToShow(
      content::WebContents* web_contents,
      const GURL& origin_url,
      const std::string& package_name_or_start_url,
      base::Time time);

  // Returns true if any site under |origin| was launched from homescreen in the
  // last ten days. This allows services outside app banners to utilise the
  // content setting that ensures app banners are not shown for sites which ave
  // already been added to homescreen.
  static bool WasLaunchedRecently(Profile* profile,
                                  const GURL& origin_url,
                                  base::Time now);

  // Set the number of days which dismissing/ignoring the banner should prevent
  // a banner from showing.
  static void SetDaysAfterDismissAndIgnoreToTrigger(unsigned int dismiss_days,
                                                    unsigned int ignore_days);

  // Set the total engagement weight required to trigger a banner.
  static void SetTotalEngagementToTrigger(double total_engagement);

  // Resets the engagement weights, minimum minutes, and total engagement to
  // trigger to their default values.
  static void SetDefaultParameters();

  // Updates all values from field trial.
  static void UpdateFromFieldTrial();

  // Queries variations to determine which language option should be used for
  // app banners and add to homescreen.
  static LanguageOption GetHomescreenLanguageOption();

  // Returns whether we are out of |scope|'s animation suppression period and
  // can show an animation.
  static bool CanShowInstallTextAnimation(content::WebContents* web_contents,
                                          const GURL& scope);

  // Records the fact that we've shown an animation for |scope| and updates its
  // animation suppression period.
  static void RecordInstallTextAnimationShown(
      content::WebContents* web_contents,
      const GURL& scope);

  // Utility class for testing, which sets how long the banner should be
  // suppressed after it is dismissed or ignored. The previous configuration
  // is restored when this object is destructed.
  class ScopedTriggerSettings {
   public:
    ScopedTriggerSettings(unsigned int dismiss_days, unsigned int ignore_days);
    virtual ~ScopedTriggerSettings();

   private:
    unsigned int old_dismiss_, old_ignore_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedTriggerSettings);
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AppBannerSettingsHelper);
};

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_SETTINGS_HELPER_H_
