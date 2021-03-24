// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MARKER_MARKER_CONTROLLER_TEST_API_H_
#define ASH_MARKER_MARKER_CONTROLLER_TEST_API_H_

namespace fast_ink {
class FastInkPoints;
}  // namespace fast_ink

namespace ash {

class MarkerController;

// An api for testing the MarkerController class.
class MarkerControllerTestApi {
 public:
  explicit MarkerControllerTestApi(MarkerController* instance);
  MarkerControllerTestApi(const MarkerControllerTestApi&) = delete;
  MarkerControllerTestApi& operator=(const MarkerControllerTestApi&) = delete;
  ~MarkerControllerTestApi();

  bool IsShowingMarker() const;

  const fast_ink::FastInkPoints& points() const;

 private:
  MarkerController* const instance_;
};

}  // namespace ash

#endif  // ASH_MARKER_MARKER_CONTROLLER_TEST_API_H_
