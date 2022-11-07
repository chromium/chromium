// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_impl.h"
#include "chrome/browser/ash/borealis/borealis_engagement_metrics.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_power_controller.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/shell_surface_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace borealis {

namespace {

constexpr char kFeedbackNotificationId[] =
    "borealis-post-game-feedback-notification";
constexpr char kNotifierBorealis[] = "ash.borealis";

// Similar to a delayed callback, but can be cancelled by deleting.
class ScopedDelayedCallback {
 public:
  explicit ScopedDelayedCallback(base::OnceClosure callback,
                                 base::TimeDelta delay)
      : weak_factory_(this) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScopedDelayedCallback::OnComplete,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        delay);
  }

 private:
  void OnComplete(base::OnceClosure callback) { std::move(callback).Run(); }

  base::WeakPtrFactory<ScopedDelayedCallback> weak_factory_;
};

}  // namespace

class BorealisLifetimeObserver
    : public BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  explicit BorealisLifetimeObserver(BorealisContext* context)
      : context_(context), observation_(this), weak_factory_(this) {
    observation_.Observe(
        &BorealisService::GetForProfile(context_->profile())->WindowManager());
  }

  // BorealisWindowManager::AppWindowLifetimeObserver overrides.
  void OnSessionStarted() override {
    if (!context_->launch_options().auto_shutdown)
      return;
    BorealisService::GetForProfile(context_->profile())
        ->ShutdownMonitor()
        .CancelDelayedShutdown();
  }

  void OnSessionFinished() override {
    if (!context_->launch_options().auto_shutdown)
      return;
    BorealisService::GetForProfile(context_->profile())
        ->ShutdownMonitor()
        .ShutdownWithDelay();
  }

  void OnAppStarted(const std::string& app_id) override {
    app_delayers_.erase(app_id);
  }

  void OnAppFinished(const std::string& app_id,
                     aura::Window* last_window) override {
    // Launch post-game survey.
    if (!context_->launch_options().feedback_forms)
      return;
    FeedbackFormUrl(
        context_->profile(), app_id, base::UTF16ToUTF8(last_window->GetTitle()),
        base::BindOnce(&BorealisLifetimeObserver::OnFeedbackUrlGenerated,
                       weak_factory_.GetWeakPtr(), app_id));
  }

  void OnWindowManagerDeleted(BorealisWindowManager* window_manager) override {
    DCHECK(observation_.IsObservingSource(window_manager));
    observation_.Reset();
  }

 private:
  void OnFeedbackUrlGenerated(std::string app_id, GURL url) {
    if (url.is_valid()) {
      app_delayers_.emplace(
          app_id, std::make_unique<ScopedDelayedCallback>(
                      base::BindOnce(&BorealisLifetimeObserver::OnDelayComplete,
                                     weak_factory_.GetWeakPtr(), std::move(url),
                                     app_id),
                      base::Seconds(5)));
    }
  }

  void OnDelayComplete(GURL gurl, std::string app_id) {
    app_delayers_.erase(app_id);
    CreateFeedbackNotification(gurl);
  }

  // Creates a notification, that when clicked, will close itself and redirect
  // the user to a feedback form. If there is an existing notification, this
  // will replace it.
  void CreateFeedbackNotification(GURL gurl) {
    // Delegate for handling click events on the notification.
    auto on_click_handler =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(
                [](GURL gurl, Profile* profile) {
                  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
                      gurl,
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

                  NotificationDisplayService::GetForProfile(profile)->Close(
                      NotificationHandler::Type::TRANSIENT,
                      kFeedbackNotificationId);
                },
                gurl, context_->profile()));

    // Close the current notification (if any).
    NotificationDisplayService::GetForProfile(context_->profile())
        ->Close(NotificationHandler::Type::TRANSIENT, kFeedbackNotificationId);

    // Create the new notification.
    message_center::Notification notification(
        /*type=*/message_center::NOTIFICATION_TYPE_SIMPLE,
        /*id=*/kFeedbackNotificationId,
        /*title=*/
        l10n_util::GetStringUTF16(IDS_BOREALIS_FEEDBACK_NOTIFICATION_TITLE),
        /*message=*/
        l10n_util::GetStringUTF16(IDS_BOREALIS_FEEDBACK_NOTIFICATION_MESSAGE),
        /*icon=*/ui::ImageModel(),
        /*display_source=*/
        l10n_util::GetStringUTF16(IDS_BOREALIS_FEEDBACK_NOTIFICATION_SOURCE),
        /*origin_url=*/GURL(),
        /*notifier_id=*/
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kNotifierBorealis,
            ash::NotificationCatalogName::kBorealisContext),
        /*optional_fields=*/message_center::RichNotificationData(),
        /*delegate*/ on_click_handler);

    // Display the new notification.
    NotificationDisplayService::GetForProfile(context_->profile())
        ->Display(NotificationHandler::Type::TRANSIENT, notification,
                  /*metadata=*/nullptr);
  }

  BorealisContext* const context_;
  base::ScopedObservation<BorealisWindowManager,
                          BorealisWindowManager::AppWindowLifetimeObserver>
      observation_;
  base::flat_map<std::string, std::unique_ptr<ScopedDelayedCallback>>
      app_delayers_;

  base::WeakPtrFactory<BorealisLifetimeObserver> weak_factory_;
};

BorealisContext::~BorealisContext() = default;

void BorealisContext::SetDiskManagerForTesting(
    std::unique_ptr<BorealisDiskManager> disk_manager) {
  disk_manager_ = std::move(disk_manager);
}

void BorealisContext::NotifyUnexpectedVmShutdown() {
  guest_os_stability_monitor_->LogUnexpectedVmShutdown();
}

BorealisContext::BorealisContext(Profile* profile)
    : profile_(profile),
      lifetime_observer_(std::make_unique<BorealisLifetimeObserver>(this)),
      guest_os_stability_monitor_(
          std::make_unique<guest_os::GuestOsStabilityMonitor>(
              kBorealisStabilityHistogram)),
      engagement_metrics_(std::make_unique<BorealisEngagementMetrics>(profile)),
      disk_manager_(std::make_unique<BorealisDiskManagerImpl>(this)),
      power_controller_(std::make_unique<BorealisPowerController>(profile)) {}

std::unique_ptr<BorealisContext>
BorealisContext::CreateBorealisContextForTesting(Profile* profile) {
  // Construct out-of-place because the constructor is private.
  BorealisContext* ptr = new BorealisContext(profile);
  return base::WrapUnique(ptr);
}

}  // namespace borealis
