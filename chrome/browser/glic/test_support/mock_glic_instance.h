// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size.h"

namespace glic {

class MockGlicInstance : public GlicInstance {
 public:
  MockGlicInstance();
  ~MockGlicInstance() override;

  MOCK_METHOD(bool, IsActive, (), (override));
  MOCK_METHOD(void,
              AddStateObserver,
              (PanelStateObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveStateObserver,
              (PanelStateObserver * observer),
              (override));
  MOCK_METHOD(mojom::PanelState, GetPanelState, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterWillBeDestroyed,
              (DestructionCallback),
              (override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(gfx::Size, GetPanelSize, (), (override));
  MOCK_METHOD(Target, GetInvokeTarget, (Target::Surface), (override));
  MOCK_METHOD(const InstanceId&, id, (), (const, override));
  MOCK_METHOD(std::optional<std::string>,
              conversation_id,
              (),
              (const, override));
  MOCK_METHOD(std::string, conversation_title, (), (const, override));
  MOCK_METHOD(base::Time, GetLastActivationTimestamp, (), (const, override));
  MOCK_METHOD(base::TimeDelta, GetTimeSinceLastActive, (), (const, override));
  MOCK_METHOD(base::TimeDelta,
              GetTimeSinceLastPromptSubmission,
              (),
              (const, override));
  MOCK_METHOD(GlicActorTaskManager*, GetActorTaskManager, (), (override));
  MOCK_METHOD(bool, IsActuating, (), (const, override));
  MOCK_METHOD(void, CancelTask, (), (override));

  MOCK_METHOD(GlicSharingManager*, GetSharingManager, (), (override));

  MOCK_METHOD(void,
              GetExperimentalTriggeringUpdates,
              (mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler>,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(Host&, host, (), (override));
  MOCK_METHOD(void,
              SendAdditionalContext,
              (mojom::AdditionalContextPtr),
              (override));
  MOCK_METHOD(void, FocusIfActive, (), (override));
  MOCK_METHOD(void, NotifyActorTaskListRowClicked, (int32_t), (override));

  base::WeakPtr<MockGlicInstance> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockGlicInstance> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_H_
