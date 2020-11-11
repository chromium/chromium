// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_

#include <string>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace apps {

std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host);
std::string GetScope(content::RenderFrameHost* render_frame_host);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_
