// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_
#define CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace plugins {

// Implements the plugins portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientPluginsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientPluginsPart();

  ChromeContentBrowserClientPluginsPart(
      const ChromeContentBrowserClientPluginsPart&) = delete;
  ChromeContentBrowserClientPluginsPart& operator=(
      const ChromeContentBrowserClientPluginsPart&) = delete;

  ~ChromeContentBrowserClientPluginsPart() override;

 private:
  void ExposeInterfacesToRendererForRenderFrameHost(
      content::RenderFrameHost& frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
};

}  // namespace plugins

#endif  // CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_
