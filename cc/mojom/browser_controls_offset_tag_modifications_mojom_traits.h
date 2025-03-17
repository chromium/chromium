// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_
#define CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_

#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_offset_tags.h"
#include "cc/mojom/browser_controls_offset_tag_modifications.mojom-shared.h"
#include "components/viz/common/quads/offset_tag.h"
#include "services/viz/public/cpp/compositing/offset_tag_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::BrowserControlsOffsetTagsDataView,
                    cc::BrowserControlsOffsetTags> {
  static const viz::OffsetTag& top_controls_offset_tag(
      const cc::BrowserControlsOffsetTags& input) {
    return input.top_controls_offset_tag;
  }

  static const viz::OffsetTag& content_offset_tag(
      const cc::BrowserControlsOffsetTags& input) {
    return input.content_offset_tag;
  }

  static const viz::OffsetTag& bottom_controls_offset_tag(
      const cc::BrowserControlsOffsetTags& input) {
    return input.bottom_controls_offset_tag;
  }

  static bool Read(cc::mojom::BrowserControlsOffsetTagsDataView data,
                   cc::BrowserControlsOffsetTags* out) {
    return data.ReadTopControlsOffsetTag(&out->top_controls_offset_tag) &&
           data.ReadContentOffsetTag(&out->content_offset_tag) &&
           data.ReadBottomControlsOffsetTag(&out->bottom_controls_offset_tag);
  }
};

template <>
struct StructTraits<cc::mojom::BrowserControlsOffsetTagModificationsDataView,
                    cc::BrowserControlsOffsetTagModifications> {
  static const cc::BrowserControlsOffsetTags& tags(
      const cc::BrowserControlsOffsetTagModifications& input) {
    return input.tags;
  }

  static int top_controls_additional_height(
      const cc::BrowserControlsOffsetTagModifications& input) {
    return input.top_controls_additional_height;
  }

  static int bottom_controls_additional_height(
      const cc::BrowserControlsOffsetTagModifications& input) {
    return input.bottom_controls_additional_height;
  }

  static bool Read(
      cc::mojom::BrowserControlsOffsetTagModificationsDataView data,
      cc::BrowserControlsOffsetTagModifications* out) {
    out->top_controls_additional_height = data.top_controls_additional_height();
    out->bottom_controls_additional_height =
        data.bottom_controls_additional_height();
    return data.ReadTags(&out->tags);
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_MOJOM_TRAITS_H_
