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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

const char kBaseIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/";

const char kBaseAttachmentResourceUrl[] =
    "https://outlook.live.com/mail/0/deeplink/attachment/";

// TODO(357700028): Construct URL with modifiable date.
const char kRequestUrl[] =
    "https://graph.microsoft.com/v1.0/me/calendar/"
    "calendarview?startdatetime=2024-12-02T00:10:06.424Z&enddatetime=2024-12-"
    "06T00:10:06.424Z&select=id,hasAttachments,subject,start,attendees,"
    "webLink,onlineMeeting,location,isOrganizer,responseStatus,end,isCancelled&"
    "expand=attachments(select=id,name,contentType)";

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("outlook_calendar_page_handler", R"(
        semantics {
          sender: "Outlook Calendar Page Handler"
          description:
            "The Outlook Calendar Page Handler requests user's "
            "Outlook calendar events from the Microsoft Graph API. "
            "The response will be used to display calendar events "
            "on the desktop NTP."
          trigger:
            "Each time a signed-in user navigates to the NTP while "
            "the Outlook Calendar module is enabled and the user's "
            "Outlook account has been authenticated on the NTP."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "OAuth2 access token identifying the Outlook account."
          destination: OTHER
          destination_other: "Microsoft Graph API"
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          last_reviewed: "2024-12-17"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by (1) selecting "
            "a non-Google default search engine in Chrome "
            "settings under 'Search Engine', (2) signing out, "
            "or (3) disabling the Outlook Calendar module."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
            NTPCardsVisible {
              NTPCardsVisible: false
            }
            NTPOutlookCardVisible {
              NTPOutlookCardVisible: false
            }
          }
        })");

constexpr int kMaxResponseSize = 1024 * 1024;

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
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      url_loader_factory_(profile->GetURLLoaderFactory()) {}

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
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(kRequestUrl);

  // TODO(357700028): Pass in actual access token.
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer <accesstoken>");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                      "no-cache");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OutlookCalendarPageHandler::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSize);
}

void OutlookCalendarPageHandler::OnJsonReceived(
    GetEventsCallback callback,
    std::unique_ptr<std::string> response_body) {
  // TODO(376516070): Attempt to retrieve "Retry-After" header for throttling
  // errors.
  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error == net::OK && response_body) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response_body,
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
          attachment_dict.FindString("contentType");
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

