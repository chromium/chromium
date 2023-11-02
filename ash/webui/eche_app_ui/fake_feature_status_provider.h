// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_

#include "ash/webui/eche_app_ui/feature_status_provider.h"

namespace ash {
namespace eche_app {

class FakeFeatureStatusProvider : public FeatureStatusProvider {
 public:
  // Defaults initial status to kConnected.
  FakeFeatureStatusProvider();
  explicit FakeFeatureStatusProvider(FeatureStatus initial_status);
  ~FakeFeatureStatusProvider() override;

  void SetStatus(FeatureStatus status);

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  FeatureStatus status_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_FEATURE_STATUS_PROVIDER_H_
