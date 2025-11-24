// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;

class GlicActorAutofillDisabledUiTest : public GlicActorUiTest {
 public:
  GlicActorAutofillDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicActorAutofill);
  }
  ~GlicActorAutofillDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that that actions are rejected when the kGlicActorAutofill feature is
// disabled and a form filling action was included.
IN_PROC_BROWSER_TEST_F(GlicActorAutofillDisabledUiTest,
                       AttemptFormFillingActionDisabledByFlag) {
  auto action_provider = base::BindLambdaForTesting([&]() {
    apc::Actions actions_proto;
    actions_proto.add_actions()->mutable_attempt_form_filling();
    actions_proto.set_task_id(task_id_.value());
    return EncodeActionProto(actions_proto);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(), CreateTask(task_id_, /*title=*/""),
      WaitUntil([this]() { return this->task_id_ ? "true" : "false"; }, "true",
                "task_id_ set"),
      ExecuteAction(std::move(action_provider),
                    actor::mojom::ActionResultCode::kArgumentsInvalid));
}

}  // namespace

}  // namespace glic::test
