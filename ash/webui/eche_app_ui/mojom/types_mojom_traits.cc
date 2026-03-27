// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/mojom/types_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
ash::eche_app::mojom::ScreenBacklightState EnumTraits<
    ash::eche_app::mojom::ScreenBacklightState,
    ash::ScreenBacklightState>::ToMojom(ash::ScreenBacklightState input) {
  switch (input) {
    case ash::ScreenBacklightState::ON:
      return ash::eche_app::mojom::ScreenBacklightState::ON;
    case ash::ScreenBacklightState::OFF:
      return ash::eche_app::mojom::ScreenBacklightState::OFF;
    case ash::ScreenBacklightState::OFF_AUTO:
      return ash::eche_app::mojom::ScreenBacklightState::OFF_AUTO;
  }

  NOTREACHED();
}

// static
ash::ScreenBacklightState EnumTraits<ash::eche_app::mojom::ScreenBacklightState,
                                     ash::ScreenBacklightState>::
    FromMojom(ash::eche_app::mojom::ScreenBacklightState input) {
  switch (input) {
    case ash::eche_app::mojom::ScreenBacklightState::ON:
      return ash::ScreenBacklightState::ON;
    case ash::eche_app::mojom::ScreenBacklightState::OFF:
      return ash::ScreenBacklightState::OFF;
    case ash::eche_app::mojom::ScreenBacklightState::OFF_AUTO:
      return ash::ScreenBacklightState::OFF_AUTO;
  }
  NOTREACHED();
}

}  // namespace mojo
