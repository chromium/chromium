// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_
#define ASH_WEBUI_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_

#include "ash/public/cpp/screen_backlight.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::eche_app::mojom::ScreenBacklightState,
                  ash::ScreenBacklightState> {
  static ash::eche_app::mojom::ScreenBacklightState ToMojom(
      ash::ScreenBacklightState input);
  static bool FromMojom(ash::eche_app::mojom::ScreenBacklightState input,
                        ash::ScreenBacklightState* output);
};

}  // namespace mojo

#endif  // ASH_WEBUI_ECHE_APP_UI_MOJOM_TYPES_MOJOM_TRAITS_H_
