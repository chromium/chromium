// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/ppd_resolution_tracker.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/printing/ppd_resolution_state.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class PpdResolutionTrackerTest : public testing::Test {
 public:
  PpdResolutionTrackerTest() = default;
  ~PpdResolutionTrackerTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(PpdResolutionTrackerTest);
};

TEST_F(PpdResolutionTrackerTest, PendingResolution) {
  PpdResolutionTracker tracker;

  const std::string printer_id = "1";

  EXPECT_FALSE(tracker.IsResolutionComplete(printer_id));
  EXPECT_FALSE(tracker.IsResolutionPending(printer_id));

  tracker.MarkResolutionPending(printer_id);

  EXPECT_TRUE(tracker.IsResolutionPending(printer_id));
  EXPECT_FALSE(tracker.IsResolutionComplete(printer_id));
  EXPECT_FALSE(tracker.WasResolutionSuccessful(printer_id));
}

TEST_F(PpdResolutionTrackerTest, MarkPpdResolutionSuccessful) {
  PpdResolutionTracker tracker;

  const std::string printer_id = "1";
  tracker.MarkResolutionPending(printer_id);

  const std::string& expected_effective_make_and_model = "printer_make_model";
  Printer::PpdReference ref;
  ref.effective_make_and_model = expected_effective_make_and_model;

  tracker.MarkResolutionSuccessful(printer_id, ref);

  EXPECT_FALSE(tracker.IsResolutionPending(printer_id));
  EXPECT_TRUE(tracker.IsResolutionComplete(printer_id));
  EXPECT_TRUE(tracker.WasResolutionSuccessful(printer_id));

  EXPECT_TRUE(tracker.GetManufacturer(printer_id).empty());
  EXPECT_EQ(expected_effective_make_and_model,
            tracker.GetPpdReference(printer_id).effective_make_and_model);
}

TEST_F(PpdResolutionTrackerTest, MarkPpdResolutionFailed) {
  PpdResolutionTracker tracker;

  const std::string printer_id = "1";
  tracker.MarkResolutionPending(printer_id);
  tracker.MarkResolutionFailed(printer_id);

  EXPECT_FALSE(tracker.IsResolutionPending(printer_id));
  EXPECT_TRUE(tracker.IsResolutionComplete(printer_id));
  EXPECT_FALSE(tracker.WasResolutionSuccessful(printer_id));

  EXPECT_TRUE(tracker.GetManufacturer(printer_id).empty());
}

TEST_F(PpdResolutionTrackerTest, SetUsbManufacturer) {
  PpdResolutionTracker tracker;

  const std::string printer_id = "1";
  tracker.MarkResolutionPending(printer_id);
  tracker.MarkResolutionFailed(printer_id);

  const std::string expected_usb_manufacturer = "Hewlett-Packard";
  tracker.SetManufacturer(printer_id, expected_usb_manufacturer);

  EXPECT_FALSE(tracker.IsResolutionPending(printer_id));
  EXPECT_TRUE(tracker.IsResolutionComplete(printer_id));
  EXPECT_FALSE(tracker.WasResolutionSuccessful(printer_id));

  EXPECT_EQ(expected_usb_manufacturer, tracker.GetManufacturer(printer_id));
}

TEST_F(PpdResolutionTrackerTest, MultipleResolutions) {
  PpdResolutionTracker tracker;

  const std::string printer_id_1 = "1";
  tracker.MarkResolutionPending(printer_id_1);

  const std::string& expected_effective_make_and_model1 =
      "printer_make_model_1";
  Printer::PpdReference ref1;
  ref1.effective_make_and_model = expected_effective_make_and_model1;

  tracker.MarkResolutionSuccessful(printer_id_1, ref1);

  const std::string printer_id_2 = "2";
  tracker.MarkResolutionPending(printer_id_2);
  tracker.MarkResolutionFailed(printer_id_2);

  EXPECT_FALSE(tracker.IsResolutionPending(printer_id_2));
  EXPECT_TRUE(tracker.IsResolutionComplete(printer_id_2));
  EXPECT_FALSE(tracker.WasResolutionSuccessful(printer_id_2));

  EXPECT_TRUE(tracker.GetManufacturer(printer_id_2).empty());

  EXPECT_FALSE(tracker.IsResolutionPending(printer_id_1));
  EXPECT_TRUE(tracker.IsResolutionComplete(printer_id_1));
  EXPECT_TRUE(tracker.WasResolutionSuccessful(printer_id_1));

  EXPECT_TRUE(tracker.GetManufacturer(printer_id_1).empty());
  EXPECT_EQ(expected_effective_make_and_model1,
            tracker.GetPpdReference(printer_id_1).effective_make_and_model);
}

}  // namespace chromeos
