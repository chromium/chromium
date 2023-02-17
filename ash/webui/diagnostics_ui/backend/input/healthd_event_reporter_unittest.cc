// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/healthd_event_reporter.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_ash_event_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {
namespace {

constexpr uint32_t kFakeKeyboardId = 42;
constexpr uint32_t kFakeKeyCode = 42;
constexpr uint32_t kFakeTopRowPosition = 3;

class HealthdEventReporterTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  ash::cros_healthd::FakeAshEventReporter fake_event_repoter_service_;
  HealthdEventReporter event_reporter_;
};

TEST_F(HealthdEventReporterTest, ReportKeyboardEvent) {
  auto fake_keyboard_info = mojom::KeyboardInfo::New();

  auto expected_event = mojom::KeyboardDiagnosticEventInfo::New();
  expected_event->keyboard_info = fake_keyboard_info.Clone();

  event_reporter_.ReportKeyboardDiagnosticEvent(kFakeKeyboardId,
                                                fake_keyboard_info);
  EXPECT_EQ(fake_event_repoter_service_.WaitKeyboardDiagnosticEvent(),
            expected_event);
}

TEST_F(HealthdEventReporterTest, PressKey) {
  auto fake_keyboard_info = mojom::KeyboardInfo::New();
  auto fake_key_event = mojom::KeyEvent::New();
  fake_key_event->key_code = kFakeKeyCode;
  fake_key_event->top_row_position = -1;

  auto expected_event = mojom::KeyboardDiagnosticEventInfo::New();
  expected_event->keyboard_info = fake_keyboard_info.Clone();
  expected_event->tested_keys.push_back(kFakeKeyCode);

  event_reporter_.AddKeyEventForNextReport(kFakeKeyboardId, fake_key_event);
  event_reporter_.ReportKeyboardDiagnosticEvent(kFakeKeyboardId,
                                                fake_keyboard_info);
  EXPECT_EQ(fake_event_repoter_service_.WaitKeyboardDiagnosticEvent(),
            expected_event);
}

TEST_F(HealthdEventReporterTest, PressTopRowKey) {
  auto fake_keyboard_info = mojom::KeyboardInfo::New();
  auto fake_key_event = mojom::KeyEvent::New();
  fake_key_event->top_row_position = kFakeTopRowPosition;

  auto expected_event = mojom::KeyboardDiagnosticEventInfo::New();
  expected_event->keyboard_info = fake_keyboard_info.Clone();
  expected_event->tested_top_row_keys.push_back(kFakeTopRowPosition);

  event_reporter_.AddKeyEventForNextReport(kFakeKeyboardId, fake_key_event);
  event_reporter_.ReportKeyboardDiagnosticEvent(kFakeKeyboardId,
                                                fake_keyboard_info);
  EXPECT_EQ(fake_event_repoter_service_.WaitKeyboardDiagnosticEvent(),
            expected_event);
}

}  // namespace

}  // namespace ash::diagnostics
