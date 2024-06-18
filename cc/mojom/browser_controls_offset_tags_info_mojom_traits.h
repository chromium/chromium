// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAGS_INFO_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAGS_INFO_MOJOM_TRAITS_H_

#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/mojom/browser_controls_offset_tags_info.mojom-shared.h"
#include "components/viz/common/quads/offset_tag.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::BrowserControlsOffsetTagsInfoDataView,
                    cc::BrowserControlsOffsetTagsInfo> {
  static const viz::OffsetTag& top_controls_offset_tag(
      const cc::BrowserControlsOffsetTagsInfo& input) {
    return input.top_controls_offset_tag;
  }

  static int top_controls_height(
      const cc::BrowserControlsOffsetTagsInfo& input) {
    return input.top_controls_height;
  }

  static bool Read(cc::mojom::BrowserControlsOffsetTagsInfoDataView data,
                   cc::BrowserControlsOffsetTagsInfo* out) {
    return data.ReadTopControlsOffsetTag(&out->top_controls_offset_tag);
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAGS_INFO_MOJOM_TRAITS_H_
