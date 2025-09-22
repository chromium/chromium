// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions WebNavigation API functions for observing and
// intercepting navigation events, as specified in the extension JSON API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class WebNavigationEventRouter;

// API function that returns the state of a given frame.
class WebNavigationGetFrameFunction : public ExtensionFunction {
  ~WebNavigationGetFrameFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getFrame", WEBNAVIGATION_GETFRAME)
};

// API function that returns the states of all frames in a given tab.
class WebNavigationGetAllFramesFunction : public ExtensionFunction {
  ~WebNavigationGetAllFramesFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webNavigation.getAllFrames",
                             WEBNAVIGATION_GETALLFRAMES)
};

class WebNavigationAPI : public BrowserContextKeyedAPI,
                         public extensions::EventRouter::Observer {
 public:
  explicit WebNavigationAPI(content::BrowserContext* context);

  WebNavigationAPI(const WebNavigationAPI&) = delete;
  WebNavigationAPI& operator=(const WebNavigationAPI&) = delete;

  ~WebNavigationAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<WebNavigationAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebNavigationAPI>;
  friend class WebNavigationTabObserver;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "WebNavigationAPI";
  }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<WebNavigationEventRouter> web_navigation_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_NAVIGATION_WEB_NAVIGATION_API_H_
