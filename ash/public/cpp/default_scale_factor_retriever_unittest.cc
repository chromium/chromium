// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/default_scale_factor_retriever.h"

#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace ash {

namespace {

class TestCrosDisplayConfig : public ash::mojom::CrosDisplayConfigController {
 public:
  static constexpr int64_t kFakeDisplayId = 1;

  TestCrosDisplayConfig() = default;

  mojo::PendingRemote<ash::mojom::CrosDisplayConfigController>
  CreateRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // ash::mojom::CrosDisplayConfigController:
  void AddObserver(
      mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver>
          observer) override {}
  void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) override {}
  void SetDisplayLayoutInfo(ash::mojom::DisplayLayoutInfoPtr info,
                            SetDisplayLayoutInfoCallback callback) override {}
  void GetDisplayUnitInfoList(
      bool single_unified,
      GetDisplayUnitInfoListCallback callback) override {
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list;
    auto info = ash::mojom::DisplayUnitInfo::New();
    info->id = kFakeDisplayId;
    info->is_internal = true;
    auto mode = ash::mojom::DisplayMode::New();
    mode->device_scale_factor = 2.f;
    info->available_display_modes.emplace_back(std::move(mode));
    info_list.push_back(std::move(info));
    std::move(callback).Run(std::move(info_list));
  }
  void SetDisplayProperties(const std::string& id,
                            ash::mojom::DisplayConfigPropertiesPtr properties,
                            ash::mojom::DisplayConfigSource source,
                            SetDisplayPropertiesCallback callback) override {}
  void SetUnifiedDesktopEnabled(bool enabled) override {}
  void OverscanCalibration(const std::string& display_id,
                           ash::mojom::DisplayConfigOperation op,
                           const base::Optional<gfx::Insets>& delta,
                           OverscanCalibrationCallback callback) override {}
  void TouchCalibration(const std::string& display_id,
                        ash::mojom::DisplayConfigOperation op,
                        ash::mojom::TouchCalibrationPtr calibration,
                        TouchCalibrationCallback callback) override {}

 private:
  mojo::Receiver<ash::mojom::CrosDisplayConfigController> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestCrosDisplayConfig);
};

class DefaultScaleFactorRetrieverTest : public testing::Test {
 public:
  DefaultScaleFactorRetrieverTest() = default;
  ~DefaultScaleFactorRetrieverTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(DefaultScaleFactorRetrieverTest);
};

}  // namespace

TEST_F(DefaultScaleFactorRetrieverTest, Basic) {
  display::Display::SetInternalDisplayId(TestCrosDisplayConfig::kFakeDisplayId);
  auto display_config = std::make_unique<TestCrosDisplayConfig>();
  auto retriever = std::make_unique<ash::DefaultScaleFactorRetriever>();

  auto callback = [](float* result, float default_scale_factor) {
    result[0] = default_scale_factor;
  };
  float result1[1] = {0};
  retriever->Start(display_config->CreateRemoteAndBind());
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result1));
  float result2[1] = {0};
  // This will cancel the 1st callback.
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result2));

  EXPECT_EQ(0.f, result1[0]);
  EXPECT_EQ(0.f, result2[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0.f, result1[0]);
  EXPECT_EQ(2.f, result2[0]);

  float result3[1] = {0};
  // This time, it should use the cached value.
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result3));
  EXPECT_EQ(2.f, result3[0]);

  // For test.
  retriever->SetDefaultScaleFactorForTest(3.0f);
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result3));
  EXPECT_EQ(3.f, result3[0]);

  display::Display::SetInternalDisplayId(display::kInvalidDisplayId);
}

TEST_F(DefaultScaleFactorRetrieverTest, Cancel) {
  display::Display::SetInternalDisplayId(TestCrosDisplayConfig::kFakeDisplayId);
  auto display_config = std::make_unique<TestCrosDisplayConfig>();
  auto retriever = std::make_unique<ash::DefaultScaleFactorRetriever>();

  auto callback = [](float* result, float default_scale_factor) {
    result[0] = default_scale_factor;
  };
  float result[1] = {0};
  retriever->Start(display_config->CreateRemoteAndBind());
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result));
  retriever->CancelCallback();
  EXPECT_EQ(0.f, result[0]);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0.f, result[0]);

  float result2[1] = {0};
  // This time, it should use the cached value.
  retriever->GetDefaultScaleFactor(base::BindOnce(callback, result2));

  EXPECT_EQ(2.f, result2[0]);
  display::Display::SetInternalDisplayId(display::kInvalidDisplayId);
}

}  // namespace ash
