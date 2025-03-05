// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/modules/microsoft_modules_helper.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_fake_data_helper.h"
#include "chrome/common/pref_names.h"
#include "components/search/ntp_features.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kBaseAttachmentResourceUrl[] =
    "https://outlook.office.com/mail/deeplink/attachment/";

const char kRequestUrl[] =
    "https://graph.microsoft.com/v1.0/me/calendar/"
    "calendarview?startdatetime=%s&enddatetime=%s&select=id,hasAttachments,"
    "subject,start,attendees,webLink,onlineMeeting,location,isOrganizer,"
    "responseStatus,end,isCancelled&expand=attachments(select=id,name,"
    "contentType)";

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

constexpr net::NetworkTrafficAnnotationTag attachment_traffic_annotation =
    net::DefineNetworkTrafficAnnotation("outlook_calendar_event_attachment", R"(
        semantics {
          sender: "Outlook Calendar Page Handler"
          description:
            "The Outlook Calendar Page Handler requests user's "
            "calendar event attachment page to ensure it exists. "
            "The response will be used to display calendar events "
            "on the desktop NTP."
          trigger:
            "Every 4 hours a signed-in user navigates to the NTP while "
            "the Outlook Calendar module is enabled and the user's "
            "Outlook account has been authenticated on the NTP."
          user_data {
            type: OTHER,
            type: SENSITIVE_URL
          }
          data: "The URL consists of a users's Outlook calendar event "
                "ID and the event's attachment's ID. This URL is a "
                "page the user may have previously visited before or "
                "may visit."
          destination: OTHER
          destination_other: "Microsoft Server"
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          last_reviewed: "2025-01-06"
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

// Returns the URL for retrieving calendar events.
GURL GetRequestUrl() {
  std::string start_date_time = TimeFormatAsIso8601(base::Time::Now());
  const base::TimeDelta time_window =
      ntp_features::kNtpOutlookCalendarModuleRetrievalWindowParam.Get();
  std::string end_date_time =
      TimeFormatAsIso8601(base::Time::Now() + time_window);
  std::string request_url =
      base::StringPrintf(kRequestUrl, start_date_time, end_date_time);
  return GURL(request_url);
}

// Emits the total number of events found in the response. Note: The Microsoft
// Graph API by default returns a max of 100 events.
void RecordResponseValueCount(int count) {
  base::UmaHistogramCounts100("NewTabPage.OutlookCalendar.ResponseResult",
                              count);
}

// Emits the result of the request for events.
void RecordCalendarRequestResult(OutlookCalendarRequestResult result) {
  base::UmaHistogramEnumeration("NewTabPage.OutlookCalendar.RequestResult",
                                result);
}

// Emits the time in seconds that should be waited before attempting another
// request.
void RecordThrottlingWaitTime(base::TimeDelta seconds) {
  base::UmaHistogramTimes("NewTabPage.OutlookCalendar.ThrottlingWaitTime",
                          seconds);
}

}  // namespace

// static
void OutlookCalendarPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kNtpOutlookCalendarRetryAfterTime,
                             base::Time());
  registry->RegisterTimePref(prefs::kNtpOutlookCalendarLastDismissedTime,
                             base::Time());
  registry->RegisterTimePref(
      prefs::kNtpOutlookCalendarLastAttachmentRequestTime, base::Time());
  registry->RegisterBooleanPref(
      prefs::kNtpOutlookCalendarLastAttachmentRequestSuccess, false);
}

OutlookCalendarPageHandler::OutlookCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      microsoft_auth_service_(
          MicrosoftAuthServiceFactory::GetForProfile(profile)),
      pref_service_(profile->GetPrefs()),
      url_loader_factory_(profile->GetURLLoaderFactory()) {}

OutlookCalendarPageHandler::~OutlookCalendarPageHandler() = default;

void OutlookCalendarPageHandler::GetEvents(GetEventsCallback callback) {
  // Do not get events if the module was dismissed.
  base::Time last_dismissed_time =
      pref_service_->GetTime(prefs::kNtpOutlookCalendarLastDismissedTime);
  if (last_dismissed_time != base::Time() &&
      base::Time::Now() - last_dismissed_time < base::Hours(12)) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpOutlookCalendarModule,
      ntp_features::kNtpOutlookCalendarModuleDataParam);
  if (!fake_data_param.empty()) {
    bool has_attachments_enabled = fake_data_param == "fake-attachments";
    std::move(callback).Run(calendar::calendar_fake_data_helper::GetFakeEvents(
        calendar::calendar_fake_data_helper::CalendarType::OUTLOOK_CALENDAR,
        has_attachments_enabled));
  } else {
    MakeRequest(std::move(callback));
  }
}

void OutlookCalendarPageHandler::DismissModule() {
  pref_service_->SetTime(prefs::kNtpOutlookCalendarLastDismissedTime,
                         base::Time::Now());
}

void OutlookCalendarPageHandler::RestoreModule() {
  pref_service_->SetTime(prefs::kNtpOutlookCalendarLastDismissedTime,
                         base::Time());
}

void OutlookCalendarPageHandler::MakeRequest(GetEventsCallback callback) {
  // Do not attempt to get calendar events when a throttling error must be
  // waited out.
  base::Time retry_after_time =
      pref_service_->GetTime(prefs::kNtpOutlookCalendarRetryAfterTime);
  if (retry_after_time != base::Time() &&
      base::Time::Now() < retry_after_time) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GetRequestUrl();
  const std::string access_token = microsoft_auth_service_->GetAccessToken();
  const std::string auth_header_value = "Bearer " + access_token;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      auth_header_value);
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
  const int net_error = url_loader_->NetError();
  OutlookCalendarRequestResult request_result =
      OutlookCalendarRequestResult::kNetworkError;

  // Check for unauthorized and throttling errors.
  auto* response_info = url_loader_->ResponseInfo();
  if (net_error != net::OK && response_info && response_info->headers) {
    int64_t wait_time =
        response_info->headers->GetInt64HeaderValue("Retry-After");
    if (wait_time != -1) {
      request_result = OutlookCalendarRequestResult::kThrottlingError;
      RecordThrottlingWaitTime(base::Seconds(wait_time));
      pref_service_->SetTime(prefs::kNtpOutlookCalendarRetryAfterTime,
                             base::Time::Now() + base::Seconds(wait_time));
    } else if (response_info->headers->response_code() ==
               net::HTTP_UNAUTHORIZED) {
      request_result = OutlookCalendarRequestResult::kAuthError;
      microsoft_auth_service_->SetAuthStateError();
    }
  }

  url_loader_.reset();

  if (net_error == net::OK && response_body) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response_body,
        base::BindOnce(&OutlookCalendarPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    RecordCalendarRequestResult(request_result);
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
  }
}

void OutlookCalendarPageHandler::OnJsonParsed(
    GetEventsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    RecordCalendarRequestResult(OutlookCalendarRequestResult::kJsonParseError);
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }
  auto* events = result->GetDict().FindList("value");
  if (!events) {
    RecordCalendarRequestResult(OutlookCalendarRequestResult::kContentError);
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }
  RecordResponseValueCount(events->size());

  std::vector<ntp::calendar::mojom::CalendarEventPtr> created_events;
  const size_t max_events =
      ntp_features::kNtpOutlookCalendarModuleMaxEventsParam.Get();
  // Keeps track of the last attachment's `resource_url`. Needed in the case
  // that the URL needs to be validated.
  std::string last_attachment_resource_url;
  for (const auto& event : *events) {
    if (created_events.size() == max_events) {
      break;
    }
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
      RecordCalendarRequestResult(OutlookCalendarRequestResult::kContentError);
      std::move(callback).Run(
          std::vector<ntp::calendar::mojom::CalendarEventPtr>());
      return;
    }

    // Do not create event when the user has declined.
    if (*response_status == "declined") {
      continue;
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
          RecordCalendarRequestResult(
              OutlookCalendarRequestResult::kContentError);
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
        RecordCalendarRequestResult(
            OutlookCalendarRequestResult::kContentError);
        std::move(callback).Run(
            std::vector<ntp::calendar::mojom::CalendarEventPtr>());
        return;
      }

      std::string file_extension =
          microsoft_modules_helper::GetFileExtension(*content_type);
      // Skip creating an attachment if an extension cannot be found. This is
      // being done because the `title` and `icon_url` are dependent on a
      // correct extension.
      if (file_extension.empty()) {
        continue;
      }

      created_attachment->title =
          microsoft_modules_helper::GetFileName(*name, file_extension);
      GURL icon_url = microsoft_modules_helper::GetFileIconUrl(*content_type);
      if (!icon_url.is_valid()) {
        continue;
      }
      created_attachment->icon_url = icon_url;

      std::string event_id_escaped =
          base::EscapeUrlEncodedData(*event_id, true);
      std::string attachment_id_escaped = base::EscapeUrlEncodedData(*id, true);
      std::string url_path = event_id_escaped + "/" + attachment_id_escaped;
      GURL attachment_url = GURL(kBaseAttachmentResourceUrl).Resolve(url_path);

      // Set `resource_url` prematurely because the request to check whether the
      // attachment page exists is handled asynchronously. This way the request
      // can finish before possibly incorrectly resetting the URLs. The urls
      // will be reset if a) the next request fails b) the last attempt was
      // unsuccessful and it is not yet time to make another request.
      if (!ntp_features::kNtpOutlookCalendarModuleDisableAttachmentsParam
               .Get()) {
        created_attachment->resource_url = attachment_url;
      }
      last_attachment_resource_url = attachment_url.spec();
      created_event->attachments.push_back(std::move(created_attachment));
    }

    created_event->location = *location;
    if (conference_url) {
      created_event->conference_url = GURL(*conference_url);
    }
    created_events.push_back(std::move(created_event));
  }
  RecordCalendarRequestResult(OutlookCalendarRequestResult::kSuccess);

  // Determine whether attachment's `resource_url` should be validated.
  base::Time last_request_time = pref_service_->GetTime(
      prefs::kNtpOutlookCalendarLastAttachmentRequestTime);
  bool should_check_attachments =
      ntp_features::kNtpOutlookCalendarModuleAttachmentCheckParam.Get();
  bool should_make_request =
      last_request_time == base::Time() && should_check_attachments
          ? !last_attachment_resource_url.empty()
          : base::Time::Now() - last_request_time > base::Hours(4) &&
                !last_attachment_resource_url.empty();

  if (should_make_request) {
    MakeAttachmentUrlRequest(std::move(callback), std::move(created_events),
                             last_attachment_resource_url);
    return;
  } else if (!pref_service_->GetBoolean(
                 prefs::kNtpOutlookCalendarLastAttachmentRequestSuccess) &&
             should_check_attachments) {
    // Reset attachment URLs if the last request was unsuccessful.
    for (auto& event : created_events) {
      for (auto& attachment : event->attachments) {
        attachment->resource_url.reset();
      }
    }
  }
  std::move(callback).Run(std::move(created_events));
}

void OutlookCalendarPageHandler::MakeAttachmentUrlRequest(
    GetEventsCallback callback,
    std::vector<::ntp::calendar::mojom::CalendarEventPtr> events,
    std::string resource_url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(resource_url);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                      "no-cache");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 attachment_traffic_annotation);
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&OutlookCalendarPageHandler::OnHeaderReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(events)));
}

void OutlookCalendarPageHandler::OnHeaderReceived(
    GetEventsCallback callback,
    std::vector<::ntp::calendar::mojom::CalendarEventPtr> events,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  const int net_error = headers->response_code();
  pref_service_->SetTime(prefs::kNtpOutlookCalendarLastAttachmentRequestTime,
                         base::Time::Now());
  pref_service_->SetBoolean(
      prefs::kNtpOutlookCalendarLastAttachmentRequestSuccess,
      net_error == net::HTTP_OK);

  url_loader_.reset();

  // Reset all attachment resource URLs when there's an error verifying that at
  // least one page does not exist.
  if (net_error != net::HTTP_OK) {
    for (auto& event : events) {
      for (auto& attachment : event->attachments) {
        attachment->resource_url.reset();
      }
    }
  }
  std::move(callback).Run(std::move(events));
}
