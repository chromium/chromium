// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_MOJOM_NETWORK_PROVIDER_MOJOM_TRAITS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_MOJOM_NETWORK_PROVIDER_MOJOM_TRAITS_H_

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

namespace diagnostics_mojom = ::ash::diagnostics::mojom;
namespace network_config_mojom = ::chromeos::network_config::mojom;

template <>
struct EnumTraits<diagnostics_mojom::SecurityType,
                  network_config_mojom::SecurityType> {
  static diagnostics_mojom::SecurityType ToMojom(
      network_config_mojom::SecurityType input);

  static bool FromMojom(diagnostics_mojom::SecurityType input,
                        network_config_mojom::SecurityType* output);
};

template <>
struct EnumTraits<diagnostics_mojom::AuthenticationType,
                  network_config_mojom::AuthenticationType> {
  static diagnostics_mojom::AuthenticationType ToMojom(
      network_config_mojom::AuthenticationType input);

  static bool FromMojom(diagnostics_mojom::AuthenticationType input,
                        network_config_mojom::AuthenticationType* output);
};

}  // namespace mojo
#endif  // ASH_WEBUI_DIAGNOSTICS_UI_MOJOM_NETWORK_PROVIDER_MOJOM_TRAITS_H_
