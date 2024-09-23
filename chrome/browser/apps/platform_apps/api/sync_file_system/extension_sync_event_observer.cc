// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/sync_file_system/extension_sync_event_observer.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/common/apps/platform_apps/api/sync_file_system.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_set.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

using ::sync_file_system::SyncEventObserver;

namespace chrome_apps {
namespace api {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncActionMetrics {
  kNone = 0,
  kAdded = 1,
  kUpdated = 2,
  kDeleted = 3,
  kMaxValue = kDeleted
};

SyncActionMetrics convertToHistogramEnum(
    ::sync_file_system::SyncAction action) {
  switch (action) {
    case ::sync_file_system::SyncAction::SYNC_ACTION_NONE:
      return SyncActionMetrics::kNone;
    case ::sync_file_system::SyncAction::SYNC_ACTION_ADDED:
      return SyncActionMetrics::kAdded;
    case ::sync_file_system::SyncAction::SYNC_ACTION_UPDATED:
      return SyncActionMetrics::kUpdated;
    case ::sync_file_system::SyncAction::SYNC_ACTION_DELETED:
      return SyncActionMetrics::kDeleted;
  }
  NOTREACHED();
}

}  // namespace

// static
extensions::BrowserContextKeyedAPIFactory<ExtensionSyncEventObserver>*
ExtensionSyncEventObserver::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<ExtensionSyncEventObserver>>
      g_sync_event_observer_factory;
  return g_sync_event_observer_factory.get();
}

ExtensionSyncEventObserver::ExtensionSyncEventObserver(
    content::BrowserContext* context)
    : browser_context_(context), sync_service_(nullptr) {}

void ExtensionSyncEventObserver::InitializeForService(
    ::sync_file_system::SyncFileSystemService* sync_service) {
  DCHECK(sync_service);
  if (sync_service_ != nullptr) {
    DCHECK_EQ(sync_service_, sync_service);
    return;
  }
  sync_service_ = sync_service;
  sync_service_->AddSyncEventObserver(this);
}

ExtensionSyncEventObserver::~ExtensionSyncEventObserver() = default;

void ExtensionSyncEventObserver::Shutdown() {
  if (sync_service_ != nullptr)
    sync_service_->RemoveSyncEventObserver(this);
}

std::string ExtensionSyncEventObserver::GetExtensionId(const GURL& app_origin) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->enabled_extensions()
          .GetAppByURL(app_origin);
  if (!app) {
    // The app is uninstalled or disabled.
    return std::string();
  }
  return app->id();
}

void ExtensionSyncEventObserver::OnSyncStateUpdated(
    const GURL& app_origin,
    ::sync_file_system::SyncServiceState state,
    const std::string& description) {
  // Convert state and description into SyncState Object.
  sync_file_system::ServiceInfo service_info;
  service_info.state = SyncServiceStateToExtensionEnum(state);
  service_info.description = description;
  auto params(sync_file_system::OnServiceStatusChanged::Create(service_info));

  BroadcastOrDispatchEvent(
      app_origin,
      extensions::events::SYNC_FILE_SYSTEM_ON_SERVICE_STATUS_CHANGED,
      sync_file_system::OnServiceStatusChanged::kEventName, std::move(params));
}

void ExtensionSyncEventObserver::OnFileSynced(
    const storage::FileSystemURL& url,
    ::sync_file_system::SyncFileType file_type,
    ::sync_file_system::SyncFileStatus status,
    ::sync_file_system::SyncAction action,
    ::sync_file_system::SyncDirection direction) {
  base::Value::List params;

  std::optional<base::Value::Dict> entry =
      CreateDictionaryValueForFileSystemEntry(url, file_type);
  if (!entry)
    return;
  params.Append(std::move(*entry));

  if (status == ::sync_file_system::SyncFileStatus::SYNC_FILE_STATUS_SYNCED &&
      direction ==
          ::sync_file_system::SyncDirection::SYNC_DIRECTION_REMOTE_TO_LOCAL) {
    base::UmaHistogramEnumeration("Storage.SyncFileSystem.FileSyncAction",
                                  convertToHistogramEnum(action));
  }

  // Status, SyncAction and any optional notes to go here.
  sync_file_system::FileStatus status_enum =
      SyncFileStatusToExtensionEnum(status);
  sync_file_system::SyncAction action_enum = SyncActionToExtensionEnum(action);
  sync_file_system::SyncDirection direction_enum =
      SyncDirectionToExtensionEnum(direction);
  params.Append(sync_file_system::ToString(status_enum));
  params.Append(sync_file_system::ToString(action_enum));
  params.Append(sync_file_system::ToString(direction_enum));

  BroadcastOrDispatchEvent(
      url.origin().GetURL(),
      extensions::events::SYNC_FILE_SYSTEM_ON_FILE_STATUS_CHANGED,
      sync_file_system::OnFileStatusChanged::kEventName, std::move(params));
}

void ExtensionSyncEventObserver::BroadcastOrDispatchEvent(
    const GURL& app_origin,
    extensions::events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List values) {
  // Check to see whether the event should be broadcasted to all listening
  // extensions or sent to a specific extension ID.
  bool broadcast_mode = app_origin.is_empty();
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context_);
  DCHECK(event_router);

  auto event = std::make_unique<extensions::Event>(
      histogram_value, event_name, std::move(values), browser_context_);

  // No app_origin, broadcast to all listening extensions for this event name.
  if (broadcast_mode) {
    event_router->BroadcastEvent(std::move(event));
    return;
  }

  // Dispatch to single extension ID.
  const std::string extension_id = GetExtensionId(app_origin);
  if (extension_id.empty())
    return;
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

}  // namespace api
}  // namespace chrome_apps

template <>
void extensions::BrowserContextKeyedAPIFactory<
    chrome_apps::api::ExtensionSyncEventObserver>::
    DeclareFactoryDependencies() {
  DependsOn(::sync_file_system::SyncFileSystemServiceFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}
