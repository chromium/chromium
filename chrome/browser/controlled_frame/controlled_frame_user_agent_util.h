// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_USER_AGENT_UTIL_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_USER_AGENT_UTIL_H_

#include <optional>

namespace blink {
struct UserAgentOverride;
}  // namespace blink

namespace controlled_frame {

blink::UserAgentOverride GetDefaultControlledFrameUserAgentOverride();

}

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_USER_AGENT_UTIL_H_
