// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_observer.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_observer.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/url_data_source.h"

#if defined(OS_ANDROID)
#error "Instant is only used on desktop";
#endif

struct OneGoogleBarData;
struct PromoData;
class Profile;

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

// Serves HTML and resources for the local New Tab page, i.e.
// chrome-search://local-ntp/local-ntp.html.
// WARNING: Due to the threading model of URLDataSource, some methods of this
// class are called on the UI thread, others on the IO thread. All data members
// live on the UI thread, so make sure not to access them from the IO thread!
// To prevent accidental access, all methods that get called on the IO thread
// are implemented as non-member functions.
class LocalNtpSource : public content::URLDataSource,
                       public NtpBackgroundServiceObserver,
                       public OneGoogleBarServiceObserver,
                       public PromoServiceObserver,
                       public SearchSuggestServiceObserver {
 public:
  explicit LocalNtpSource(Profile* profile);
  ~LocalNtpSource() override;

 private:
  class SearchConfigurationProvider;
  class DesktopLogoObserver;

  struct NtpBackgroundRequest {
    NtpBackgroundRequest(base::TimeTicks start_time,
                         content::URLDataSource::GotDataCallback callback);
    NtpBackgroundRequest(NtpBackgroundRequest&&);
    NtpBackgroundRequest& operator=(NtpBackgroundRequest&&);
    ~NtpBackgroundRequest();

    base::TimeTicks start_time;
    content::URLDataSource::GotDataCallback callback;
  };

  // Overridden from content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  bool ShouldAddContentSecurityPolicy() override;

  // The Content Security Policy for the Local NTP.
  std::string GetContentSecurityPolicyForNTP();

  // Overridden from NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override {}
  void OnNtpBackgroundServiceShuttingDown() override;

  // Overridden from OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  // Overridden from PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  // Overridden from SearchSuggestServiceObserver:
  void OnSearchSuggestDataUpdated() override;
  void OnSearchSuggestServiceShuttingDown() override;

  // Called when the OGB data is available and serves |data| to any pending
  // request from the NTP.
  void ServeOneGoogleBar(const base::Optional<OneGoogleBarData>& data);
  // Called when the page requests OGB data. If the data is available it
  // is returned immediately, otherwise it is returned when it becomes available
  // in ServeOneGoogleBar.
  void ServeOneGoogleBarWhenAvailable(
      content::URLDataSource::GotDataCallback callback);

  // Called when the promo data is available and serves |data| to any pending
  // requests from the NTP.
  void ServePromo(const base::Optional<PromoData>& data);
  // Called when the page requests promo data. If the data is available it
  // is returned immediately, otherwise it is returned when it becomes
  // available in ServePromo.
  void ServePromoWhenAvailable(
      content::URLDataSource::GotDataCallback callback);

  // If suggestion data is available return it immediately, otherwise no search
  // suggestions will be shown on this NTP load.
  void ServeSearchSuggestionsIfAvailable(
      content::URLDataSource::GotDataCallback callback);

  // Start requests for the promo and OGB.
  void InitiatePromoAndOGBRequests();

  Profile* const profile_;

  std::vector<NtpBackgroundRequest> ntp_background_collections_requests_;
  std::vector<NtpBackgroundRequest> ntp_background_image_info_requests_;

  NtpBackgroundService* ntp_background_service_;

  ScopedObserver<NtpBackgroundService, NtpBackgroundServiceObserver>
      ntp_background_service_observer_{this};

  base::Optional<base::TimeTicks> pending_one_google_bar_request_;
  std::vector<content::URLDataSource::GotDataCallback>
      one_google_bar_callbacks_;

  OneGoogleBarService* one_google_bar_service_;

  ScopedObserver<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observer_{this};

  base::Optional<base::TimeTicks> pending_promo_request_;
  std::vector<content::URLDataSource::GotDataCallback> promo_callbacks_;

  PromoService* promo_service_;

  ScopedObserver<PromoService, PromoServiceObserver> promo_service_observer_{
      this};

  base::Optional<base::TimeTicks> pending_search_suggest_request_;

  SearchSuggestService* search_suggest_service_;

  ScopedObserver<SearchSuggestService, SearchSuggestServiceObserver>
      search_suggest_service_observer_{this};

  search_provider_logos::LogoService* logo_service_;
  std::unique_ptr<DesktopLogoObserver> logo_observer_;

  std::unique_ptr<SearchConfigurationProvider> search_config_provider_;

  base::WeakPtrFactory<LocalNtpSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalNtpSource);
};

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_SOURCE_H_
