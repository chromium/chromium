// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_
#define ASH_WEBUI_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {
namespace network_diagnostics {

// Adds the strings and resource paths needed for network diagnostics elements
// to |html_source|.
void AddResources(content::WebUIDataSource* html_source);

}  // namespace network_diagnostics
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
namespace network_diagnostics {
using ::ash::network_diagnostics::AddResources;
}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // ASH_WEBUI_NETWORK_UI_NETWORK_DIAGNOSTICS_RESOURCE_PROVIDER_H_
