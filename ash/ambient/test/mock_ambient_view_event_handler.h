// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_MOCK_AMBIENT_VIEW_EVENT_HANDLER_H_
#define ASH_AMBIENT_TEST_MOCK_AMBIENT_VIEW_EVENT_HANDLER_H_

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAmbientViewEventHandler : public AmbientViewEventHandler {
 public:
  MockAmbientViewEventHandler();
  MockAmbientViewEventHandler(const MockAmbientViewEventHandler&) = delete;
  MockAmbientViewEventHandler& operator=(const MockAmbientViewEventHandler&) =
      delete;
  ~MockAmbientViewEventHandler() override;

  MOCK_METHOD(void,
              OnMarkerHit,
              (AmbientPhotoConfig::Marker marker),
              (override));
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_MOCK_AMBIENT_VIEW_EVENT_HANDLER_H_
