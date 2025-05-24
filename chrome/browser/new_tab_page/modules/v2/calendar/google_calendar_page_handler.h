// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_GOOGLE_CALENDAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_GOOGLE_CALENDAR_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "google_apis/calendar/calendar_api_url_generator.h"
#include "google_apis/common/api_error_codes.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace google_apis {
class RequestSender;

namespace calendar {
class EventList;
}  // namespace calendar
}  // namespace google_apis

class PrefRegistrySimple;
class PrefService;
class Profile;

class GoogleCalendarPageHandler
    : public ntp::calendar::mojom::GoogleCalendarPageHandler {
 public:
  GoogleCalendarPageHandler(
      mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
          handler,
      Profile* profile,
      std::unique_ptr<google_apis::RequestSender> sender);
  GoogleCalendarPageHandler(
      mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
          handler,
      Profile* profile);
  ~GoogleCalendarPageHandler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ntp::calendar::mojom::GoogleCalendarPageHandler:
  void GetEvents(GetEventsCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  void OnRequestComplete(
      GetEventsCallback callback,
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events);

  mojo::Receiver<ntp::calendar::mojom::GoogleCalendarPageHandler> handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  google_apis::calendar::CalendarApiUrlGenerator url_generator_;

  base::WeakPtrFactory<GoogleCalendarPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_GOOGLE_CALENDAR_PAGE_HANDLER_H_
