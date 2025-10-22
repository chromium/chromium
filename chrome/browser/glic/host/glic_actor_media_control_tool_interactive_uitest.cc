// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

using MediaControl = actor::MediaControl;
using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorMediaControlToolUiTest : public GlicActorUiTest {
 public:
  MultiStep MediaControlAction(MediaControl media_control,
                               ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorMediaControlToolUiTest::MediaControlAction(
    MediaControl media_control,
    ExpectedErrorResult expected_result) {
  auto media_control_provider =
      base::BindLambdaForTesting([this, media_control]() {
        optimization_guide::proto::Actions action =
            actor::MakeMediaControl(tab_handle_, media_control);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(media_control_provider),
                       std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, NoMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      MediaControlAction(actor::PauseMedia(),
                         actor::mojom::ActionResultCode::kMediaControlNoMedia));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, PauseAndPlayMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_test_server()->GetURL("/actor/media.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      ExecuteJs(kNewActorTabId, "play"),
      MediaControlAction(actor::PauseMedia()),
      WaitForJsResult(kNewActorTabId, "() => { return event_log.join(','); }",
                      "pause"),
      MediaControlAction(actor::PlayMedia()),
      WaitForJsResult(kNewActorTabId, "() => { return event_log.join(','); }",
                      "pause,play"));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, SeekMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_test_server()->GetURL("/actor/media.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      ExecuteJs(kNewActorTabId, "play"),
      MediaControlAction(actor::SeekMedia(1000000)),
      WaitForJsResult(kNewActorTabId, "() => { return event_log.join(','); }",
                      "seek 1"));
}

}  //  namespace

}  // namespace glic::test
