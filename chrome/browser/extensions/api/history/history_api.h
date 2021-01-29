// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/history.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace base {
class ListValue;
}

namespace extensions {

// Observes History service and routes the notifications as events to the
// extension system.
class HistoryEventRouter : public history::HistoryServiceObserver {
 public:
  HistoryEventRouter(Profile* profile,
                     history::HistoryService* history_service);
  ~HistoryEventRouter() override;

 private:
  // history::HistoryServiceObserver.
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

  Profile* profile_;
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(HistoryEventRouter);
};

class HistoryAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit HistoryAPI(content::BrowserContext* context);
  ~HistoryAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HistoryAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<HistoryAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "HistoryAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<HistoryEventRouter> history_event_router_;
};

template <>
void BrowserContextKeyedAPIFactory<HistoryAPI>::DeclareFactoryDependencies();

// Base class for history function APIs.
class HistoryFunction : public ExtensionFunction {
 protected:
  ~HistoryFunction() override {}

  bool ValidateUrl(const std::string& url_string,
                   GURL* url,
                   std::string* error);
  bool VerifyDeleteAllowed(std::string* error);
  base::Time GetTime(double ms_from_epoch);
  Profile* GetProfile() const;
};

// Base class for history funciton APIs which require async interaction with
// chrome services and the extension thread.
class HistoryFunctionWithCallback : public HistoryFunction {
 public:
  HistoryFunctionWithCallback();

 protected:
  ~HistoryFunctionWithCallback() override;

  // The task tracker for the HistoryService callbacks.
  base::CancelableTaskTracker task_tracker_;
};

class HistoryGetVisitsFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.getVisits", HISTORY_GETVISITS)

 protected:
  ~HistoryGetVisitsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the history function to provide results.
  void QueryComplete(history::QueryURLResult result);
};

class HistorySearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.search", HISTORY_SEARCH)

 protected:
  ~HistorySearchFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults results);
};

class HistoryAddUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.addUrl", HISTORY_ADDURL)

 protected:
  ~HistoryAddUrlFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryDeleteAllFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteAll", HISTORY_DELETEALL)

 protected:
  ~HistoryDeleteAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};


class HistoryDeleteUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteUrl", HISTORY_DELETEURL)

 protected:
  ~HistoryDeleteUrlFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryDeleteRangeFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteRange", HISTORY_DELETERANGE)

 protected:
  ~HistoryDeleteRangeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
