// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This extension API provides access to the Activity Log, which is a
// monitoring framework for extension behavior. Only specific Google-produced
// extensions should have access to it.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ACTIVITY_LOG_PRIVATE_ACTIVITY_LOG_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ACTIVITY_LOG_PRIVATE_ACTIVITY_LOG_PRIVATE_API_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace extensions {

class ActivityLog;

// Handles interactions between the Activity Log API and implementation.
class ActivityLogAPI : public BrowserContextKeyedAPI,
                       public extensions::ActivityLog::Observer,
                       public EventRouter::Observer {
 public:
  explicit ActivityLogAPI(content::BrowserContext* context);
  ~ActivityLogAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ActivityLogAPI>* GetFactoryInstance();

  void Shutdown() override;

  // Lookup whether the extension ID is whitelisted.
  static bool IsExtensionWhitelisted(const std::string& extension_id);

 private:
  friend class BrowserContextKeyedAPIFactory<ActivityLogAPI>;
  static const char* service_name() { return "ActivityLogPrivateAPI"; }

  // ActivityLog::Observer
  // We pass this along to activityLogPrivate.onExtensionActivity.
  void OnExtensionActivity(scoped_refptr<Action> activity) override;

  // EventRouter::Observer
  // We only keep track of OnExtensionActivity if we have any listeners.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  void StartOrStopListeningForExtensionActivities();

  content::BrowserContext* browser_context_;
  ActivityLog* activity_log_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogAPI);
};

template <>
void
    BrowserContextKeyedAPIFactory<ActivityLogAPI>::DeclareFactoryDependencies();

// The implementation of activityLogPrivate.getExtensionActivities
class ActivityLogPrivateGetExtensionActivitiesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("activityLogPrivate.getExtensionActivities",
                             ACTIVITYLOGPRIVATE_GETEXTENSIONACTIVITIES)

 protected:
  ~ActivityLogPrivateGetExtensionActivitiesFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void OnLookupCompleted(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> activities);
};

// The implementation of activityLogPrivate.deleteActivities
class ActivityLogPrivateDeleteActivitiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("activityLogPrivate.deleteActivities",
                             ACTIVITYLOGPRIVATE_DELETEACTIVITIES)

 protected:
  ~ActivityLogPrivateDeleteActivitiesFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// The implementation of activityLogPrivate.deleteActivitiesByExtension
class ActivityLogPrivateDeleteActivitiesByExtensionFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("activityLogPrivate.deleteActivitiesByExtension",
                             ACTIVITYLOGPRIVATE_DELETEACTIVITIESBYEXTENSION)

 protected:
  ~ActivityLogPrivateDeleteActivitiesByExtensionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// The implementation of activityLogPrivate.deleteDatabase
class ActivityLogPrivateDeleteDatabaseFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("activityLogPrivate.deleteDatabase",
                             ACTIVITYLOGPRIVATE_DELETEDATABASE)

 protected:
  ~ActivityLogPrivateDeleteDatabaseFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

// The implementation of activityLogPrivate.deleteUrls
class ActivityLogPrivateDeleteUrlsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("activityLogPrivate.deleteUrls",
                             ACTIVITYLOGPRIVATE_DELETEURLS)

 protected:
  ~ActivityLogPrivateDeleteUrlsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ACTIVITY_LOG_PRIVATE_ACTIVITY_LOG_PRIVATE_API_H_
