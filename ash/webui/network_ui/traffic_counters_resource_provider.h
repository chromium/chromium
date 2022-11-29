// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_
#define ASH_WEBUI_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {
namespace traffic_counters {

// Adds the strings and resource paths needed for traffic counters elements
// to |html_source|.
void AddResources(content::WebUIDataSource* html_source);

}  // namespace traffic_counters
}  // namespace ash

#endif  // ASH_WEBUI_NETWORK_UI_TRAFFIC_COUNTERS_RESOURCE_PROVIDER_H_
