// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SERVICE_WORKER_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_SERVICE_WORKER_APITEST_H_

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/page_type.h"
#include "extensions/browser/process_manager.h"

class GURL;

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

class ServiceWorkerTest : public ExtensionApiTest {
 public:
  ServiceWorkerTest(const ServiceWorkerTest&) = delete;
  ServiceWorkerTest& operator=(const ServiceWorkerTest&) = delete;

 protected:
  ServiceWorkerTest() = default;
  ~ServiceWorkerTest() override = default;

  void SetUpOnMainThread() override;

  // Returns the ProcessManager for the test's profile.
  ProcessManager* process_manager() { return ProcessManager::Get(profile()); }

  // Starts running a test from the background page test extension.
  //
  // This registers a service worker with |script_name|, and fetches the
  // registration result.
  const Extension* StartTestFromBackgroundPage(const char* script_name);

  // Navigates the browser to a new tab at |url|, waits for it to load, then
  // returns it.
  content::WebContents* Navigate(const GURL& url);

  // Navigates the browser to |url| and returns the new tab's page type.
  content::PageType NavigateAndGetPageType(const GURL& url);

  // Extracts the innerText from |contents|.
  std::string ExtractInnerText(content::WebContents* contents);

  // Navigates the browser to |url|, then returns the innerText of the new
  // tab's WebContents' main frame.
  std::string NavigateAndExtractInnerText(const GURL& url);

  size_t GetWorkerRefCount(const blink::StorageKey& key);
};

class ServiceWorkerBasedBackgroundTest : public ServiceWorkerTest {
 public:
  ServiceWorkerBasedBackgroundTest() = default;

  ServiceWorkerBasedBackgroundTest(const ServiceWorkerBasedBackgroundTest&) =
      delete;
  ServiceWorkerBasedBackgroundTest& operator=(
      const ServiceWorkerBasedBackgroundTest&) = delete;

  ~ServiceWorkerBasedBackgroundTest() override {}

  void SetUpOnMainThread() override;

  // Returns the only running worker id for |extension_id|.
  // Returns std::nullopt if there isn't any worker running or more than one
  // worker is running for |extension_id|.
  std::optional<WorkerId> GetUniqueRunningWorkerId(
      const ExtensionId& extension_id);

  bool ExtensionHasRenderProcessHost(const ExtensionId& extension_id);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SERVICE_WORKER_APITEST_H_
