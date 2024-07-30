// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/calendar/calendar_keyed_service.h"

#include <string>
#include <vector>

#include "ash/calendar/calendar_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using google_apis::RequestSender;
using google_apis::calendar::CalendarApiCalendarListRequest;
using google_apis::calendar::CalendarApiEventsRequest;
using google_apis::calendar::CalendarEventListCallback;
using google_apis::calendar::CalendarListCallback;

namespace ash {
namespace {

constexpr net::NetworkTrafficAnnotationTag kCalendarTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("calendar_get_events", R"(
       semantics {
         sender: "Calendar Keyed Service"
         description:
            "Fetch a Chrome OS user's Google Calendar calendar list or event "
            "list in order to display their events in the Quick Settings "
            "Calendar."
          trigger:
              "Chrome OS system tray calendar view is opened by the user."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "jiamingc@google.com"
            }
            contacts {
              email: "cros-status-area-eng@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2023-08-01"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
            CalendarIntegrationEnabled {
              CalendarIntegrationEnabled: false
            }
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
        })");

}  // namespace

CalendarKeyedService::CalendarKeyedService(Profile* profile,
                                           const AccountId& account_id)
    : profile_(profile), account_id_(account_id), calendar_client_(profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  // Instance check for tests.
  if (Shell::HasInstance()) {
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id_, &calendar_client_);
  }
  Initialize();
}

CalendarKeyedService::~CalendarKeyedService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CalendarKeyedService::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kCalendarReadOnlyOAuth2Scope);
  url_loader_factory_ = profile_->GetURLLoaderFactory();
  sender_ = std::make_unique<RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager_,
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory_, scopes),
      url_loader_factory_,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), kCalendarTrafficAnnotation);
}

void CalendarKeyedService::SetUrlForTesting(const std::string& url) {
  url_generator_.SetBaseUrlForTesting(url);  // IN-TEST
}

void CalendarKeyedService::Shutdown() {
  if (Shell::HasInstance()) {
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id_,
        /*client=*/nullptr);
  }
  // Reset `sender_` early to prevent a crash during destruction of
  // CalendarKeyedService. See https://crbug.com/1319563.
  sender_.reset();
}

base::OnceClosure CalendarKeyedService::GetCalendarList(
    CalendarListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(callback);

  if (!sender_) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*calendars=*/nullptr);
    return base::DoNothing();
  }

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<CalendarApiCalendarListRequest>(
          sender_.get(), url_generator_, std::move(callback)));
}

base::OnceClosure CalendarKeyedService::GetEventList(
    CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(callback);
  if (start_time > end_time || !sender_) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<CalendarApiEventsRequest>(sender_.get(), url_generator_,
                                                 std::move(callback),
                                                 start_time, end_time));
}

base::OnceClosure CalendarKeyedService::GetEventList(
    CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time,
    const std::string& calendar_id,
    const std::string& calendar_color_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(callback);
  if (start_time > end_time || !sender_) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  return sender_->StartRequestWithAuthRetry(
      std::make_unique<CalendarApiEventsRequest>(
          sender_.get(), url_generator_, std::move(callback), start_time,
          end_time, calendar_id, calendar_color_id));
}

}  // namespace ash
