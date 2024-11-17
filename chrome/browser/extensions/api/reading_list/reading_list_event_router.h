// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace extensions {

// The ReadingListEventRouter listens for reading list events and notifies
// observers of the changes.
class ReadingListEventRouter : public KeyedService,
                               public ReadingListModelObserver {
 public:
  explicit ReadingListEventRouter(content::BrowserContext* browser_context);
  ReadingListEventRouter(const ReadingListEventRouter&) = delete;
  ReadingListEventRouter& operator=(const ReadingListEventRouter&) = delete;
  ~ReadingListEventRouter() override;

 private:
  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override {}
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                 const GURL& url) override;

  // TODO(crbug.com/40260548): Remove when MoveEntry is replaced with
  // UpdateEntry Called when the read status of an entry is changed.
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override;

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args);

  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_observation_{this};

  // Guaranteed to outlive this object since it is declared as a KeyedService
  // dependency.
  raw_ptr<ReadingListModel> reading_list_model_;

  // Guaranteed to outlive this object since it is declared as a KeyedService
  // dependency.
  raw_ptr<Profile> const profile_;

  // Notifies observers of events associated with the profile. Guaranteed to
  // outlive this object since it is declared as a KeyedService dependency.
  raw_ptr<EventRouter> const event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_H_
