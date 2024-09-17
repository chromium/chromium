// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class OutlookCalendarPageHandler
    : public ntp::calendar::mojom::OutlookCalendarPageHandler {
 public:
  explicit OutlookCalendarPageHandler(
      mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
          handler);
  ~OutlookCalendarPageHandler() override;

  // ntp::calendar::mojom::OutlookCalendarPageHandler
  void GetEvents(GetEventsCallback callback) override;

 private:
  mojo::Receiver<ntp::calendar::mojom::OutlookCalendarPageHandler> handler_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_OUTLOOK_CALENDAR_PAGE_HANDLER_H_
