// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <string>
#include <vector>

#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"
#include "components/search/ntp_features.h"

OutlookCalendarPageHandler::OutlookCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        handler)
    : handler_(this, std::move(handler)) {}

OutlookCalendarPageHandler::~OutlookCalendarPageHandler() = default;

void OutlookCalendarPageHandler::GetEvents(GetEventsCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpOutlookCalendarModule,
      ntp_features::kNtpOutlookCalendarModuleDataParam);
  if (!fake_data_param.empty()) {
    std::move(callback).Run(calendar::calendar_fake_data_helper::GetFakeEvents(
        calendar::calendar_fake_data_helper::CalendarType::OUTLOOK_CALENDAR));
  } else {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}
