// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_notifications.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/net_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "chrome/browser/printing/cloud_print/privet_traffic_detector.h"
#endif

namespace cloud_print {

namespace {

const int kTenMinutesInSeconds = 600;
const char kPrivetInfoKeyUptime[] = "uptime";
const char kPrivetNotificationID[] = "privet_notification";
const char kPrivetNotificationOriginUrl[] = "chrome://devices";
const int kStartDelaySeconds = 5;

}  // namespace

PrivetNotificationsListener::PrivetNotificationsListener(
    std::unique_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory,
    Delegate* delegate)
    : delegate_(delegate), devices_active_(0) {
  privet_http_factory_.swap(privet_http_factory);
}

PrivetNotificationsListener::~PrivetNotificationsListener() {
}

void PrivetNotificationsListener::DeviceChanged(
    const std::string& name,
    const DeviceDescription& description) {
  auto it = devices_seen_.find(name);
  if (it != devices_seen_.end()) {
    if (!description.id.empty() &&  // Device is registered
        it->second->notification_may_be_active) {
      it->second->notification_may_be_active = false;
      devices_active_--;
      NotifyDeviceRemoved();
    }
    return;  // Already saw this device.
  }

  std::unique_ptr<DeviceContext>& device_context = devices_seen_[name];
  device_context.reset(new DeviceContext);
  device_context->notification_may_be_active = false;
  device_context->registered = !description.id.empty();

  if (device_context->registered)
    return;

  device_context->privet_http_resolution =
      privet_http_factory_->CreatePrivetHTTP(name);
  device_context->privet_http_resolution->Start(
      description.address,
      base::BindOnce(&PrivetNotificationsListener::CreateInfoOperation,
                     base::Unretained(this)));
}

void PrivetNotificationsListener::CreateInfoOperation(
    std::unique_ptr<PrivetHTTPClient> http_client) {
  // Do nothing if resolution fails.
  if (!http_client)
    return;

  std::string name = http_client->GetName();
  auto it = devices_seen_.find(name);
  if (it == devices_seen_.end())
    return;

  DeviceContext* device = it->second.get();
  device->privet_http.swap(http_client);
  device->info_operation = device->privet_http->CreateInfoOperation(
      base::BindOnce(&PrivetNotificationsListener::OnPrivetInfoDone,
                     base::Unretained(this), device));
  device->info_operation->Start();
}

void PrivetNotificationsListener::OnPrivetInfoDone(
    DeviceContext* device,
    const base::DictionaryValue* json_value) {
  int uptime;

  if (!json_value ||
      !json_value->GetInteger(kPrivetInfoKeyUptime, &uptime) ||
      uptime > kTenMinutesInSeconds) {
    return;
  }

  DCHECK(!device->notification_may_be_active);
  device->notification_may_be_active = true;
  devices_active_++;
  delegate_->PrivetNotify(devices_active_, true);
}

void PrivetNotificationsListener::DeviceRemoved(const std::string& name) {
  auto it = devices_seen_.find(name);
  if (it == devices_seen_.end())
    return;

  DeviceContext* device = it->second.get();
  device->info_operation.reset();
  device->privet_http_resolution.reset();
  if (!device->notification_may_be_active)
    return;

  device->notification_may_be_active = false;
  devices_active_--;
  NotifyDeviceRemoved();
}

void PrivetNotificationsListener::DeviceCacheFlushed() {
  for (const auto& it : devices_seen_) {
    DeviceContext* device = it.second.get();
    device->info_operation.reset();
    device->privet_http_resolution.reset();
    device->notification_may_be_active = false;
  }

  devices_active_ = 0;
  NotifyDeviceRemoved();
}

void PrivetNotificationsListener::NotifyDeviceRemoved() {
  if (devices_active_ == 0) {
    delegate_->PrivetRemoveNotification();
  } else {
    delegate_->PrivetNotify(devices_active_, false);
  }
}

PrivetNotificationsListener::DeviceContext::DeviceContext() {
}

PrivetNotificationsListener::DeviceContext::~DeviceContext() {
}

PrivetNotificationService::PrivetNotificationService(
    content::BrowserContext* profile)
    : profile_(profile) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PrivetNotificationService::Start, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kStartDelaySeconds +
                                   base::RandInt(0, kStartDelaySeconds / 4)));
}

PrivetNotificationService::~PrivetNotificationService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PrivetNotificationService::DeviceChanged(
    const std::string& name,
    const DeviceDescription& description) {
  privet_notifications_listener_->DeviceChanged(name, description);
}

void PrivetNotificationService::DeviceRemoved(const std::string& name) {
  privet_notifications_listener_->DeviceRemoved(name);
}

void PrivetNotificationService::DeviceCacheFlushed() {
  privet_notifications_listener_->DeviceCacheFlushed();
}

// static
bool PrivetNotificationService::IsEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(
      switches::kDisableDeviceDiscoveryNotifications);
}

// static
bool PrivetNotificationService::IsForced() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableDeviceDiscoveryNotifications);
}

void PrivetNotificationService::PrivetNotify(int devices_active,
                                             bool added) {
  DCHECK_GT(devices_active, 0);

  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(profile_))
      ->GetDisplayed(base::BindOnce(&PrivetNotificationService::AddNotification,
                                    AsWeakPtr(), devices_active, added));
}

void PrivetNotificationService::AddNotification(
    int devices_active,
    bool device_added,
    std::set<std::string> displayed_notifications,
    bool supports_synchronization) {
  // If the UI is already open or a device was removed, we'll update the
  // existing notification but not add a new one.
  const bool notification_exists =
      base::Contains(displayed_notifications, kPrivetNotificationID);
  const bool add_new_notification =
      device_added &&
      !local_discovery::LocalDiscoveryUIHandler::GetHasVisible();
  if (!notification_exists && !add_new_notification)
    return;

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_LOCAL_DISCOVERY_NOTIFICATION_BUTTON_PRINTER)));
  rich_notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_LOCAL_DISCOVERY_NOTIFICATIONS_DISABLE_BUTTON_LABEL)));

  base::string16 title = l10n_util::GetPluralStringFUTF16(
      IDS_LOCAL_DISCOVERY_NOTIFICATION_TITLE_PRINTER, devices_active);
  base::string16 body = l10n_util::GetPluralStringFUTF16(
      IDS_LOCAL_DISCOVERY_NOTIFICATION_CONTENTS_PRINTER, devices_active);
  base::string16 product_name =
      l10n_util::GetStringUTF16(IDS_LOCAL_DISCOVERY_SERVICE_NAME_PRINTER);

  Profile* profile = Profile::FromBrowserContext(profile_);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kPrivetNotificationID, title,
      body,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_LOCAL_DISCOVERY_CLOUDPRINT_ICON),
      product_name, GURL(kPrivetNotificationOriginUrl),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivetNotificationID),
      rich_notification_data, CreateNotificationDelegate(profile));

  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(profile_))
      ->Display(NotificationHandler::Type::TRANSIENT, notification,
                /*metadata=*/nullptr);
}

void PrivetNotificationService::PrivetRemoveNotification() {
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(profile_))
      ->Close(NotificationHandler::Type::TRANSIENT, kPrivetNotificationID);
}

void PrivetNotificationService::Start() {
#if defined(OS_CHROMEOS)
  auto* identity_manager = IdentityManagerFactory::GetForProfileIfExists(
      Profile::FromBrowserContext(profile_));

  if (!identity_manager || !identity_manager->HasPrimaryAccount())
    return;
#endif  // defined(OS_CHROMEOS)

  enable_privet_notification_member_.Init(
      prefs::kLocalDiscoveryNotificationsEnabled,
      Profile::FromBrowserContext(profile_)->GetPrefs(),
      base::BindRepeating(
          &PrivetNotificationService::OnNotificationsEnabledChanged,
          base::Unretained(this)));
  OnNotificationsEnabledChanged();
}

void PrivetNotificationService::OnNotificationsEnabledChanged() {
#if BUILDFLAG(ENABLE_MDNS)
  traffic_detector_.reset();

  if (IsForced()) {
    StartLister();
  } else if (*enable_privet_notification_member_) {
    traffic_detector_ = std::make_unique<PrivetTrafficDetector>(
        profile_, base::BindRepeating(&PrivetNotificationService::StartLister,
                                      AsWeakPtr()));
  } else {
    device_lister_.reset();
    service_discovery_client_ = nullptr;
    privet_notifications_listener_.reset();
  }
#else
  if (IsForced() || *enable_privet_notification_member_) {
    StartLister();
  } else {
    device_lister_.reset();
    service_discovery_client_ = nullptr;
    privet_notifications_listener_.reset();
  }
#endif
}

void PrivetNotificationService::StartLister() {
  service_discovery_client_ =
      local_discovery::ServiceDiscoverySharedClient::GetInstance();
  device_lister_.reset(
      new PrivetDeviceListerImpl(service_discovery_client_.get(), this));
  device_lister_->Start();
  device_lister_->DiscoverNewDevices();

  std::unique_ptr<PrivetHTTPAsynchronousFactory> http_factory(
      PrivetHTTPAsynchronousFactory::CreateInstance(
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess()));

  privet_notifications_listener_.reset(
      new PrivetNotificationsListener(std::move(http_factory), this));
}

PrivetNotificationDelegate*
PrivetNotificationService::CreateNotificationDelegate(Profile* profile) {
  return new PrivetNotificationDelegate(profile);
}

PrivetNotificationDelegate::PrivetNotificationDelegate(Profile* profile)
    : profile_(profile) {}

PrivetNotificationDelegate::~PrivetNotificationDelegate() {
}

void PrivetNotificationDelegate::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (!button_index)
    return;

  if (*button_index == 0) {
    OpenTab(GURL(kPrivetNotificationOriginUrl));
  } else {
    DCHECK_EQ(1, *button_index);
    DisableNotifications();
  }
  CloseNotification();
}

void PrivetNotificationDelegate::OpenTab(const GURL& url) {
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void PrivetNotificationDelegate::DisableNotifications() {
  profile_->GetPrefs()->SetBoolean(prefs::kLocalDiscoveryNotificationsEnabled,
                                   false);
}

void PrivetNotificationDelegate::CloseNotification() {
  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kPrivetNotificationID);
}

}  // namespace cloud_print
