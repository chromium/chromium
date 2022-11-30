// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"

#include "chrome/browser/ash/power/auto_screen_brightness/fake_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

class AlsReaderTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    als_reader_ = std::make_unique<AlsReader>();
    als_reader_->AddObserver(&fake_observer_);
  }

  void SetLux(int lux) { als_reader_->SetLux(lux); }
  void SetAlsInitStatus(AlsReader::AlsInitStatus status) {
    als_reader_->SetAlsInitStatus(status);
  }
  void SetAlsInitStatusForTesting(AlsReader::AlsInitStatus status) {
    als_reader_->SetAlsInitStatusForTesting(status);
  }

  FakeObserver fake_observer_;
  FakeObserver fake_observer2_;
  std::unique_ptr<AlsReader> als_reader_;
};

TEST_F(AlsReaderTest, LuxUpdated) {
  SetLux(100);
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 1);
  EXPECT_EQ(fake_observer_.ambient_light(), 100);

  als_reader_->AddObserver(&fake_observer2_);

  SetLux(200);
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 2);
  EXPECT_EQ(fake_observer_.ambient_light(), 200);
  EXPECT_EQ(fake_observer2_.num_received_ambient_lights(), 1);
  EXPECT_EQ(fake_observer2_.ambient_light(), 200);

  als_reader_->RemoveObserver(&fake_observer_);

  SetLux(150);
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 2);
  EXPECT_EQ(fake_observer_.ambient_light(), 200);
  EXPECT_EQ(fake_observer2_.num_received_ambient_lights(), 2);
  EXPECT_EQ(fake_observer2_.ambient_light(), 150);
}

TEST_F(AlsReaderTest, StatusUpdated) {
  SetAlsInitStatus(AlsReader::AlsInitStatus::kDisabled);
  EXPECT_EQ(fake_observer_.status(), AlsReader::AlsInitStatus::kDisabled);

  SetAlsInitStatusForTesting(AlsReader::AlsInitStatus::kMissingPath);

  als_reader_->AddObserver(&fake_observer2_);

  EXPECT_EQ(fake_observer_.status(), AlsReader::AlsInitStatus::kDisabled);
  EXPECT_EQ(fake_observer2_.status(), AlsReader::AlsInitStatus::kMissingPath);

  SetAlsInitStatus(AlsReader::AlsInitStatus::kIncorrectConfig);
  EXPECT_EQ(fake_observer_.status(),
            AlsReader::AlsInitStatus::kIncorrectConfig);
  EXPECT_EQ(fake_observer2_.status(),
            AlsReader::AlsInitStatus::kIncorrectConfig);

  als_reader_->RemoveObserver(&fake_observer_);

  SetAlsInitStatus(AlsReader::AlsInitStatus::kSuccess);
  EXPECT_EQ(fake_observer_.status(),
            AlsReader::AlsInitStatus::kIncorrectConfig);
  EXPECT_EQ(fake_observer2_.status(), AlsReader::AlsInitStatus::kSuccess);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
