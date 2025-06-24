// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_

#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/mojom/browser_controls_offset_tag_modifications.mojom-shared.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::BrowserControlsOffsetTagsDataView,
                    cc::BrowserControlsOffsetTags> {
  static viz::OffsetTag top_controls_offset_tag(
      const cc::BrowserControlsOffsetTags& tags) {
    return tags.top_controls_offset_tag;
  }

  static viz::OffsetTag content_offset_tag(
      const cc::BrowserControlsOffsetTags& tags) {
    return tags.content_offset_tag;
  }

  static viz::OffsetTag bottom_controls_offset_tag(
      const cc::BrowserControlsOffsetTags& tags) {
    return tags.bottom_controls_offset_tag;
  }

  static inline bool Read(cc::mojom::BrowserControlsOffsetTagsDataView data,
                          cc::BrowserControlsOffsetTags* out) {
    if (!data.ReadTopControlsOffsetTag(&out->top_controls_offset_tag) ||
        !data.ReadContentOffsetTag(&out->content_offset_tag) ||
        !data.ReadBottomControlsOffsetTag(&out->bottom_controls_offset_tag)) {
      return false;
    }
    return true;
  }
};

template <>
struct StructTraits<cc::mojom::BrowserControlsOffsetTagModificationsDataView,
                    cc::BrowserControlsOffsetTagModifications> {
  static const cc::BrowserControlsOffsetTags& tags(
      const cc::BrowserControlsOffsetTagModifications& modifications) {
    return modifications.tags;
  }

  static int top_controls_additional_height(
      const cc::BrowserControlsOffsetTagModifications& modifications) {
    return modifications.top_controls_additional_height;
  }

  static int bottom_controls_additional_height(
      const cc::BrowserControlsOffsetTagModifications& modifications) {
    return modifications.bottom_controls_additional_height;
  }

  static inline bool Read(
      cc::mojom::BrowserControlsOffsetTagModificationsDataView data,
      cc::BrowserControlsOffsetTagModifications* out) {
    if (!data.ReadTags(&out->tags)) {
      return false;
    }
    out->top_controls_additional_height = data.top_controls_additional_height();
    out->bottom_controls_additional_height =
        data.bottom_controls_additional_height();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_
