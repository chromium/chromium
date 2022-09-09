// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_throttle.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/concierge_helper_service.h"
#include "chrome/browser/ash/crostini/throttle/crostini_active_window_throttle_observer.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace crostini {
namespace {

class DefaultDelegateImpl : public CrostiniThrottle::Delegate {
 public:
  explicit DefaultDelegateImpl(content::BrowserContext* context)
      : context_(context) {}

  DefaultDelegateImpl(const DefaultDelegateImpl&) = delete;
  DefaultDelegateImpl& operator=(const DefaultDelegateImpl&) = delete;

  ~DefaultDelegateImpl() override = default;

  void SetCpuRestriction(bool do_restrict) override {
    ash::ConciergeHelperService::GetForBrowserContext(context_)
        ->SetTerminaVmCpuRestriction(do_restrict);
  }

 private:
  content::BrowserContext* context_;
};

class CrostiniThrottleFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniThrottleFactory* GetInstance() {
    static base::NoDestructor<CrostiniThrottleFactory> instance;
    return instance.get();
  }

  static CrostiniThrottle* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<CrostiniThrottle*>(
        CrostiniThrottleFactory::GetInstance()->GetServiceForBrowserContext(
            context, true /* create */));
  }

  CrostiniThrottleFactory(const CrostiniThrottleFactory&) = delete;
  CrostiniThrottleFactory& operator=(const CrostiniThrottleFactory&) = delete;

 private:
  friend class base::NoDestructor<CrostiniThrottleFactory>;

  CrostiniThrottleFactory()
      : ProfileKeyedServiceFactory("CrostiniThrottleFactory") {}
  ~CrostiniThrottleFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new CrostiniThrottle(context);
  }
};

}  // namespace

// static
CrostiniThrottle* CrostiniThrottle::GetForBrowserContext(
    content::BrowserContext* context) {
  return CrostiniThrottleFactory::GetForBrowserContext(context);
}

CrostiniThrottle::CrostiniThrottle(content::BrowserContext* context)
    : ThrottleService(context),
      delegate_(std::make_unique<DefaultDelegateImpl>(context)) {
  AddObserver(std::make_unique<CrostiniActiveWindowThrottleObserver>());
  StartObservers();
}

CrostiniThrottle::~CrostiniThrottle() = default;

void CrostiniThrottle::Shutdown() {
  StopObservers();
}

void CrostiniThrottle::ThrottleInstance(bool should_throttle) {
  delegate_->SetCpuRestriction(should_throttle);
}

}  // namespace crostini
