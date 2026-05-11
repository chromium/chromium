// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_KEYED_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace glic {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  MockGlicKeyedService(content::BrowserContext* context,
                       signin::IdentityManager* identity_manager,
                       ProfileManager* profile_manager,
                       GlicProfileManager* glic_profile_manager,
                       ContextualCueingService* contextual_cueing_service,
                       actor::ActorKeyedService* actor_keyed_service);
  ~MockGlicKeyedService() override;

  MOCK_METHOD(base::WeakPtr<GlicInstance>,
              Invoke,
              (GlicInvokeOptions),
              (override));
  MOCK_METHOD(void, CloseFloatingPanel, (), (override));
  MOCK_METHOD(void,
              ToggleUI,
              (BrowserWindowInterface*, bool, mojom::InvocationSource),
              (override));
  MOCK_METHOD(GlicInstance*,
              GetInstanceForTab,
              (tabs::TabInterface*),
              (override));
  MOCK_METHOD(void,
              SendAdditionalContext,
              (tabs::TabHandle, mojom::AdditionalContextPtr),
              (override));
  MOCK_METHOD(bool,
              IsPanelShowingForBrowser,
              (const BrowserWindowInterface&),
              (const, override));
  MOCK_METHOD(base::WeakPtr<GlicInstance>,
              InvokeWithAutoSubmit,
              (InvokeWithAutoSubmitPasskey, GlicInvokeOptions),
              (override));
  MOCK_METHOD(base::WeakPtr<GlicInstance>,
              InvokeWithAutoSubmit,
              (InvokeWithAutoSubmitPasskey,
               GlicInvokeOptions,
               GlicInvokeWithAutoSubmitOptions),
              (override));

  bool IsWindowDetached() const override { return detached_; }
  void SetWindowDetached(bool detached) { detached_ = detached; }

  GlicInstanceCoordinator& instance_coordinator() const override {
    return const_cast<MockGlicInstanceCoordinator&>(mock_coordinator_);
  }
  MockGlicInstanceCoordinator& mock_coordinator() { return mock_coordinator_; }

 private:
  bool detached_ = false;
  MockGlicInstanceCoordinator mock_coordinator_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_KEYED_SERVICE_H_
