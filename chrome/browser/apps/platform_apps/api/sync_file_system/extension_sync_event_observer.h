// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace content {
class BrowserContext;
}

namespace sync_file_system {
class SyncFileSystemService;
}

namespace chrome_apps {
namespace api {

// Observes changes in SyncFileSystem and relays events to JS Extension API.
class ExtensionSyncEventObserver : public sync_file_system::SyncEventObserver,
                                   public extensions::BrowserContextKeyedAPI {
 public:
  static extensions::BrowserContextKeyedAPIFactory<ExtensionSyncEventObserver>*
  GetFactoryInstance();

  explicit ExtensionSyncEventObserver(content::BrowserContext* context);
  ~ExtensionSyncEventObserver() override;

  void InitializeForService(
      sync_file_system::SyncFileSystemService* sync_service);

  // KeyedService override.
  void Shutdown() override;

  // sync_file_system::SyncEventObserver interface implementation.
  void OnSyncStateUpdated(const GURL& app_origin,
                          sync_file_system::SyncServiceState state,
                          const std::string& description) override;

  void OnFileSynced(const storage::FileSystemURL& url,
                    sync_file_system::SyncFileType file_type,
                    sync_file_system::SyncFileStatus status,
                    sync_file_system::SyncAction action,
                    sync_file_system::SyncDirection direction) override;

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<
      ExtensionSyncEventObserver>;

  // Returns an empty string if the extension |app_origin| cannot be found
  // in the installed extension list.
  std::string GetExtensionId(const GURL& app_origin);

  // extensions::BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ExtensionSyncEventObserver"; }
  static const bool kServiceIsCreatedWithBrowserContext = false;

  content::BrowserContext* browser_context_;

  // Not owned. If not null, then this is registered to SyncFileSystemService.
  sync_file_system::SyncFileSystemService* sync_service_;

  void BroadcastOrDispatchEvent(
      const GURL& app_origin,
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> value);

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncEventObserver);
};

}  // namespace api
}  // namespace chrome_apps

template <>
void extensions::BrowserContextKeyedAPIFactory<
    chrome_apps::api::ExtensionSyncEventObserver>::DeclareFactoryDependencies();

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_EXTENSION_SYNC_EVENT_OBSERVER_H_
