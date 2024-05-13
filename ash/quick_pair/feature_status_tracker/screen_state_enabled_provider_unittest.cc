// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"

namespace ash {
namespace quick_pair {

namespace {

constexpr gfx::Size kDisplaySize{1024, 768};

}  // namespace

class ScreenStateEnabledProviderTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    InitDisplaySnapshots();
    provider_ = std::make_unique<ScreenStateEnabledProvider>();
  }

  void InitDisplaySnapshots() {
    internal_on_snapshot_ =
        display::FakeDisplaySnapshot::Builder()
            .SetId(123u)
            .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetNativeMode(kDisplaySize)
            .SetCurrentMode(kDisplaySize)
            .Build();
    // Null current_mode() is the signal for a disconnected internal display.
    EXPECT_TRUE(internal_on_snapshot_->current_mode() != nullptr);

    internal_off_snapshot_ =
        display::FakeDisplaySnapshot::Builder()
            .SetId(123u)
            .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetNativeMode(kDisplaySize)
            .Build();
    EXPECT_FALSE(internal_off_snapshot_->current_mode() != nullptr);

    external_snapshot_ = display::FakeDisplaySnapshot::Builder()
                             .SetId(456u)
                             .SetType(display::DISPLAY_CONNECTION_TYPE_HDMI)
                             .SetNativeMode(kDisplaySize)
                             .AddMode(kDisplaySize)
                             .Build();
    external_snapshot_->set_current_mode(external_snapshot_->native_mode());
    EXPECT_TRUE(external_snapshot_->current_mode() != nullptr);
  }

  void UpdateDisplays(bool external_on, bool internal_on) {
    std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>> outputs;

    if (internal_on) {
      outputs.push_back(internal_on_snapshot_.get());
    } else {
      outputs.push_back(internal_off_snapshot_.get());
    }

    if (external_on) {
      outputs.push_back(external_snapshot_.get());
    }

    provider_->OnDisplayConfigurationChanged(outputs);
  }

  void TearDown() override {
    // Ensure our provider is destroyed before Ash TearDown
    provider_.reset();
    internal_on_snapshot_.reset();
    internal_off_snapshot_.reset();
    external_snapshot_.reset();

    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<display::DisplaySnapshot> internal_on_snapshot_;
  std::unique_ptr<display::DisplaySnapshot> internal_off_snapshot_;
  std::unique_ptr<display::DisplaySnapshot> external_snapshot_;
  std::unique_ptr<ScreenStateEnabledProvider> provider_;
};

TEST_F(ScreenStateEnabledProviderTest, IsInitiallyEnabled) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run).Times(0);
  provider_->SetCallback(callback.Get());

  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(ScreenStateEnabledProviderTest, ExternalOffInternalOff) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));
  provider_->SetCallback(callback.Get());

  UpdateDisplays(/*external_on=*/false, /*internal_on=*/false);
  EXPECT_FALSE(provider_->is_enabled());
}

TEST_F(ScreenStateEnabledProviderTest, ExternalOnInternalOff) {
  // Start with screens disabled.
  UpdateDisplays(/*external_on=*/false, /*internal_on=*/false);
  EXPECT_FALSE(provider_->is_enabled());
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  provider_->SetCallback(callback.Get());

  UpdateDisplays(/*external_on=*/true, /*internal_on=*/false);
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(ScreenStateEnabledProviderTest, ExternalOffInternalOn) {
  // Start with screens disabled.
  UpdateDisplays(/*external_on=*/false, /*internal_on=*/false);
  EXPECT_FALSE(provider_->is_enabled());
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  provider_->SetCallback(callback.Get());

  UpdateDisplays(/*external_on=*/false, /*internal_on=*/true);
  EXPECT_TRUE(provider_->is_enabled());
}

TEST_F(ScreenStateEnabledProviderTest, ExternalOnInternalOn) {
  // Start with screens disabled.
  UpdateDisplays(/*external_on=*/false, /*internal_on=*/false);
  EXPECT_FALSE(provider_->is_enabled());
  base::MockCallback<base::RepeatingCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  provider_->SetCallback(callback.Get());

  UpdateDisplays(/*external_on=*/true, /*internal_on=*/true);
  EXPECT_TRUE(provider_->is_enabled());
}

}  // namespace quick_pair
}  // namespace ash
