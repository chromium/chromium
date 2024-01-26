// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/logging.h"

namespace ash {

// This stub source file is only built on platforms that don't support the
// ambient animation resources. 2 things are required currently for support:
// 1) An internal chrome-branded checkout.
// 2) The include_ash_ambient_animation_resources GN flag must be true.
// Without these, the ambient animation code will still compile due to the
// dummy definitions in this stub, but any resource loading will immediately
// fail with a FATAL error.

// static
std::unique_ptr<AmbientAnimationStaticResources>
AmbientAnimationStaticResources::Create(AmbientUiSettings ui_settings,
                                        bool serializable) {
  if (ui_settings.theme() ==
      personalization_app::mojom::AmbientTheme::kSlideshow) {
    return nullptr;
  }

  LOG(FATAL) << "Ambient animation resources are not available on this build. "
                "To enable, an internal chrome-branded checkout is required, "
                "and the include_ash_ambient_animation_resources GN flag must "
                "be true.";
}

}  // namespace ash
