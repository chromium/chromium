// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/history/history_api.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/history.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

using api::history::HistoryItem;
using api::history::VisitItem;

typedef std::vector<api::history::HistoryItem> HistoryItemList;
typedef std::vector<api::history::VisitItem> VisitItemList;

namespace AddUrl = api::history::AddUrl;
namespace DeleteUrl = api::history::DeleteUrl;
namespace DeleteRange = api::history::DeleteRange;
namespace GetVisits = api::history::GetVisits;
namespace OnVisited = api::history::OnVisited;
namespace OnVisitRemoved = api::history::OnVisitRemoved;
namespace Search = api::history::Search;

namespace {

const char kInvalidUrlError[] = "Url is invalid.";
const char kDeleteProhibitedError[] = "Browsing history is not allowed to be "
                                      "deleted.";

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

HistoryItem GetHistoryItem(const history::URLRow& row) {
  HistoryItem history_item;

  history_item.id = base::NumberToString(row.id());
  history_item.url.reset(new std::string(row.url().spec()));
  history_item.title.reset(new std::string(base::UTF16ToUTF8(row.title())));
  history_item.last_visit_time.reset(
      new double(MilliSecondsFromTime(row.last_visit())));
  history_item.typed_count.reset(new int(row.typed_count()));
  history_item.visit_count.reset(new int(row.visit_count()));

  return history_item;
}

VisitItem GetVisitItem(const history::VisitRow& row) {
  VisitItem visit_item;

  visit_item.id = base::NumberToString(row.url_id);
  visit_item.visit_id = base::NumberToString(row.visit_id);
  visit_item.visit_time.reset(new double(MilliSecondsFromTime(row.visit_time)));
  visit_item.referring_visit_id = base::NumberToString(row.referring_visit);

  api::history::TransitionType transition = api::history::TRANSITION_TYPE_LINK;
  switch (row.transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      transition = api::history::TRANSITION_TYPE_LINK;
      break;
    case ui::PAGE_TRANSITION_TYPED:
      transition = api::history::TRANSITION_TYPE_TYPED;
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      transition = api::history::TRANSITION_TYPE_AUTO_BOOKMARK;
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      transition = api::history::TRANSITION_TYPE_AUTO_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      transition = api::history::TRANSITION_TYPE_MANUAL_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      transition = api::history::TRANSITION_TYPE_GENERATED;
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      transition = api::history::TRANSITION_TYPE_AUTO_TOPLEVEL;
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      transition = api::history::TRANSITION_TYPE_FORM_SUBMIT;
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      transition = api::history::TRANSITION_TYPE_RELOAD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      transition = api::history::TRANSITION_TYPE_KEYWORD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      transition = api::history::TRANSITION_TYPE_KEYWORD_GENERATED;
      break;
    default:
      DCHECK(false);
  }

  visit_item.transition = transition;

  return visit_item;
}

}  // namespace

HistoryEventRouter::HistoryEventRouter(Profile* profile,
                                       history::HistoryService* history_service)
    : profile_(profile) {
  DCHECK(profile);
  history_service_observer_.Add(history_service);
}

HistoryEventRouter::~HistoryEventRouter() {
}

void HistoryEventRouter::OnURLVisited(history::HistoryService* history_service,
                                      ui::PageTransition transition,
                                      const history::URLRow& row,
                                      const history::RedirectList& redirects,
                                      base::Time visit_time) {
  std::unique_ptr<base::ListValue> args =
      OnVisited::Create(GetHistoryItem(row));
  DispatchEvent(profile_, events::HISTORY_ON_VISITED,
                api::history::OnVisited::kEventName, std::move(args));
}

void HistoryEventRouter::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  OnVisitRemoved::Removed removed;
  removed.all_history = deletion_info.IsAllHistory();

  std::vector<std::string>* urls = new std::vector<std::string>();
  for (const auto& row : deletion_info.deleted_rows())
    urls->push_back(row.url().spec());
  removed.urls.reset(urls);

  std::unique_ptr<base::ListValue> args = OnVisitRemoved::Create(removed);
  DispatchEvent(profile_, events::HISTORY_ON_VISIT_REMOVED,
                api::history::OnVisitRemoved::kEventName, std::move(args));
}

void HistoryEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  if (profile && EventRouter::Get(profile)) {
    auto event = std::make_unique<Event>(histogram_value, event_name,
                                         std::move(event_args), profile);
    EventRouter::Get(profile)->BroadcastEvent(std::move(event));
  }
}

HistoryAPI::HistoryAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, api::history::OnVisited::kEventName);
  event_router->RegisterObserver(this,
                                 api::history::OnVisitRemoved::kEventName);
}

HistoryAPI::~HistoryAPI() {
}

void HistoryAPI::Shutdown() {
  history_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<HistoryAPI>>::
    DestructorAtExit g_history_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<HistoryAPI>* HistoryAPI::GetFactoryInstance() {
  return g_history_api_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<HistoryAPI>::DeclareFactoryDependencies() {
  DependsOn(ActivityLog::GetFactoryInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

void HistoryAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  history_event_router_.reset(new HistoryEventRouter(
      profile, HistoryServiceFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

bool HistoryFunction::ValidateUrl(const std::string& url_string,
                                  GURL* url,
                                  std::string* error) {
  GURL temp_url(url_string);
  if (!temp_url.is_valid()) {
    *error = kInvalidUrlError;
    return false;
  }
  url->Swap(&temp_url);
  return true;
}

bool HistoryFunction::VerifyDeleteAllowed(std::string* error) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    *error = kDeleteProhibitedError;
    return false;
  }
  return true;
}

base::Time HistoryFunction::GetTime(double ms_from_epoch) {
  // The history service has seconds resolution, while javascript Date() has
  // milliseconds resolution.
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  // Time::FromDoubleT converts double time 0 to empty Time object. So we need
  // to do special handling here.
  return (seconds_from_epoch == 0) ?
      base::Time::UnixEpoch() : base::Time::FromDoubleT(seconds_from_epoch);
}

Profile* HistoryFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

HistoryFunctionWithCallback::HistoryFunctionWithCallback() {}

HistoryFunctionWithCallback::~HistoryFunctionWithCallback() {}

ExtensionFunction::ResponseAction HistoryGetVisitsFunction::Run() {
  std::unique_ptr<GetVisits::Params> params(GetVisits::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error))
    return RespondNow(Error(error));

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryURL(url,
               true,  // Retrieve full history of a URL.
               base::BindOnce(&HistoryGetVisitsFunction::QueryComplete,
                              base::Unretained(this)),
               &task_tracker_);
  AddRef();               // Balanced in QueryComplete().
  return RespondLater();  // QueryComplete() will be called asynchronously.
}

void HistoryGetVisitsFunction::QueryComplete(history::QueryURLResult result) {
  VisitItemList visit_item_vec;
  if (result.success && !result.visits.empty()) {
    for (const history::VisitRow& visit : result.visits)
      visit_item_vec.push_back(GetVisitItem(visit));
  }

  Respond(ArgumentList(GetVisits::Results::Create(visit_item_vec)));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction HistorySearchFunction::Run() {
  std::unique_ptr<Search::Params> params(Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 search_text = base::UTF8ToUTF16(params->query.text);

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (params->query.start_time.get())
    options.begin_time = GetTime(*params->query.start_time);
  if (params->query.end_time.get())
    options.end_time = GetTime(*params->query.end_time);
  if (params->query.max_results.get())
    options.max_count = *params->query.max_results;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text, options,
                   base::BindOnce(&HistorySearchFunction::SearchComplete,
                                  base::Unretained(this)),
                   &task_tracker_);

  AddRef();               // Balanced in SearchComplete().
  return RespondLater();  // SearchComplete() will be called asynchronously.
}

void HistorySearchFunction::SearchComplete(history::QueryResults results) {
  HistoryItemList history_item_vec;
  if (!results.empty()) {
    for (const auto& item : results)
      history_item_vec.push_back(GetHistoryItem(item));
  }
  Respond(ArgumentList(Search::Results::Create(history_item_vec)));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction HistoryAddUrlFunction::Run() {
  std::unique_ptr<AddUrl::Params> params(AddUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  std::string error;
  if (!ValidateUrl(params->details.url, &url, &error))
    return RespondNow(Error(error));

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->AddPage(url, base::Time::Now(), history::SOURCE_EXTENSION);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction HistoryDeleteUrlFunction::Run() {
  std::unique_ptr<DeleteUrl::Params> params(DeleteUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  if (!VerifyDeleteAllowed(&error))
    return RespondNow(Error(error));

  GURL url;
  if (!ValidateUrl(params->details.url, &url, &error))
    return RespondNow(Error(error));

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(GetProfile());
  hs->DeleteLocalAndRemoteUrl(web_history, url);

  // Also clean out from the activity log. If the activity log testing flag is
  // set then don't clean so testers can see what potentially malicious
  // extensions have been trying to clean from their logs.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURL(url);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction HistoryDeleteRangeFunction::Run() {
  std::unique_ptr<DeleteRange::Params> params(
      DeleteRange::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  if (!VerifyDeleteAllowed(&error))
    return RespondNow(Error(error));

  base::Time start_time = GetTime(params->range.start_time);
  base::Time end_time = GetTime(params->range.end_time);

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(GetProfile());
  hs->DeleteLocalAndRemoteHistoryBetween(
      web_history, start_time, end_time,
      base::BindOnce(&HistoryDeleteRangeFunction::DeleteComplete,
                     base::Unretained(this)),
      &task_tracker_);

  // Also clean from the activity log unless in testing mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURLs(/*restrict_urls=*/std::vector<GURL>());
  }

  AddRef();               // Balanced in DeleteComplete().
  return RespondLater();  // DeleteComplete() will be called asynchronously.
}

void HistoryDeleteRangeFunction::DeleteComplete() {
  Respond(NoArguments());
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction HistoryDeleteAllFunction::Run() {
  std::string error;
  if (!VerifyDeleteAllowed(&error))
    return RespondNow(Error(error));

  std::set<GURL> restrict_urls;
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(GetProfile());
  hs->DeleteLocalAndRemoteHistoryBetween(
      web_history,
      /*begin_time*/ base::Time(),
      /*end_time*/ base::Time::Max(),
      base::BindOnce(&HistoryDeleteAllFunction::DeleteComplete,
                     base::Unretained(this)),
      &task_tracker_);

  // Also clean from the activity log unless in testing mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURLs(/*restrict_urls=*/std::vector<GURL>());
  }

  AddRef();               // Balanced in DeleteComplete().
  return RespondLater();  // DeleteComplete() will be called asynchronously.
}

void HistoryDeleteAllFunction::DeleteComplete() {
  Respond(NoArguments());
  Release();  // Balanced in Run().
}

}  // namespace extensions
