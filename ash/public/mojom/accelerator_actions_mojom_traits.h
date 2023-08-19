// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_MOJOM_ACCELERATOR_ACTIONS_MOJOM_TRAITS_H_
#define ASH_PUBLIC_MOJOM_ACCELERATOR_ACTIONS_MOJOM_TRAITS_H_

#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/mojom/accelerator_actions.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::mojom::AcceleratorAction, ::ash::AcceleratorAction> {
  static ash::mojom::AcceleratorAction ToMojom(::ash::AcceleratorAction);

  static bool FromMojom(::ash::mojom::AcceleratorAction input,
                        ::ash::AcceleratorAction* out);
};

}  // namespace mojo

#endif  // ASH_PUBLIC_MOJOM_ACCELERATOR_ACTIONS_MOJOM_TRAITS_H_
