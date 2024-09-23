// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"

#include <cstddef>

#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/public/cpp/window_properties.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc::input_overlay {

std::unique_ptr<views::Widget> CreateArcWindow(
    aura::Window* root_window,
    const gfx::Rect& bounds,
    const std::string& package_name) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  params.context = root_window;
  // `chromeos::kAppTypeKey` property should be assigned before widget init.
  // It simulates the situation that
  // `AppServiceAppWindowShelfController::OnWindowInitialized()` is called
  // before `ArcInputOverlayManager::OnWindowInitialized()`;
  params.init_properties_container.SetProperty(chromeos::kAppTypeKey,
                                               chromeos::AppType::ARC_APP);
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->widget_delegate()->SetCanResize(true);
  widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, std::string("app_id"));
  widget->GetNativeWindow()->SetProperty(ash::kArcPackageNameKey, package_name);
  widget->Show();
  widget->Activate();

  return widget;
}

// Make sure the tasks run synchronously when creating the window.
std::unique_ptr<views::Widget> CreateArcWindowSyncAndWait(
    base::test::TaskEnvironment* task_environment,
    aura::Window* root_window,
    const gfx::Rect& bounds,
    const std::string& package_name) {
  task_environment->RunUntilIdle();
  auto window = CreateArcWindow(root_window, bounds, package_name);
  // I/O takes time here.
  task_environment->FastForwardBy(kIORead);
  return window;
}

void CheckActions(TouchInjector* injector,
                  size_t expect_size,
                  const std::vector<ActionType>& expect_types,
                  const std::vector<int>& expect_ids) {
  CHECK(injector) << "The touch injector should be not nullptr.";
  CHECK_EQ(expect_size, expect_types.size())
      << "Expected size for expect_types: " << expect_size;
  CHECK_EQ(expect_size, expect_ids.size())
      << "Expected size for expect_ids: " << expect_size;

  EXPECT_EQ(expect_size, injector->actions().size());
  for (size_t i = 0; i < expect_size; i++) {
    EXPECT_EQ(expect_types[i], injector->actions()[i]->GetType());
    EXPECT_EQ(expect_ids[i], injector->actions()[i]->id());
  }
}

void SimulatedAppInstalled(base::test::TaskEnvironment* task_environment,
                           ArcAppTest& arc_app_test,
                           const std::string& package_name,
                           bool is_gc_opt_out,
                           bool is_game) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->game_controls_opt_out = is_gc_opt_out;
  arc_app_test.AddPackage(package->Clone());

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc::mojom::AppInfo::New(package_name, package_name,
                                             package_name + ".activiy"))
      ->app_category = is_game ? arc::mojom::AppCategory::kGame
                               : arc::mojom::AppCategory::kProductivity;
  arc_app_test.app_instance()->SendPackageAppListRefreshed(package_name, apps);
  task_environment->RunUntilIdle();
}

std::u16string GetControlName(ActionType action_type,
                              std::u16string key_string) {
  int control_type_id = 0;
  switch (action_type) {
    case ActionType::TAP:
      control_type_id = IDS_INPUT_OVERLAY_BUTTON_TYPE_SINGLE_BUTTON_LABEL;
      break;
    case ActionType::MOVE:
      control_type_id = IDS_INPUT_OVERLAY_BUTTON_TYPE_JOYSTICK_BUTTON_LABEL;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (key_string.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_INPUT_OVERLAY_CONTROL_NAME_LABEL_UNASSIGNED_TEMPLATE,
        l10n_util::GetStringUTF16(control_type_id));
  }
  return l10n_util::GetStringFUTF16(
      IDS_INPUT_OVERLAY_CONTROL_NAME_LABEL_TEMPLATE,
      l10n_util::GetStringUTF16(control_type_id), key_string);
}

void VerifyEditingListFunctionTriggeredUkmEvent(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expected_entry_size,
    int64_t expect_histograms_value) {
  EXPECT_GE(expected_entry_size, 1u);
  const auto ukm_entries = ukm_recorder.GetEntriesByName(
      BuildGameControlsUkmEventName(kEditingListFunctionTriggeredHistogram));
  EXPECT_EQ(expected_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[expected_entry_size - 1u],
      ukm::builders::GameControls_EditingListFunctionTriggered::kFunctionName,
      expect_histograms_value);
}

void VerifyButtonOptionsMenuFunctionTriggeredUkmEvent(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expected_entry_size,
    size_t index,
    int64_t expect_histograms_value) {
  EXPECT_LT(index, expected_entry_size);
  const auto ukm_entries =
      ukm_recorder.GetEntriesByName(BuildGameControlsUkmEventName(
          kButtonOptionsMenuFunctionTriggeredHistogram));
  EXPECT_EQ(expected_entry_size, ukm_entries.size());
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      ukm_entries[index],
      ukm::builders::GameControls_ButtonOptionsMenuFunctionTriggered::
          kFunctionName,
      expect_histograms_value);
}

}  // namespace arc::input_overlay
