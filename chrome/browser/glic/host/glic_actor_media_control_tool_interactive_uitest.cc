// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "content/public/test/browser_test.h"

namespace glic::test {

namespace {

class GlicActorMediaControlToolUiTest : public GlicActorUiTest {};

// A placeholder test to ensure it is set up correctly.
IN_PROC_BROWSER_TEST_F(GlicActorMediaControlToolUiTest, PlaceholderTest) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(url, kNewActorTabId));
}

}  //  namespace

}  // namespace glic::test
