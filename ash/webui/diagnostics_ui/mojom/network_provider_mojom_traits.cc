// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/mojom/network_provider_mojom_traits.h"

#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/notreached.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {
namespace {
namespace diagnostics_mojom = ::ash::diagnostics::mojom;
namespace network_config_mojom = ::chromeos::network_config::mojom;
}  // namespace

// static
diagnostics_mojom::SecurityType EnumTraits<diagnostics_mojom::SecurityType,
                                           network_config_mojom::SecurityType>::
    ToMojom(network_config_mojom::SecurityType input) {
  switch (input) {
    case network_config_mojom::SecurityType::kNone:
      return diagnostics_mojom::SecurityType::kNone;
    case network_config_mojom::SecurityType::kWep8021x:
      return diagnostics_mojom::SecurityType::kWep8021x;
    case network_config_mojom::SecurityType::kWepPsk:
      return diagnostics_mojom::SecurityType::kWepPsk;
    case network_config_mojom::SecurityType::kWpaEap:
      return diagnostics_mojom::SecurityType::kWpaEap;
    case network_config_mojom::SecurityType::kWpaPsk:
      return diagnostics_mojom::SecurityType::kWpaPsk;
  }
  VLOG(1) << "Unknown security type: " << input;
  NOTREACHED();
}

// static
bool EnumTraits<diagnostics_mojom::SecurityType,
                network_config_mojom::SecurityType>::
    FromMojom(diagnostics_mojom::SecurityType input,
              network_config_mojom::SecurityType* output) {
  switch (input) {
    case diagnostics_mojom::SecurityType::kNone:
      *output = network_config_mojom::SecurityType::kNone;
      return true;
    case diagnostics_mojom::SecurityType::kWep8021x:
      *output = network_config_mojom::SecurityType::kWep8021x;
      return true;
    case diagnostics_mojom::SecurityType::kWepPsk:
      *output = network_config_mojom::SecurityType::kWepPsk;
      return true;
    case diagnostics_mojom::SecurityType::kWpaEap:
      *output = network_config_mojom::SecurityType::kWpaEap;
      return true;
    case diagnostics_mojom::SecurityType::kWpaPsk:
      *output = network_config_mojom::SecurityType::kWpaPsk;
      return true;
  }
  VLOG(1) << "Unknown security type: "
          << static_cast<network_config_mojom::SecurityType>(input);
  NOTREACHED();
}

// static
diagnostics_mojom::AuthenticationType
EnumTraits<diagnostics_mojom::AuthenticationType,
           network_config_mojom::AuthenticationType>::
    ToMojom(network_config_mojom::AuthenticationType input) {
  switch (input) {
    case network_config_mojom::AuthenticationType::kNone:
      return diagnostics_mojom::AuthenticationType::kNone;
    case network_config_mojom::AuthenticationType::k8021x:
      return diagnostics_mojom::AuthenticationType::k8021x;
  }
  VLOG(1) << "Unknown authentication type: " << input;
  NOTREACHED();
}

// static
bool EnumTraits<diagnostics_mojom::AuthenticationType,
                network_config_mojom::AuthenticationType>::
    FromMojom(diagnostics_mojom::AuthenticationType input,
              network_config_mojom::AuthenticationType* output) {
  switch (input) {
    case diagnostics_mojom::AuthenticationType::kNone:
      *output = network_config_mojom::AuthenticationType::kNone;
      return true;
    case diagnostics_mojom::AuthenticationType::k8021x:
      *output = network_config_mojom::AuthenticationType::k8021x;
      return true;
  }
  VLOG(1) << "Unknown authentication type: "
          << static_cast<network_config_mojom::AuthenticationType>(input);
  NOTREACHED();
}

}  // namespace mojo
