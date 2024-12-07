// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"
#include "components/search/ntp_features.h"
#include "net/base/mime_util.h"

namespace {

const char kBaseIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/";

const char kBaseAttachmentResourceUrl[] =
    "https://outlook.live.com/mail/0/deeplink/attachment/";

std::string GetFileExtension(std::string mime_type) {
  base::FilePath::StringType extension;
  net::GetPreferredExtensionForMimeType(mime_type, &extension);
  std::string result;

#if BUILDFLAG(IS_WIN)
  // `extension` will be of std::wstring type on Windows which needs to be
  // handled differently than std::string. See base/files/file_path.h for more
  // info.
  result = base::WideToUTF8(extension);
#else
  result = extension;
#endif

  return result;
}

GURL GetIconUrl(std::string extension) {
  return GURL(kBaseIconUrl + extension + ".png");
}

// The file names in the response are formatted as "name.extension" we
// only want the file name so we remove the extension.
std::string GetFileName(std::string full_name, std::string extension) {
  return full_name.substr(0, full_name.size() - extension.size() - 1);
}

}  // namespace

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
    MakeRequest(std::move(callback));
  }
}

void OutlookCalendarPageHandler::MakeRequest(GetEventsCallback callback) {
  // TODO(357700028): Replace fake JSON response with an actual HTTP
  // request/response.
  calendar::calendar_fake_data_helper::GetFakeJsonResponse(
      base::BindOnce(&OutlookCalendarPageHandler::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void OutlookCalendarPageHandler::OnJsonReceived(GetEventsCallback callback,
                                                std::string response_body) {
  if (!response_body.empty()) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        response_body,
        base::BindOnce(&OutlookCalendarPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}

void OutlookCalendarPageHandler::OnJsonParsed(
    GetEventsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }
  auto* events = result->GetDict().FindList("value");
  if (!events) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }

  std::vector<ntp::calendar::mojom::CalendarEventPtr> created_events;
  for (const auto& event : *events) {
    const auto& event_dict = event.GetDict();
    const std::string* event_id = event_dict.FindString("id");
    std::optional<bool> has_attachments = event_dict.FindBool("hasAttachments");
    const std::string* title = event_dict.FindString("subject");
    std::optional<bool> is_canceled = event_dict.FindBool("isCancelled");
    const std::string* calendar_url = event_dict.FindString("webLink");
    const std::string* response_status =
        event_dict.FindStringByDottedPath("responseStatus.response");
    const std::string* conference_url =
        event_dict.FindStringByDottedPath("onlineMeeting.joinUrl");
    const std::string* location =
        event_dict.FindStringByDottedPath("location.displayName");
    const std::string* start_time =
        event_dict.FindStringByDottedPath("start.dateTime");
    const std::string* end_time =
        event_dict.FindStringByDottedPath("end.dateTime");
    std::optional<bool> is_organizer = event_dict.FindBool("isOrganizer");
    const base::Value::List* attendees = event_dict.FindList("attendees");
    const base::Value::List* attachments = event_dict.FindList("attachments");

    base::Time start_timestamp;
    base::Time end_timestamp;

    // Do not send calendar event data if all required information is not
    // found in the response.
    if (!event_id || !has_attachments.has_value() || !title || !calendar_url ||
        !start_time || !end_time || !is_organizer.has_value() || !attendees ||
        !location || !response_status || !is_canceled.has_value() ||
        !base::Time::FromUTCString((*start_time).c_str(), &start_timestamp) ||
        !base::Time::FromUTCString((*end_time).c_str(), &end_timestamp)) {
      std::move(callback).Run(
          std::vector<ntp::calendar::mojom::CalendarEventPtr>());
      return;
    }
    ntp::calendar::mojom::CalendarEventPtr created_event =
        ntp::calendar::mojom::CalendarEvent::New();
    created_event->title = *title;
    created_event->start_time = start_timestamp;
    created_event->end_time = end_timestamp;
    created_event->url = GURL(*calendar_url);
    created_event->is_accepted =
        *response_status == "accepted" || is_organizer.value();

    // On Outlook calendar, if an event exists and the user is not the
    // organizer, there must be another user attending (the organizer by
    // default), unless the event is canceled, but not removed from calendar.
    // Note: If user is the organizer they are not found in the attendees list.
    if (is_organizer.value()) {
      for (const auto& attendee : *attendees) {
        const std::string* attendee_response =
            attendee.GetDict().FindStringByDottedPath("status.response");
        if (!attendee_response) {
          std::move(callback).Run(
              std::vector<ntp::calendar::mojom::CalendarEventPtr>());
          return;
        } else if (*attendee_response == "accepted") {
          created_event->has_other_attendee = true;
          break;
        }
      }
    } else {
      created_event->has_other_attendee = !is_canceled.value();
    }

    // Create attachments.
    for (const auto& attachment : *attachments) {
      ntp::calendar::mojom::AttachmentPtr created_attachment =
          ntp::calendar::mojom::Attachment::New();
      const auto& attachment_dict = attachment.GetDict();
      const std::string* id = attachment_dict.FindString("id");
      const std::string* name = attachment_dict.FindString("name");
      const std::string* content_type =
          attachment_dict.FindString("@odata.mediaContentType");
      if (!id || !name || !content_type) {
        std::move(callback).Run(
            std::vector<ntp::calendar::mojom::CalendarEventPtr>());
        return;
      }

      std::string file_extension = GetFileExtension(*content_type);
      // Skip creating an attachment if an extension cannot be found. This is
      // being done because the `title` and `icon_url` are dependent on a
      // correct extension.
      if (file_extension.empty()) {
        continue;
      }

      created_attachment->title = GetFileName(*name, file_extension);
      created_attachment->icon_url = GetIconUrl(file_extension);
      // TODO(376515087): Verify resource URL is valid by making a GET request.
      created_attachment->resource_url =
          GURL(kBaseAttachmentResourceUrl + *event_id + "/" + *id);
      created_event->attachments.push_back(std::move(created_attachment));
    }

    created_event->location = *location;
    if (conference_url) {
      created_event->conference_url = GURL(*conference_url);
    }
    created_events.push_back(std::move(created_event));
  }
  std::move(callback).Run(std::move(created_events));
}

// TODO(357700028): Delete once the end-to-end HTTP request implementation is
// done.
void OutlookCalendarPageHandler::GetEventsForTesting(
    GetEventsCallback callback,
    std::string response_body) {
  // Bypass "making a request" and trigger response received.
  OnJsonReceived(std::move(callback), response_body);
}
