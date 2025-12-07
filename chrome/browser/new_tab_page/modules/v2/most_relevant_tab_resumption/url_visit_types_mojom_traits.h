// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_URL_VISIT_TYPES_MOJOM_TRAITS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_URL_VISIT_TYPES_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/url_visit_types.mojom.h"
#include "components/sync_device_info/device_info.h"

namespace mojo {

template <>
struct EnumTraits<ntp::most_relevant_tab_resumption::mojom::FormFactor,
                  syncer::DeviceInfo::FormFactor> {
  static ntp::most_relevant_tab_resumption::mojom::FormFactor ToMojom(
      syncer::DeviceInfo::FormFactor input) {
    static constexpr auto form_factor_map = base::MakeFixedFlatMap<
        syncer::DeviceInfo::FormFactor,
        ntp::most_relevant_tab_resumption::mojom::FormFactor>(
        {{syncer::DeviceInfo::FormFactor::kUnknown,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kUnknown},
         {syncer::DeviceInfo::FormFactor::kDesktop,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kDesktop},
         {syncer::DeviceInfo::FormFactor::kPhone,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kPhone},
         {syncer::DeviceInfo::FormFactor::kTablet,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kTablet},
         {syncer::DeviceInfo::FormFactor::kAutomotive,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kAutomotive},
         {syncer::DeviceInfo::FormFactor::kWearable,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kWearable},
         {syncer::DeviceInfo::FormFactor::kTv,
          ntp::most_relevant_tab_resumption::mojom::FormFactor::kTv}});
    return form_factor_map.at(input);
  }

  static bool FromMojom(
      ntp::most_relevant_tab_resumption::mojom::FormFactor input,
      syncer::DeviceInfo::FormFactor* out) {
    static constexpr auto form_factor_map = base::MakeFixedFlatMap<
        ntp::most_relevant_tab_resumption::mojom::FormFactor,
        syncer::DeviceInfo::FormFactor>(
        {{ntp::most_relevant_tab_resumption::mojom::FormFactor::kUnknown,
          syncer::DeviceInfo::FormFactor::kUnknown},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kDesktop,
          syncer::DeviceInfo::FormFactor::kDesktop},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kPhone,
          syncer::DeviceInfo::FormFactor::kPhone},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kTablet,
          syncer::DeviceInfo::FormFactor::kTablet},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kAutomotive,
          syncer::DeviceInfo::FormFactor::kAutomotive},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kWearable,
          syncer::DeviceInfo::FormFactor::kWearable},
         {ntp::most_relevant_tab_resumption::mojom::FormFactor::kTv,
          syncer::DeviceInfo::FormFactor::kTv}});
    *out = form_factor_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_MOST_RELEVANT_TAB_RESUMPTION_URL_VISIT_TYPES_MOJOM_TRAITS_H_
