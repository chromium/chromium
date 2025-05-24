// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class MicrosoftAuthService;
class PrefRegistrySimple;
class PrefService;
class Profile;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OutlookCalendarRequestResult {
  // Success parsing necessary event data from the response.
  kSuccess = 0,
  kNetworkError = 1,
  kJsonParseError = 2,
  // Error retrieving all the expected event data from the response.
  kContentError = 3,
  kThrottlingError = 4,
  kAuthError = 5,
  kMaxValue = kAuthError,
};

class OutlookCalendarPageHandler
    : public ntp::calendar::mojom::OutlookCalendarPageHandler {
 public:
  explicit OutlookCalendarPageHandler(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          handler,
      Profile* profile);
  ~OutlookCalendarPageHandler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ntp::calendar::mojom::OutlookCalendarPageHandler
  void GetEvents(GetEventsCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  void MakeRequest(GetEventsCallback callback);
  void OnJsonReceived(GetEventsCallback callback,
                      std::unique_ptr<std::string> response_body);
  void OnJsonParsed(GetEventsCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);
  void MakeAttachmentUrlRequest(
      GetEventsCallback callback,
      std::vector<::ntp::calendar::mojom::CalendarEventPtr> events,
      std::string resource_url);
  void OnHeaderReceived(
      GetEventsCallback callback,
      std::vector<::ntp::calendar::mojom::CalendarEventPtr> events,
      scoped_refptr<net::HttpResponseHeaders> headers);

  mojo::Receiver<ntp::calendar::mojom::OutlookCalendarPageHandler> handler_;
  raw_ptr<MicrosoftAuthService> microsoft_auth_service_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<OutlookCalendarPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
