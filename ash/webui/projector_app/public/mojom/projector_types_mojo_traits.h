// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PUBLIC_MOJOM_PROJECTOR_TYPES_MOJO_TRAITS_H_
#define ASH_WEBUI_PROJECTOR_APP_PUBLIC_MOJOM_PROJECTOR_TYPES_MOJO_TRAITS_H_

#include <string>
#include <vector>

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

using MojomNewScreencastPreconditionState =
    ash::projector::mojom::NewScreencastPreconditionState;
using MojomNewScreencastPreconditionReason =
    ash::projector::mojom::NewScreencastPreconditionReason;
using MojomNewScreencastPreconditioDataView =
    ash::projector::mojom::NewScreencastPreconditionDataView;

template <>
struct EnumTraits<MojomNewScreencastPreconditionState,
                  ash::NewScreencastPreconditionState> {
  static MojomNewScreencastPreconditionState ToMojom(
      ash::NewScreencastPreconditionState input);
  static bool FromMojom(MojomNewScreencastPreconditionState input,
                        ash::NewScreencastPreconditionState* out);
};

template <>
struct EnumTraits<MojomNewScreencastPreconditionReason,
                  ash::NewScreencastPreconditionReason> {
  static MojomNewScreencastPreconditionReason ToMojom(
      ash::NewScreencastPreconditionReason input);
  static bool FromMojom(MojomNewScreencastPreconditionReason input,
                        ash::NewScreencastPreconditionReason* out);
};

template <>
class StructTraits<MojomNewScreencastPreconditioDataView,
                   ash::NewScreencastPrecondition> {
 public:
  static ash::NewScreencastPreconditionState state(
      const ash::NewScreencastPrecondition& r);
  static std::vector<ash::NewScreencastPreconditionReason> reasons(
      const ash::NewScreencastPrecondition& r);

  static bool Read(MojomNewScreencastPreconditioDataView data,
                   ash::NewScreencastPrecondition* out);
};

}  // namespace mojo

#endif  // ASH_WEBUI_PROJECTOR_APP_PUBLIC_MOJOM_PROJECTOR_TYPES_MOJO_TRAITS_H_
