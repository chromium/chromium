// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_MAGIC_BOOST_STATE_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_MAGIC_BOOST_STATE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A class that mocks `MagicBoostStateAsh` to use in tests.
class MockMagicBoostState : public MagicBoostStateAsh {
 public:
  MockMagicBoostState();

  MockMagicBoostState(const MockMagicBoostState&) = delete;
  MockMagicBoostState& operator=(const MockMagicBoostState&) = delete;

  ~MockMagicBoostState() override;

  // chromeos::MagicBoostState:
  MOCK_METHOD(void,
              ShouldIncludeOrcaInOptIn,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, EnableOrcaFeature, (), (override));
  MOCK_METHOD(void, DisableOrcaFeature, (), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_MAGIC_BOOST_STATE_H_
