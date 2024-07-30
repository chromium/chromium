// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CALENDAR_CALENDAR_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_CALENDAR_CALENDAR_CLIENT_IMPL_H_

#include <string>

#include "ash/calendar/calendar_client.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_requests.h"

class Profile;

namespace ash {

// Implementation of the calendar browser client, one per multiprofile user.
class CalendarClientImpl : public CalendarClient {
 public:
  explicit CalendarClientImpl(Profile* profile);
  CalendarClientImpl(const CalendarClientImpl& other) = delete;
  CalendarClientImpl& operator=(const CalendarClientImpl& other) = delete;
  ~CalendarClientImpl() override;

  // CalendarClient:
  bool IsDisabledByAdmin() const override;
  base::OnceClosure GetCalendarList(
      google_apis::calendar::CalendarListCallback callback) override;
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time) override;
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time start_time,
      const base::Time end_time,
      const std::string& calendar_id,
      const std::string& calendar_color_id) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CALENDAR_CALENDAR_CLIENT_IMPL_H_
