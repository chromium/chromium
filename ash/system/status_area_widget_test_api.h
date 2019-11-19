// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_API_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_API_H_

#include <memory>

#include "ash/public/mojom/status_area_widget_test_api.test-mojom.h"
#include "ash/system/status_area_widget.h"
#include "base/macros.h"

namespace ash {

class StatusAreaWidgetTestApi : public mojom::StatusAreaWidgetTestApi {
 public:
  explicit StatusAreaWidgetTestApi(StatusAreaWidget* widget);
  ~StatusAreaWidgetTestApi() override;

  // Creates and binds an instance from a remote request (e.g. from chrome).
  static void BindRequest(mojom::StatusAreaWidgetTestApiRequest request);

  // mojom::StatusAreaWidgetTestApi:
  void TapSelectToSpeakTray(TapSelectToSpeakTrayCallback callback) override;

  void SetCollapseState(StatusAreaWidget::CollapseState collapse_state);

 private:
  StatusAreaWidget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidgetTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_API_H_
