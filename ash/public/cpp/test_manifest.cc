// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test_manifest.h"

#include "ash/public/mojom/status_area_widget_test_api.test-mojom.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace ash {

const service_manager::Manifest& GetManifestOverlayForTesting() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability("test", service_manager::Manifest::InterfaceList<
                                        mojom::StatusAreaWidgetTestApi>())
          .Build()};
  return *manifest;
}

}  // namespace ash
