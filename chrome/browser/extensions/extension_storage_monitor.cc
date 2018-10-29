// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_storage_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_storage_monitor_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/storage_observer.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;

namespace extensions {

namespace {

// The rate at which we would like to observe storage events.
constexpr base::TimeDelta kStorageEventRate = base::TimeDelta::FromSeconds(30);

// Set the thresholds for the first notification. Once a threshold is exceeded,
// it will be doubled to throttle notifications.
const int64_t kMBytes = 1024 * 1024;
const int64_t kExtensionInitialThreshold = 1000 * kMBytes;

// Notifications have an ID so that we can update them.
const char kNotificationIdFormat[] = "ExtensionStorageMonitor-$1-$2";
const char kSystemNotifierId[] = "ExtensionStorageMonitor";

// A preference that stores the next threshold for displaying a notification
// when an extension or app consumes excessive disk space. This will not be
// set until the extension/app reaches the initial threshold.
const char kPrefNextStorageThreshold[] = "next_storage_threshold";

// If this preference is set to true, notifications will be suppressed when an
// extension or app consumes excessive disk space.
const char kPrefDisableStorageNotifications[] = "disable_storage_notifications";

bool ShouldMonitorStorageFor(const Extension* extension) {
  // Only monitor storage for extensions that are granted unlimited storage.
  // Do not monitor storage for component extensions.
  return extension->permissions_data()->HasAPIPermission(
             APIPermission::kUnlimitedStorage) &&
         extension->location() != Manifest::COMPONENT;
}

bool ShouldGatherMetricsFor(const Extension* extension) {
  // We want to know the usage of hosted apps' storage.
  return ShouldMonitorStorageFor(extension) && extension->is_hosted_app();
}

const Extension* GetExtensionById(content::BrowserContext* context,
                                  const std::string& extension_id) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING);
}

void LogTemporaryStorageUsage(
    scoped_refptr<storage::QuotaManager> quota_manager,
    int64_t usage) {
  const storage::QuotaSettings& settings = quota_manager->settings();
  if (settings.per_host_quota > 0) {
    // Note we use COUNTS_100 (instead of PERCENT) because this can potentially
    // exceed 100%.
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.HostedAppUnlimitedStorageTemporaryStorageUsage",
        100.0 * usage / settings.per_host_quota);
  }
}

}  // namespace

// SingleExtensionStorageObserver monitors the storage usage of one extension,
// and lives on the IO thread. When a threshold is exceeded, a message will be
// posted to the ExtensionStorageMonitor on the UI thread, which displays the
// notification.
class SingleExtensionStorageObserver : public storage::StorageObserver {
 public:
  SingleExtensionStorageObserver(
      ExtensionStorageMonitorIOHelper* io_helper,
      const std::string& extension_id,
      scoped_refptr<storage::QuotaManager> quota_manager,
      const url::Origin& origin,
      int64_t next_threshold,
      base::TimeDelta rate,
      bool should_uma)
      : io_helper_(io_helper),
        extension_id_(extension_id),
        quota_manager_(std::move(quota_manager)),
        next_threshold_(next_threshold),
        should_uma_(should_uma) {
    // We always observe persistent storage usage.
    storage::StorageObserver::MonitorParams params(
        blink::mojom::StorageType::kPersistent, origin, rate, false);
    quota_manager_->AddStorageObserver(this, params);
    if (should_uma) {
      // And if this is for uma, we also observe temporary storage usage.
      MonitorParams temporary_params(blink::mojom::StorageType::kTemporary,
                                     origin, rate, false);
      quota_manager_->AddStorageObserver(this, temporary_params);
    }
  }

  ~SingleExtensionStorageObserver() override {
    // This removes all our registrations.
    quota_manager_->RemoveStorageObserver(this);
  }

  void set_next_threshold(int64_t next_threshold) {
    next_threshold_ = next_threshold;
  }

  // storage::StorageObserver implementation.
  void OnStorageEvent(const Event& event) override;

 private:
  // The IO thread helper that owns this instance.
  ExtensionStorageMonitorIOHelper* const io_helper_;

  // The extension associated with the origin under observation.
  const std::string extension_id_;

  // The quota manager being observed, corresponding to the extension's storage
  // partition.
  scoped_refptr<storage::QuotaManager> quota_manager_;

  // If |next_threshold| is -1, it signifies that we should not enforce (and
  // only track) storage for this extension.
  int64_t next_threshold_;

  const bool should_uma_;

  DISALLOW_COPY_AND_ASSIGN(SingleExtensionStorageObserver);
};

// The IO thread part of ExtensionStorageMonitor. This class manages a flock of
// SingleExtensionStorageObserver instances, one for each tracked extension.
// This class is owned by, and reports back to, ExtensionStorageMonitor.
class ExtensionStorageMonitorIOHelper
    : public base::RefCountedThreadSafe<ExtensionStorageMonitorIOHelper,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  explicit ExtensionStorageMonitorIOHelper(
      base::WeakPtr<ExtensionStorageMonitor> extension_storage_monitor)
      : extension_storage_monitor_(std::move(extension_storage_monitor)) {}

  // Register a StorageObserver for the extension's storage events.
  void StartObservingForExtension(
      scoped_refptr<storage::QuotaManager> quota_manager,
      const std::string& extension_id,
      const url::Origin& site_origin,
      int64_t next_threshold,
      const base::TimeDelta& rate,
      bool should_uma) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(quota_manager.get());

    DCHECK(!FindObserver(extension_id));

    storage_observers_[extension_id] =
        std::make_unique<SingleExtensionStorageObserver>(
            this, extension_id, std::move(quota_manager), site_origin,
            next_threshold, rate, should_uma);
  }

  // Updates the threshold for an extension already being monitored.
  void UpdateThresholdForExtension(const std::string& extension_id,
                                   int64_t next_threshold) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Note that |extension_id| may not be in the map, since some extensions may
    // be exempt from monitoring.
    SingleExtensionStorageObserver* observer = FindObserver(extension_id);
    if (observer)
      observer->set_next_threshold(next_threshold);
  }

  // Deregister as an observer for the extension's storage events.
  void StopObservingForExtension(const std::string& extension_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Note that |extension_id| may not be in the map, since some extensions may
    // be exempt from monitoring.
    storage_observers_.erase(extension_id);
  }

  base::WeakPtr<ExtensionStorageMonitor> extension_storage_monitor() {
    return extension_storage_monitor_;
  }

 private:
  friend class base::DeleteHelper<ExtensionStorageMonitorIOHelper>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;

  ~ExtensionStorageMonitorIOHelper() {}

  SingleExtensionStorageObserver* FindObserver(
      const std::string& extension_id) {
    auto it = storage_observers_.find(extension_id);
    if (it != storage_observers_.end())
      return it->second.get();
    return nullptr;
  }

  // Keys are extension IDs. Values are self-registering StorageObservers.
  std::map<std::string, std::unique_ptr<SingleExtensionStorageObserver>>
      storage_observers_;

  base::WeakPtr<ExtensionStorageMonitor> extension_storage_monitor_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionStorageMonitorIOHelper);
};

void SingleExtensionStorageObserver::OnStorageEvent(const Event& event) {
  if (should_uma_) {
    if (event.filter.storage_type == blink::mojom::StorageType::kPersistent) {
      UMA_HISTOGRAM_MEMORY_KB(
          "Extensions.HostedAppUnlimitedStoragePersistentStorageUsage",
          event.usage);
    } else {
      // We can't use the quota in the event because it assumes unlimited
      // storage.
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                               base::BindOnce(&LogTemporaryStorageUsage,
                                              quota_manager_, event.usage));
    }
  }

  if (next_threshold_ != -1 && event.usage >= next_threshold_) {
    while (event.usage >= next_threshold_)
      next_threshold_ *= 2;

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ExtensionStorageMonitor::OnStorageThresholdExceeded,
                       io_helper_->extension_storage_monitor(), extension_id_,
                       next_threshold_, event.usage));
  }
}

// ExtensionStorageMonitor

// static
ExtensionStorageMonitor* ExtensionStorageMonitor::Get(Profile* profile) {
  return ExtensionStorageMonitorFactory::GetForBrowserContext(profile);
}

ExtensionStorageMonitor::ExtensionStorageMonitor(Profile* profile)
    : enable_for_all_extensions_(false),
      initial_extension_threshold_(kExtensionInitialThreshold),
      observer_rate_(kStorageEventRate),
      profile_(profile),
      extension_prefs_(ExtensionPrefs::Get(profile)),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(extension_prefs_);

  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
}

ExtensionStorageMonitor::~ExtensionStorageMonitor() = default;

void ExtensionStorageMonitor::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  StartMonitoringStorage(extension);
}

void ExtensionStorageMonitor::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  StopMonitoringStorage(extension->id());
}

void ExtensionStorageMonitor::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (!ShouldMonitorStorageFor(extension))
    return;

  if (!enable_for_all_extensions_) {
    // If monitoring is not enabled for installed extensions, just stop
    // monitoring.
    SetNextStorageThreshold(extension->id(), 0);
    StopMonitoringStorage(extension->id());
    return;
  }

  int64_t next_threshold = GetNextStorageThresholdFromPrefs(extension->id());
  if (next_threshold <= initial_extension_threshold_) {
    // Clear the next threshold in the prefs. This effectively raises it to
    // |initial_extension_threshold_|. If the current threshold is already
    // higher than this, leave it as is.
    SetNextStorageThreshold(extension->id(), 0);

    if (io_helper_) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &ExtensionStorageMonitorIOHelper::UpdateThresholdForExtension,
              io_helper_, extension->id(), initial_extension_threshold_));
    }
  }
}

void ExtensionStorageMonitor::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoveNotificationForExtension(extension->id());
}

void ExtensionStorageMonitor::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const base::string16& error) {
  // We may get a lagging OnExtensionUninstalledDialogClosed() call during
  // testing, but did_start_uninstall should be false in this case.
  DCHECK(!uninstall_extension_id_.empty() || !did_start_uninstall);
  uninstall_extension_id_.clear();
}

std::string ExtensionStorageMonitor::GetNotificationId(
    const std::string& extension_id) {
  std::vector<std::string> placeholders;
  placeholders.push_back(profile_->GetPath().BaseName().MaybeAsASCII());
  placeholders.push_back(extension_id);

  return base::ReplaceStringPlaceholders(
      kNotificationIdFormat, placeholders, NULL);
}

void ExtensionStorageMonitor::OnStorageThresholdExceeded(
    const std::string& extension_id,
    int64_t next_threshold,
    int64_t current_usage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const Extension* extension = GetExtensionById(profile_, extension_id);
  if (!extension)
    return;

  if (GetNextStorageThreshold(extension->id()) < next_threshold)
    SetNextStorageThreshold(extension->id(), next_threshold);

  const int kIconSize = message_center::kNotificationIconSize;
  ExtensionResource resource =  IconsInfo::GetIconResource(
      extension, kIconSize, ExtensionIconSet::MATCH_BIGGER);
  ImageLoader::Get(profile_)->LoadImageAsync(
      extension, resource, gfx::Size(kIconSize, kIconSize),
      base::Bind(&ExtensionStorageMonitor::OnImageLoaded,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, current_usage));
}

void ExtensionStorageMonitor::OnImageLoaded(const std::string& extension_id,
                                            int64_t current_usage,
                                            const gfx::Image& image) {
  const Extension* extension = GetExtensionById(profile_, extension_id);
  if (!extension)
    return;

  // Remove any existing notifications to force a new notification to pop up.
  std::string notification_id(GetNotificationId(extension_id));
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);

  message_center::RichNotificationData notification_data;
  notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(extension->is_app() ?
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_DISMISS_APP :
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_DISMISS_EXTENSION)));
  notification_data.buttons.push_back(message_center::ButtonInfo(
      l10n_util::GetStringUTF16(extension->is_app() ?
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_UNINSTALL_APP :
          IDS_EXTENSION_STORAGE_MONITOR_BUTTON_UNINSTALL_EXTENSION)));

  gfx::Image notification_image(image);
  if (notification_image.IsEmpty()) {
    notification_image =
        extension->is_app() ? gfx::Image(util::GetDefaultAppIcon())
                            : gfx::Image(util::GetDefaultExtensionIcon());
  }

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_EXTENSION_STORAGE_MONITOR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_STORAGE_MONITOR_TEXT,
          base::UTF8ToUTF16(extension->name()),
          base::Int64ToString16(current_usage / kMBytes)),
      notification_image, base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kSystemNotifierId),
      notification_data,
      new message_center::HandleNotificationClickDelegate(
          base::Bind(&ExtensionStorageMonitor::OnNotificationButtonClick,
                     weak_ptr_factory_.GetWeakPtr(), extension_id)));
  notification.SetSystemPriority();
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification);

  notified_extension_ids_.insert(extension_id);
}

void ExtensionStorageMonitor::OnNotificationButtonClick(
    const std::string& extension_id,
    base::Optional<int> button_index) {
  if (!button_index)
    return;

  switch (*button_index) {
    case BUTTON_DISABLE_NOTIFICATION: {
      DisableStorageMonitoring(extension_id);
      break;
    }
    case BUTTON_UNINSTALL: {
      ShowUninstallPrompt(extension_id);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionStorageMonitor::DisableStorageMonitoring(
    const std::string& extension_id) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension.get() || !ShouldGatherMetricsFor(extension.get()))
    StopMonitoringStorage(extension_id);

  SetStorageNotificationEnabled(extension_id, false);

  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationId(extension_id));
}

void ExtensionStorageMonitor::StartMonitoringStorage(
    const Extension* extension) {
  if (!ShouldMonitorStorageFor(extension))
    return;

  bool should_enforce = (enable_for_all_extensions_) &&
                        IsStorageNotificationEnabled(extension->id());

  bool for_metrics = ShouldGatherMetricsFor(extension);

  if (!should_enforce && !for_metrics)
    return;  // Don't track this extension.

  // Lazily create the storage monitor proxy on the IO thread.
  if (!io_helper_) {
    io_helper_ = base::MakeRefCounted<ExtensionStorageMonitorIOHelper>(
        weak_ptr_factory_.GetWeakPtr());
  }

  GURL site_url = util::GetSiteForExtensionId(extension->id(), profile_);
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_, site_url);
  DCHECK(storage_partition);
  scoped_refptr<storage::QuotaManager> quota_manager(
      storage_partition->GetQuotaManager());

  url::Origin storage_origin = url::Origin::Create(site_url);
  if (extension->is_hosted_app()) {
    storage_origin =
        url::Origin::Create(AppLaunchInfo::GetLaunchWebURL(extension));
  }

  // Don't give a threshold if we're not enforcing.
  int next_threshold =
      should_enforce ? GetNextStorageThreshold(extension->id()) : -1;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ExtensionStorageMonitorIOHelper::StartObservingForExtension,
          io_helper_, quota_manager, extension->id(), storage_origin,
          next_threshold, observer_rate_, for_metrics));
}

void ExtensionStorageMonitor::StopMonitoringStorage(
    const std::string& extension_id) {
  if (!io_helper_.get())
    return;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ExtensionStorageMonitorIOHelper::StopObservingForExtension,
          io_helper_, extension_id));
}

void ExtensionStorageMonitor::RemoveNotificationForExtension(
    const std::string& extension_id) {
  auto ext_id = notified_extension_ids_.find(extension_id);
  if (ext_id == notified_extension_ids_.end())
    return;

  notified_extension_ids_.erase(ext_id);
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationId(extension_id));
}

void ExtensionStorageMonitor::ShowUninstallPrompt(
    const std::string& extension_id) {
  const Extension* extension = GetExtensionById(profile_, extension_id);
  if (!extension)
    return;

  uninstall_dialog_.reset(
      ExtensionUninstallDialog::Create(profile_, nullptr, this));

  uninstall_extension_id_ = extension->id();
  uninstall_dialog_->ConfirmUninstall(
      extension, extensions::UNINSTALL_REASON_STORAGE_THRESHOLD_EXCEEDED,
      UNINSTALL_SOURCE_STORAGE_THRESHOLD_EXCEEDED);
}

int64_t ExtensionStorageMonitor::GetNextStorageThreshold(
    const std::string& extension_id) const {
  int next_threshold = GetNextStorageThresholdFromPrefs(extension_id);
  if (next_threshold == 0) {
    // The next threshold is written to the prefs after the initial threshold is
    // exceeded.
    next_threshold = initial_extension_threshold_;
  }
  return next_threshold;
}

void ExtensionStorageMonitor::SetNextStorageThreshold(
    const std::string& extension_id,
    int64_t next_threshold) {
  extension_prefs_->UpdateExtensionPref(
      extension_id, kPrefNextStorageThreshold,
      next_threshold > 0
          ? std::make_unique<base::Value>(base::Int64ToString(next_threshold))
          : nullptr);
}

int64_t ExtensionStorageMonitor::GetNextStorageThresholdFromPrefs(
    const std::string& extension_id) const {
  std::string next_threshold_str;
  if (extension_prefs_->ReadPrefAsString(
          extension_id, kPrefNextStorageThreshold, &next_threshold_str)) {
    int64_t next_threshold;
    if (base::StringToInt64(next_threshold_str, &next_threshold))
      return next_threshold;
  }

  // A return value of zero indicates that the initial threshold has not yet
  // been reached.
  return 0;
}

bool ExtensionStorageMonitor::IsStorageNotificationEnabled(
    const std::string& extension_id) const {
  bool disable_notifications;
  if (extension_prefs_->ReadPrefAsBoolean(extension_id,
                                          kPrefDisableStorageNotifications,
                                          &disable_notifications)) {
    return !disable_notifications;
  }

  return true;
}

void ExtensionStorageMonitor::SetStorageNotificationEnabled(
    const std::string& extension_id,
    bool enable_notifications) {
  extension_prefs_->UpdateExtensionPref(
      extension_id, kPrefDisableStorageNotifications,
      enable_notifications ? nullptr : std::make_unique<base::Value>(true));
}

}  // namespace extensions
