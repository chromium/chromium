// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_controller.h"

#include "ash/annotator/annotator_metrics.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kAnnotatorMarkerColorHistogramName[] =
    "Ash.Projector.MarkerColor.ClamshellMode";

}  // namespace

class AnnotatorControllerTest : public AshTestBase {
 public:
  AnnotatorControllerTest() = default;

  AnnotatorControllerTest(const AnnotatorControllerTest&) = delete;
  AnnotatorControllerTest& operator=(const AnnotatorControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto* annotator_controller = Shell::Get()->annotator_controller();
    annotator_controller->SetToolClient(&client_);
  }

  AnnotatorController* annotator_controller() {
    return Shell::Get()->annotator_controller();
  }

 protected:
  MockAnnotatorClient client_;
};

TEST_F(AnnotatorControllerTest, SetAnnotatorTool) {
  base::HistogramTester histogram_tester;
  AnnotatorTool tool;
  tool.color = kProjectorDefaultPenColor;
  EXPECT_CALL(client_, SetTool(tool));

  annotator_controller()->SetAnnotatorTool(tool);
  histogram_tester.ExpectUniqueSample(kAnnotatorMarkerColorHistogramName,
                                      AnnotatorMarkerColor::kMagenta,
                                      /*count=*/1);
}

TEST_F(AnnotatorControllerTest, ResetTools) {
  EXPECT_CALL(client_, Clear());

  annotator_controller()->ResetTools();
}

}  // namespace ash
