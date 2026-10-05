// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

using MultiStep = GlicActorUiTest::MultiStep;

class GlicActorMediaControlToolUiTest : public GlicActorUiTest {
 public:
  MultiStep PlayMediaAction(ExpectedErrorResult expected_result = {});
  MultiStep PauseMediaAction(ExpectedErrorResult expected_result = {});
  MultiStep SeekMediaAction(base::TimeDelta seek_time,
                            ExpectedErrorResult expected_result = {});
};

MultiStep GlicActorMediaControlToolUiTest::PlayMediaAction(
    ExpectedErrorResult expected_result) {
  auto provider = base::BindLambdaForTesting([this]() {
    optimization_guide::proto::Actions action =
        actor::MakePlayMedia(tab_handle_, task_id_);
    return EncodeActionProto(action);
  });
  return ExecuteAction(std::move(provider), std::move(expected_result));
}

MultiStep GlicActorMediaControlToolUiTest::PauseMediaAction(
    ExpectedErrorResult expected_result) {
  auto provider = base::BindLambdaForTesting([this]() {
    optimization_guide::proto::Actions action =
        actor::MakePauseMedia(tab_handle_, task_id_);
    return EncodeActionProto(action);
  });
  return ExecuteAction(std::move(provider), std::move(expected_result));
}

MultiStep GlicActorMediaControlToolUiTest::SeekMediaAction(
    base::TimeDelta seek_time,
    ExpectedErrorResult expected_result) {
  auto provider = base::BindLambdaForTesting([this, seek_time]() {
    optimization_guide::proto::Actions action =
        actor::MakeSeekMedia(tab_handle_, seek_time, task_id_);
    return EncodeActionProto(action);
  });
  return ExecuteAction(std::move(provider), std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, NoMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      PauseMediaAction(actor::mojom::ActionResultCode::kMediaControlNoMedia));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, PauseAndPlayMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/media.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      ExecuteJs(kNewActorTabId, "play"),
      WaitForJsResult(kNewActorTabId, "() => waitForEvent('play')"),
      PauseMediaAction(),
      WaitForJsResult(kNewActorTabId, "() => waitForEvent('pause')"),
      PlayMediaAction(),
      WaitForJsResult(kNewActorTabId, "() => waitForEvent('play')"));
}

IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, SeekMedia) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/media.html");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(url, kNewActorTabId),
      ExecuteJs(kNewActorTabId, "play"),
      WaitForJsResult(kNewActorTabId, "() => waitForEvent('play')"),
      ExecuteJs(kNewActorTabId, "() => { video.pause(); }"),
      WaitForJsResult(kNewActorTabId, "() => waitForEvent('pause')"),
      SeekMediaAction(base::Milliseconds(1000)),
      WaitForJsResult(kNewActorTabId, "() => waitForSeek(1.0)"));
}

}  //  namespace

}  // namespace glic::test
