// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_CONTEXT_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_CONTEXT_H_

#include <stdint.h>

#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor {

// What get_pointer needs to know about a page to show and describe what the
// user pointed at. Both parts are captured together, so they agree.
struct PointerContext {
  // A JPEG of the viewport at its natural size, i.e. in device pixels.
  std::vector<uint8_t> screenshot;
  // Device pixels per DIP, for both the screenshot and the geometry in
  // `page_content`.
  float scale_factor = 1.0f;
  // What is on the page, with the geometry of everything in the viewport.
  optimization_guide::proto::AnnotatedPageContent page_content;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_CONTEXT_H_
