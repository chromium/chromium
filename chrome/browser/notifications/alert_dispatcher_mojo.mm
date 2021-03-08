// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_mojo.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#import "chrome/browser/notifications/notification_alert_service_bridge.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

@implementation AlertDispatcherMojo {
  std::unique_ptr<MacNotificationProviderFactory> _providerFactory;
  base::scoped_nsobject<NotificationAlertServiceBridge> _mojoService;
  SEQUENCE_CHECKER(_sequenceChecker);
  base::CancelableOnceClosure _noAlertsChecker;
  base::TimeTicks _serviceStartTime;
}

- (instancetype)initWithProviderFactory:
    (std::unique_ptr<MacNotificationProviderFactory>)providerFactory {
  if ((self = [super init])) {
    _providerFactory = std::move(providerFactory);
  }
  return self;
}

- (void)dispatchNotification:(NSDictionary*)data {
  [[self serviceProxy] deliverNotification:data];
  // We know that there will be at least one notification after this.
  _noAlertsChecker.Cancel();
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  [[self serviceProxy] closeNotificationWithId:notificationId
                                     profileId:profileId
                                     incognito:incognito];
  // Check if there are any alerts left after this.
  [self checkIfAlertsRemaining];
}

- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito {
  [[self serviceProxy] closeNotificationsWithProfileId:profileId
                                             incognito:incognito];
  // Check if there are any alerts left after this.
  [self checkIfAlertsRemaining];
}

- (void)closeAllNotifications {
  [[self serviceProxy] closeAllNotifications];
  // We know that there are no more notifications after this.
  [self onServiceDisconnectedGracefully:YES];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                              callback:
                                  (GetDisplayedNotificationsCallback)callback {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetDisplayedNotificationsCallback blockCallback = std::move(callback);
  auto reply = ^(NSArray* alerts) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
    std::set<std::string> alertIds;

    for (NSString* alert in alerts)
      alertIds.insert(base::SysNSStringToUTF8(alert));

    // Check if there are any alerts left after this.
    if (![alerts count])
      [self checkIfAlertsRemaining];

    std::move(blockCallback)
        .Run(std::move(alertIds),
             /*supports_synchronization=*/true);
  };

  [[self serviceProxy] getDisplayedAlertsForProfileId:profileId
                                            incognito:incognito
                                                reply:reply];
}

- (void)getAllDisplayedAlertsWithCallback:
    (GetAllDisplayedNotificationsCallback)callback {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetAllDisplayedNotificationsCallback blockCallback =
      std::move(callback);
  auto reply = ^(NSArray* alerts) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
    std::vector<MacNotificationIdentifier> alertIdentifiers;
    alertIdentifiers.reserve([alerts count]);

    for (NSDictionary* alert in alerts) {
      NSString* notificationId =
          [alert objectForKey:notification_constants::kNotificationId];
      NSString* profileId =
          [alert objectForKey:notification_constants::kNotificationProfileId];
      bool incognito =
          [[alert objectForKey:notification_constants::kNotificationIncognito]
              boolValue];

      alertIdentifiers.push_back({base::SysNSStringToUTF8(notificationId),
                                  base::SysNSStringToUTF8(profileId),
                                  incognito});
    }

    // Check if there are any alerts left after this. We need to do another call
    // here as we might have shown another notification in between.
    if (![alerts count])
      [self checkIfAlertsRemaining];

    // Initialize the base::flat_set via a std::vector to avoid N^2 runtime.
    base::flat_set<MacNotificationIdentifier> identifiers(
        std::move(alertIdentifiers));
    std::move(blockCallback).Run(std::move(identifiers));
  };

  [[self serviceProxy] getAllDisplayedAlertsWithReply:reply];
}

- (void)checkIfAlertsRemaining {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Create a new cancelable callback that closes the mojo connection.
  _noAlertsChecker.Reset(base::BindOnce(base::RetainBlock(^{
    [self onServiceDisconnectedGracefully:YES];
  })));
  __block base::OnceClosure blockCallback = _noAlertsChecker.callback();

  // This block will be called with all displayed notifications. If there are
  // none left we close the mojo connection (only if the callback has not been
  // canceled yet).
  // TODO(crbug.com/1127306): Revisit this for the UNNotification API as we need
  // to keep the process running during the initial permission request.
  auto reply = ^(NSArray* alerts) {
    if (![alerts count])
      std::move(blockCallback).Run();
  };
  [[self serviceProxy] getAllDisplayedAlertsWithReply:reply];
}

- (void)onServiceDisconnectedGracefully:(BOOL)gracefully {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_mojoService) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - _serviceStartTime;
    base::UmaHistogramCustomTimes(
        "Notifications.macOS.ServiceProcessRuntime", elapsed,
        base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromHours(8),
        /*buckets=*/50);
    if (!gracefully) {
      base::UmaHistogramCustomTimes(
          "Notifications.macOS.ServiceProcessKilled", elapsed,
          base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromHours(8),
          /*buckets=*/50);
    }
  }
  _noAlertsChecker.Cancel();
  _mojoService.reset();
}

- (id<NotificationDelivery>)serviceProxy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_mojoService) {
    _serviceStartTime = base::TimeTicks::Now();
    auto onDisconnect = base::BindOnce(base::RetainBlock(^{
      [self onServiceDisconnectedGracefully:NO];
    }));
    auto onAction = base::BindRepeating(base::RetainBlock(^{
      [self checkIfAlertsRemaining];
    }));
    _mojoService.reset([[NotificationAlertServiceBridge alloc]
        initWithDisconnectHandler:std::move(onDisconnect)
                    actionHandler:std::move(onAction)
                         provider:_providerFactory->LaunchProvider(
                                      /*in_process=*/false)]);
  }
  return _mojoService.get();
}

@end
