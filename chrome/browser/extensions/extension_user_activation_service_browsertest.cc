// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_activation_service.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class ExtensionUserActivationServiceBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Load a simple extension that listens to messages.
    test_dir_.WriteManifest(R"({
      "name": "Test",
      "version": "1.0",
      "manifest_version": 3,
      "permissions": ["storage"]
    })");
    test_dir_.WriteFile(
        FILE_PATH_LITERAL("page.html"),
        R"(<html><body><script src="page.js"></script></body></html>)");
    test_dir_.WriteFile(FILE_PATH_LITERAL("page.js"), R"(
      chrome.storage.onChanged.addListener(() => {});
      chrome.test.sendMessage('ready');
    )");

    ExtensionTestMessageListener listener("ready");
    extension_ = LoadExtension(test_dir_.UnpackedPath());
    ASSERT_TRUE(extension_);

    GURL page_url = extension_->GetResourceURL("page.html");
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), page_url));

    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

 protected:
  const Extension* extension() const { return extension_.get(); }

 private:
  TestExtensionDir test_dir_;
  scoped_refptr<const Extension> extension_;
};

// Tests that when an extension calls an API method while a user gesture is
// active, the service records it properly.
IN_PROC_BROWSER_TEST_F(ExtensionUserActivationServiceBrowserTest,
                       ApiFunctionCall) {
  ExtensionUserActivationService* service =
      ExtensionUserActivationService::Get(profile());
  ASSERT_TRUE(service);

  content::WebContents* web_contents = GetActiveWebContents();

  // An API function call without a user gesture.
  EXPECT_FALSE(service->HasTransientActivation(extension()->id()));
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "chrome.storage.local.set({foo: 'bar'});",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(service->HasTransientActivation(extension()->id()));

  // An API function call with a user gesture.
  EXPECT_TRUE(
      content::ExecJs(web_contents, "chrome.storage.local.set({foo: 'bar'});"));
  EXPECT_TRUE(service->HasTransientActivation(extension()->id()));
}

// Tests that when an extension event is dispatched with a user gesture, the
// service records it properly.
IN_PROC_BROWSER_TEST_F(ExtensionUserActivationServiceBrowserTest,
                       EventDispatch) {
  ExtensionUserActivationService* service =
      ExtensionUserActivationService::Get(profile());
  ASSERT_TRUE(service);

  EventRouter* event_router = EventRouter::Get(profile());

  // Event dispatch without a user gesture.
  auto event1 = std::make_unique<Event>(events::FOR_TEST, "storage.onChanged",
                                        base::ListValue(), profile());
  event1->user_gesture = EventRouter::UserGestureState::kNotEnabled;
  event_router->DispatchEventToExtension(extension()->id(), std::move(event1));
  EXPECT_FALSE(service->HasTransientActivation(extension()->id()));

  // Event dispatch with a user gesture.
  auto event2 = std::make_unique<Event>(events::FOR_TEST, "storage.onChanged",
                                        base::ListValue(), profile());
  event2->user_gesture = EventRouter::UserGestureState::kEnabled;
  event_router->DispatchEventToExtension(extension()->id(), std::move(event2));
  EXPECT_TRUE(service->HasTransientActivation(extension()->id()));
}

}  // namespace extensions
