// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_COMMON_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_COMMON_H_

namespace content {
class RenderFrameHost;
struct Referrer;
}  // namespace content

class GURL;

// Shared code between PluginObserver and PluginObserverAndroid.

// Only sets `referrer` on success.
bool CanOpenPdfUrl(content::RenderFrameHost* render_frame_host,
                   const GURL& url,
                   const GURL& last_committed_url,
                   content::Referrer* referrer);

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_COMMON_H_
