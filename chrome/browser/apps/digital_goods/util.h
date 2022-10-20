// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_

#include <string>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace apps {

/**
 * Gets the TWA package name associated with the website present in the
 * RenderFrameHost. An empty string will be returned if there is no associated
 * TWA, if the website is in an Incognito tab, or if the website is not in a
 * web app window.
 */
std::string GetTwaPackageName(content::RenderFrameHost* render_frame_host);
std::string GetScope(content::RenderFrameHost* render_frame_host);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_UTIL_H_
