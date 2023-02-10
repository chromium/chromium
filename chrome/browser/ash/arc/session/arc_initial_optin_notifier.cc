// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_initial_optin_notifier.h"

#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

class ArcInitialOptInNotifierFactory : public ProfileKeyedServiceFactory {
 public:
  ArcInitialOptInNotifierFactory();

  ArcInitialOptInNotifierFactory(const ArcInitialOptInNotifierFactory&) =
      delete;
  ArcInitialOptInNotifierFactory& operator=(
      const ArcInitialOptInNotifierFactory&) = delete;

  ~ArcInitialOptInNotifierFactory() override = default;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override {
    return new ArcInitialOptInNotifier(browser_context);
  }

  // static
  static ArcInitialOptInNotifier* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<ArcInitialOptInNotifier*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  // static
  static ArcInitialOptInNotifierFactory* GetInstance() {
    return base::Singleton<ArcInitialOptInNotifierFactory>::get();
  }
};

ArcInitialOptInNotifierFactory::ArcInitialOptInNotifierFactory()
    : ProfileKeyedServiceFactory("ArcInitialOptInNotifierFactory") {}

}  // anonymous namespace

// static
ArcInitialOptInNotifier* ArcInitialOptInNotifier::GetForProfile(
    Profile* profile) {
  return ArcInitialOptInNotifierFactory::GetForBrowserContext(profile);
}

ArcInitialOptInNotifier::ArcInitialOptInNotifier(
    content::BrowserContext* context) {
  ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcInitialOptInNotifier::~ArcInitialOptInNotifier() {
  ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

void ArcInitialOptInNotifier::OnArcInitialStart() {
  if (!IsArcPlayAutoInstallDisabled())
    return;

  LOG(WARNING) << "kArcDisablePlayAutoInstall flag is set. Force Arc apps "
                  "loaded metric.";
  ash::LoginUnlockThroughputRecorder* throughput_recorder =
      ash::Shell::HasInstance()
          ? ash::Shell::Get()->login_unlock_throughput_recorder()
          : nullptr;
  if (throughput_recorder) {
    throughput_recorder->OnArcAppListReady();
  }
}

void ArcInitialOptInNotifier::OnArcOptInUserAction() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ash::LoginUnlockThroughputRecorder* throughput_recorder =
      ash::Shell::HasInstance()
          ? ash::Shell::Get()->login_unlock_throughput_recorder()
          : nullptr;
  if (throughput_recorder)
    throughput_recorder->OnArcOptedIn();
}

// static
void ArcInitialOptInNotifier::EnsureFactoryBuilt() {
  ArcInitialOptInNotifierFactory::GetInstance();
}

}  // namespace arc
