// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_APP_ARC_APP_LAUNCH_NOTIFIER_H_
#define ASH_COMPONENTS_ARC_APP_ARC_APP_LAUNCH_NOTIFIER_H_

#include <string>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {
class ArcBridgeService;

// ArcAppLaunchNotifier is an ARC service to notifying potential ARC app launch
// behavior.
class ArcAppLaunchNotifier : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when an app launch is requested.
    // It just a event from anywhere for the app launch attempt. The identifier
    // can be anything, so that it cannot be used to specify the app.
    virtual void OnArcAppLaunchRequested(std::string_view identifier) {}

    // Called when ArcAppLaunchNotifier destroying.
    virtual void OnArcAppLaunchNotifierDestroy() {}
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAppLaunchNotifier* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcAppLaunchNotifier* GetForBrowserContextForTesting(
      content::BrowserContext* context);
  static void EnsureFactoryBuilt();

  ArcAppLaunchNotifier(content::BrowserContext* context,
                       ArcBridgeService* bridge_service);
  ArcAppLaunchNotifier(const ArcAppLaunchNotifier&) = delete;
  ArcAppLaunchNotifier& operator=(const ArcAppLaunchNotifier&) = delete;
  ~ArcAppLaunchNotifier() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies potential ARC app launch behavior. It should be call before
  // each ARC app launch, in order to notify ARC ready for receive the app
  // launch request.
  void NotifyArcAppLaunchRequest(std::string_view identifier);

 private:
  base::ObserverList<Observer> observers_;
};

class ArcAppLaunchNotifierFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAppLaunchNotifier,
          ArcAppLaunchNotifierFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAppLaunchNotifierFactory";

  static ArcAppLaunchNotifierFactory* GetInstance();

 private:
  friend class base::NoDestructor<ArcAppLaunchNotifierFactory>;

  ArcAppLaunchNotifierFactory() = default;
  ~ArcAppLaunchNotifierFactory() override = default;
};
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_APP_ARC_APP_LAUNCH_NOTIFIER_H_
