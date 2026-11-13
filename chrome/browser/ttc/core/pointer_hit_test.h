// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_HIT_TEST_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_HIT_TEST_H_

#include <optional>

#include "base/values.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace ttc {

// Describes, for the model, the page element at `point` according to
// `page_content`, which must have been extracted with geometry (the
// actionable mode). `point` is in the viewport's DIP space and `scale_factor`
// is the viewport's device pixels per DIP. If the point is on part of a link
// or a form control, e.g. a button's text, the element is the link or
// control. The description has:
//
// - "dom_node_id" and "document_id": the element's identity, as the tools
//   that act on elements take it;
// - "type": what kind of content it is, e.g. "image", "text" or "anchor";
// - "label": its accessible label, if it has one;
// - "text": the start of its text, including that of its descendants and
//   captions of images, if there is any;
// - "bounds": where it is in the viewport, in DIPs.
//
// Returns nullopt if nothing is at the point, or only the page's background:
// a document itself, or a plain container spanning most of the viewport.
std::optional<base::DictValue> DescribeNodeAtPoint(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    const gfx::PointF& point,
    float scale_factor);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_HIT_TEST_H_
