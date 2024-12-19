// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_

#include <string>

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class Profile;

class OutlookCalendarPageHandler
    : public ntp::calendar::mojom::OutlookCalendarPageHandler {
 public:
  explicit OutlookCalendarPageHandler(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          handler,
      Profile* profile);
  ~OutlookCalendarPageHandler() override;

  // ntp::calendar::mojom::OutlookCalendarPageHandler
  void GetEvents(GetEventsCallback callback) override;

 private:
  void MakeRequest(GetEventsCallback callback);
  void OnJsonReceived(GetEventsCallback callback,
                      std::unique_ptr<std::string> response_body);
  void OnJsonParsed(GetEventsCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);
  mojo::Receiver<ntp::calendar::mojom::OutlookCalendarPageHandler> handler_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<OutlookCalendarPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
