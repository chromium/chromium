// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_event_router.h"

#include "chrome/browser/extensions/api/reading_list/reading_list_util.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/common/extensions/api/reading_list.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

ReadingListEventRouter::ReadingListEventRouter(
    content::BrowserContext* browser_context)
    : reading_list_model_(
          ReadingListModelFactory::GetForBrowserContext(browser_context)),
      profile_(Profile::FromBrowserContext(browser_context)),
      event_router_(EventRouter::Get(browser_context)) {
  reading_list_observation_.Observe(reading_list_model_);
}

ReadingListEventRouter::~ReadingListEventRouter() = default;

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
