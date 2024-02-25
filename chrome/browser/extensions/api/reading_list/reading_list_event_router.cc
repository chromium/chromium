// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_event_router.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/reading_list/reading_list_util.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

namespace {

// The factory responsible for creating the per-profile event router for the
// ReadingList API.
class ReadingListEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  ReadingListEventRouterFactory();
  ReadingListEventRouterFactory(const ReadingListEventRouterFactory&) = delete;
  ReadingListEventRouterFactory& operator=(
      const ReadingListEventRouterFactory&) = delete;
  ~ReadingListEventRouterFactory() override = default;

  // Given a browser context, returns the corresponding ReadingListEventRouter.
  ReadingListEventRouter* GetForProfile(content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

ReadingListEventRouterFactory::ReadingListEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListEventRouter",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
}

ReadingListEventRouter* ReadingListEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<ReadingListEventRouter*>(
      GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* ReadingListEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ReadingListEventRouter(context);
}

bool ReadingListEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Since there is a dependency on `EventRouter` that is null by default in unit
// tests, this service needs to be null as well. If we want to enable it in a
// specific test we need to override the factories for both `EventRouter` and
// this factory to enforce the service creation.
bool ReadingListEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace

ReadingListEventRouter::ReadingListEventRouter(
    content::BrowserContext* browser_context)
    : reading_list_model_(
          ReadingListModelFactory::GetForBrowserContext(browser_context)),
      profile_(Profile::FromBrowserContext(browser_context)),
      event_router_(EventRouter::Get(browser_context)) {
  reading_list_observation_.Observe(reading_list_model_);
}

ReadingListEventRouter::~ReadingListEventRouter() = default;

// static
ReadingListEventRouter* ReadingListEventRouter::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ReadingListEventRouterFactory*>(GetFactoryInstance())
      ->GetForProfile(browser_context);
}

// static
ProfileKeyedServiceFactory* ReadingListEventRouter::GetFactoryInstance() {
  static base::NoDestructor<ReadingListEventRouterFactory> factory;
  return factory.get();
}

void ReadingListEventRouter::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  auto args(api::reading_list::OnEntryAdded::Create(
      reading_list_util::ParseEntry(*model->GetEntryByURL(url))));

  DispatchEvent(events::READING_LIST_ON_ENTRY_ADDED,
                api::reading_list::OnEntryAdded::kEventName, std::move(args));
}

void ReadingListEventRouter::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  auto args(api::reading_list::OnEntryRemoved::Create(
      reading_list_util::ParseEntry(*model->GetEntryByURL(url))));

  // Even though we dispatch the event in ReadingListWillRemoveEntry() (i.e.,
  // the entry is still in the model at this point), we can safely dispatch it
  // as `onEntryRemoved` (past tense) to the extension. The entry is removed
  // synchronously after this, so there's no way the extension could still
  // see the entry in the list.
  DispatchEvent(events::READING_LIST_ON_ENTRY_REMOVED,
                api::reading_list::OnEntryRemoved::kEventName, std::move(args));
}

void ReadingListEventRouter::ReadingListDidUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  auto args(api::reading_list::OnEntryUpdated::Create(
      reading_list_util::ParseEntry(*model->GetEntryByURL(url))));

  DispatchEvent(events::READING_LIST_ON_ENTRY_UPDATED,
                api::reading_list::OnEntryUpdated::kEventName, std::move(args));
}

void ReadingListEventRouter::ReadingListDidMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  auto args(api::reading_list::OnEntryUpdated::Create(
      reading_list_util::ParseEntry(*model->GetEntryByURL(url))));

  DispatchEvent(events::READING_LIST_ON_ENTRY_UPDATED,
                api::reading_list::OnEntryUpdated::kEventName, std::move(args));
}

void ReadingListEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args) {
  event_router_->BroadcastEvent(std::make_unique<Event>(
      histogram_value, event_name, std::move(args), profile_));
}

}  // namespace extensions
