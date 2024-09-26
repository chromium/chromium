// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace {

const char kGoogleCalendarLastDismissedTimePrefName[] =
    "NewTabPage.GoogleCalendar.LastDimissedTime";

const char kGoogleCalendarDriveIconUrl[] =
    "https://drive-thirdparty.googleusercontent.com/16/type/application/"
    "vnd.google-apps.document";

// TODO(crbug.com/343738665): Update when more granular policy is added.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("google_calendar_page_handler", R"(
       semantics {
         sender: "Google Calendar Page Handler"
         description:
            "The Google Calendar Page Handler requests the signed in user's "
            "calendar for the current day to display on the desktop NTP."
          trigger:
              "NTP is open with the Google Calendar card enabled by both the "
              "user and Enterprise policy."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "rtatum@google.com"
            }
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2024-05-16"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by (1) selecting "
            "a non-Google default search engine in Chrome "
            "settings under 'Search Engine', (2) signing out, "
            "or (3) disabling the Google Calendar module."
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
          }
        })");

ntp::calendar::mojom::CalendarEventPtr GetFakeEvent(int index) {
  ntp::calendar::mojom::CalendarEventPtr event =
      ntp::calendar::mojom::CalendarEvent::New();
  event->title = "Calendar Event " + base::NumberToString(index);
  event->start_time = base::Time::Now() + base::Minutes(index * 30);
  event->end_time = event->start_time + base::Minutes(30);
  event->url = GURL("https://foo.com/" + base::NumberToString(index));
  event->location = "Conference Room " + base::NumberToString(index);
  for (int i = 0; i < 3; ++i) {
    ntp::calendar::mojom::AttachmentPtr attachment =
        ntp::calendar::mojom::Attachment::New();
    attachment->title = "Attachment " + base::NumberToString(i);
    attachment->resource_url =
        GURL("https://foo.com/attachment" + base::NumberToString(i));
    attachment->icon_url = GURL(kGoogleCalendarDriveIconUrl);
    event->attachments.push_back(std::move(attachment));
  }
  event->conference_url =
      GURL("https://foo.com/conference" + base::NumberToString(index));
  event->is_accepted = true;
  event->has_other_attendee = false;
  return event;
}

std::vector<ntp::calendar::mojom::CalendarEventPtr> GetFakeEvents() {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> events;
  for (int i = 0; i < 5; ++i) {
    events.push_back(GetFakeEvent(i));
  }
  return events;
}

std::unique_ptr<google_apis::RequestSender> MakeSender(Profile* profile) {
  std::vector<std::string> scopes = {
      GaiaConstants::kCalendarReadOnlyOAuth2Scope};
  auto url_loader_factory = profile->GetURLLoaderFactory();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<google_apis::RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory, scopes),
      url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/"", kTrafficAnnotation);
}

}  // namespace

// static
void GoogleCalendarPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (base::FeatureList::IsEnabled(ntp_features::kNtpCalendarModule)) {
    registry->RegisterTimePref(kGoogleCalendarLastDismissedTimePrefName,
                               base::Time());
  }
}

GoogleCalendarPageHandler::GoogleCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        handler,
    Profile* profile,
    std::unique_ptr<google_apis::RequestSender> sender)
    : handler_(this, std::move(handler)),
      profile_(profile),
      pref_service_(profile_->GetPrefs()),
      sender_(std::move(sender)) {}

GoogleCalendarPageHandler::GoogleCalendarPageHandler(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        handler,
    Profile* profile)
    : GoogleCalendarPageHandler(std::move(handler),
                                std::move(profile),
                                MakeSender(profile)) {}

GoogleCalendarPageHandler::~GoogleCalendarPageHandler() = default;

void GoogleCalendarPageHandler::GetEvents(GetEventsCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback),
      std::vector<ntp::calendar::mojom::CalendarEventPtr>());

  // Do not grab data if it is within 12 hours since the module was dismissed.
  base::Time dismiss_time =
      pref_service_->GetTime(kGoogleCalendarLastDismissedTimePrefName);
  if (dismiss_time != base::Time() &&
      base::Time::Now() - dismiss_time < base::Hours(12)) {
    std::move(callback).Run(
        std::vector<ntp::calendar::mojom::CalendarEventPtr>());
    return;
  }

  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpCalendarModule,
      ntp_features::kNtpCalendarModuleDataParam);
  if (!fake_data_param.empty()) {
    std::move(callback).Run(GetFakeEvents());
  } else {
    std::vector<google_apis::calendar::EventType> event_types = {
        google_apis::calendar::EventType::kDefault};
    sender_->StartRequestWithAuthRetry(
        std::make_unique<google_apis::calendar::CalendarApiEventsRequest>(
            sender_.get(), url_generator_,
            base::BindOnce(&GoogleCalendarPageHandler::OnRequestComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback)),
            /*start_time=*/base::Time::Now() +
                ntp_features::kNtpCalendarModuleWindowStartDeltaParam.Get(),
            /*end_time=*/base::Time::Now() +
                ntp_features::kNtpCalendarModuleWindowEndDeltaParam.Get(),
            /*event_types=*/event_types,
            ntp_features::kNtpCalendarModuleExperimentParam.Get(),
            /*order_by=*/"startTime"));
    base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                             base::PersistentHash("google_calendar"));
  }
}

void GoogleCalendarPageHandler::DismissModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time::Now());
}

void GoogleCalendarPageHandler::RestoreModule() {
  pref_service_->SetTime(kGoogleCalendarLastDismissedTimePrefName,
                         base::Time());
}

void GoogleCalendarPageHandler::OnRequestComplete(
    GetEventsCallback callback,
    google_apis::ApiErrorCode response_code,
    std::unique_ptr<google_apis::calendar::EventList> events) {
  std::vector<ntp::calendar::mojom::CalendarEventPtr> result;
  size_t max_events =
      static_cast<size_t>(ntp_features::kNtpCalendarModuleMaxEventsParam.Get());
  if (response_code == google_apis::ApiErrorCode::HTTP_SUCCESS) {
    base::UmaHistogramCounts100("NewTabPage.GoogleCalendar.RequestResult",
                                events->items().size());
    for (const auto& event : events->items()) {
      // If the result is already at max length, stop.
      if (result.size() == max_events) {
        break;
      }
      // Do not include all day events in response.
      if (event->all_day_event()) {
        continue;
      }
      // Do not include declined events in response.
      if (event->self_response_status() ==
          google_apis::calendar::CalendarEvent::ResponseStatus::kDeclined) {
        continue;
      }
      ntp::calendar::mojom::CalendarEventPtr formatted_event =
          ntp::calendar::mojom::CalendarEvent::New();
      formatted_event->title = event->summary();
      formatted_event->start_time = event->start_time().date_time();
      formatted_event->end_time = event->end_time().date_time();
      formatted_event->url = GURL(event->html_link());
      formatted_event->location = event->location();
      for (const auto& attachment : event->attachments()) {
        ntp::calendar::mojom::AttachmentPtr formatted_attachment =
            ntp::calendar::mojom::Attachment::New();
        formatted_attachment->title = attachment.title();
        formatted_attachment->resource_url = attachment.file_url();
        formatted_attachment->icon_url = attachment.icon_link();
        formatted_event->attachments.push_back(std::move(formatted_attachment));
      }
      formatted_event->conference_url = event->conference_data_uri();
      formatted_event->is_accepted =
          event->self_response_status() ==
          google_apis::calendar::CalendarEvent::ResponseStatus::kAccepted;
      formatted_event->has_other_attendee = event->has_other_attendee();
      result.push_back(std::move(formatted_event));
    }
  }
  std::move(callback).Run(std::move(result));
}
