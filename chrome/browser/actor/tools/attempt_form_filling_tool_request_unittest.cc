// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

namespace {

class AttemptFormFillingToolRequestTest : public testing::Test {
 public:
  AttemptFormFillingToolRequestTest() = default;
  ~AttemptFormFillingToolRequestTest() override = default;

  // Helper to create a dummy tab handle.
  tabs::TabHandle GetDummyTabHandle() { return tabs::TabHandle(1); }
};

TEST_F(AttemptFormFillingToolRequestTest, NameReturnsCorrectString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  // The string returned by Name() is used in histograms.xml to define the
  // ToolRequest variants for metrics collection.
  EXPECT_EQ(tool_request.Name(), "AttemptFormFilling");
}

TEST_F(AttemptFormFillingToolRequestTest, JournalEventString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  EXPECT_EQ("Attempt form filling", tool_request.JournalEvent());
}

}  // namespace
}  // namespace actor
