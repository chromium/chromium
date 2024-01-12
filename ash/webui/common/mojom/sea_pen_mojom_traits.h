// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_MOJOM_SEA_PEN_MOJOM_TRAITS_H_
#define ASH_WEBUI_COMMON_MOJOM_SEA_PEN_MOJOM_TRAITS_H_

#include "ash/webui/common/mojom/sea_pen.mojom-shared.h"
#include "components/manta/manta_status.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::personalization_app::mojom::MantaStatusCode,
                  manta::MantaStatusCode> {
  using MojomMantaStatusCode =
      ::ash::personalization_app::mojom::MantaStatusCode;
  static MojomMantaStatusCode ToMojom(manta::MantaStatusCode input);
  static bool FromMojom(MojomMantaStatusCode input,
                        manta::MantaStatusCode* output);
};

}  // namespace mojo

#endif  // ASH_WEBUI_COMMON_MOJOM_SEA_PEN_MOJOM_TRAITS_H_
