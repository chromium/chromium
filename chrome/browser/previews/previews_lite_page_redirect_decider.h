// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_DECIDER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_DECIDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "net/http/http_request_headers.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
class WebContents;

}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class manages the triggering logic for Lite Page Redirect previews.
class PreviewsLitePageRedirectDecider
    : public AvailabilityProber::Delegate,
      public PreviewsHTTPSNotificationInfoBarDecider,
      public data_reduction_proxy::DataReductionProxySettingsObserver {
 public:
  explicit PreviewsLitePageRedirectDecider(
      content::BrowserContext* browser_context);
  virtual ~PreviewsLitePageRedirectDecider();

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Helpers to generate page ID.
  static uint64_t GeneratePageIdForWebContents(
      content::WebContents* web_contents);
  static uint64_t GeneratePageIdForProfile(Profile* profile);

  // Removes |this| as a DataReductionProxySettingsObserver.
  void Shutdown();

  // Sets the internal clock for testing.
  void SetClockForTesting(const base::TickClock* clock);

  // Sets |drp_settings_| for testing and registers |this| as an observer.
  void SetDRPSettingsForTesting(
      data_reduction_proxy::DataReductionProxySettings* drp_settings);

  // Clears the host blacklist. Used when user deletes their browsing history.
  void ClearBlacklist();

  // Clears all single bypasses and the host blacklist for testing.
  void ClearStateForTesting();

  // Sets that the user has seen the UI notification.
  void SetUserHasSeenUINotification();

  // PreviewsHTTPSNotificationInfoBarDecider:
  bool NeedsToNotifyUser() override;
  void NotifyUser(content::WebContents* web_contents) override;

  // Used to notify that the Previews Server should not be sent anymore requests
  // until after the given duration.
  void SetServerUnavailableFor(base::TimeDelta retry_after);

  // Returns true if a Preview should not be triggered because the server is
  // unavailable.
  bool IsServerUnavailable();

  // Informs that the given URL should be bypassed one time.
  void AddSingleBypass(std::string url);

  // Queries if the given URL should be bypassed one time, returning true if
  // yes.
  bool CheckSingleBypass(std::string url);

  // Generates a new page id for a request to the previews server.
  uint64_t GeneratePageID();

  // Reports data savings to Data Saver. Only the difference in |original_bytes|
  // and |network_bytes| will be updated in the data saver calls.
  void ReportDataSavings(int64_t network_bytes,
                         int64_t original_bytes,
                         const std::string& host);

  // Blacklists the given |host| for the given |duration| in the server
  // bypass blacklist for LitePageRedirects.
  void BlacklistBypassedHost(const std::string& host, base::TimeDelta duration);

  // Returns true if the given |host| is blacklisted in the server bypass
  // blacklist.
  bool HostBlacklistedFromBypass(const std::string& host);

  // Returns true if the Preview server is reachable on the network according to
  // a network probe. This will return the result of the most recent probe,
  // either from this session or a previous cached session's.
  bool IsServerReachableByProbe();

  // Returns true if a Preview server probe has completed for the current
  // network ID. This is session-agnostic because cached values from previous
  // sessions will be used if they exist for the current network ID.
  bool IsServerProbeResultAvailable();

  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnProxyRequestHeadersChanged(
      const net::HttpRequestHeaders& headers) override;
  void OnSettingsInitialized() override;

  bool has_drp_headers() const { return drp_headers_valid_; }

  AvailabilityProber* litepages_service_prober() {
    return litepages_service_prober_.get();
  }

 private:
  // AvailabilityProber::Delegate:
  bool ShouldSendNextProbe() override;
  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override;

  // The time after which it is ok to send the server more preview requests.
  base::Optional<base::TimeTicks> retry_at_;

  // A map that tracks the time at which a URL will stop being bypassed.
  std::unordered_map<std::string, base::TimeTicks> single_bypass_;

  // The clock used for getting the current time ticks. Use |SetClockForTesting|
  // in tests.
  const base::TickClock* clock_;

  // The page id to send on requests to the previews server. This is reset to a
  // random value on instantiation and every time the server headers change.
  uint64_t page_id_;

  // A reference to the DRP Settings so that |this| can be removed as an
  // observer on |Shutdown|. Not owned.
  data_reduction_proxy::DataReductionProxySettings* drp_settings_;

  // A reference to the profile's |PrefService|.
  PrefService* pref_service_;

  // Whether the notification infobar needs to be shown to the user in order to
  // use this preview.
  bool need_to_show_notification_;

  // A dictionary of host string to base::Time. If a hostname is a member of
  // this dictionary, that host should be blacklisted from this preview until
  // after the time value. This is stored persistently in prefs.
  std::unique_ptr<base::DictionaryValue> host_bypass_blacklist_;

  // A bool that tracks if the last call to |OnProxyRequestHeadersChanged| had
  // what looked like a valid chrome-proxy header.
  bool drp_headers_valid_;

  content::BrowserContext* browser_context_;

  // Probes the litepages service to establish that it is reachable before
  // attempting a preview.
  std::unique_ptr<AvailabilityProber> litepages_service_prober_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectDecider);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_DECIDER_H_
