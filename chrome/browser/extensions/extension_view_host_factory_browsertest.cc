// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/input.h>
#include <android/keycodes.h>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/ui_android_features.h"
#include "ui/events/android/key_event_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using ExtensionViewHostFactoryTest = ExtensionBrowserTest;

// Tests that ExtensionHosts are created with the correct type and profiles.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest, CreateExtensionHosts) {
  // Load a simple MV3 extension.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  content::BrowserContext* browser_context = profile();

  // Popup hosts are created with the correct type and profile.
  std::unique_ptr<ExtensionViewHost> host =
      ExtensionViewHostFactory::CreatePopupHost(*extension, extension->url(),
                                                GetBrowserWindowInterface());
  EXPECT_EQ(extension.get(), host->extension());
  EXPECT_EQ(browser_context, host->browser_context());
  EXPECT_EQ(mojom::ViewType::kExtensionPopup, host->extension_host_type());
  autofill::ContentAutofillClient* autofill_client =
      autofill::ContentAutofillClient::FromWebContents(host->host_contents());
  ASSERT_TRUE(autofill_client);
}

// Tests that side panel hosts are created with the correct profile and
// browsers.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest,
                       CreateExtensionSidePanelHost) {
  // Load a simple MV3 extension.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("side_panel")
                        .AppendASCII("simple_default"));
  ASSERT_TRUE(extension);

  content::BrowserContext* browser_context = profile();

  {
    // Create a side panel host with a browser passed in.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(
            *extension, extension->url(), GetBrowserWindowInterface(),
            /*tab_interface=*/nullptr);
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }

  {
    // Create a side panel host with a tab based WebContents passed in.
    auto* browser = GetBrowserWindowInterface();
    auto* tab_list = TabListInterface::From(browser);
    auto* active_tab = tab_list->GetActiveTab();
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(
            *extension, extension->url(), /*browser=*/nullptr, active_tab);
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }
}

#if BUILDFLAG(IS_ANDROID)
namespace {

input::NativeWebKeyboardEvent CreateKeyDownEvent(int key_code) {
  ui::KeyEventAndroid key_event(AKEY_EVENT_ACTION_DOWN, key_code,
                                /*meta_state=*/0);
  return input::NativeWebKeyboardEvent(
      jni_zero::AttachCurrentThread(), key_event.GetJavaObject(),
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now(), key_code,
      /*scancode=*/0, /*unicode_character=*/0, /*is_system_key=*/false);
}

}  // namespace

class ExtensionViewHostFactoryShortcutsAndroidTest
    : public ExtensionViewHostFactoryTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      ui::kAndroidUnifiedKeyboardShortcuts};
};

IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryShortcutsAndroidTest,
                       PreHandleKeyboardEventForwardsToActiveTabDelegate) {
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("side_panel")
                        .AppendASCII("simple_default"));
  ASSERT_TRUE(extension);

  auto* browser = GetBrowserWindowInterface();
  auto* active_tab = TabListInterface::From(browser)->GetActiveTab();
  ASSERT_TRUE(active_tab);

  input::NativeWebKeyboardEvent f6_event = CreateKeyDownEvent(AKEYCODE_F6);

  std::unique_ptr<ExtensionViewHost> popup_host =
      ExtensionViewHostFactory::CreatePopupHost(*extension, extension->url(),
                                                browser);
  EXPECT_EQ(content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT,
            popup_host->PreHandleKeyboardEvent(popup_host->host_contents(),
                                               f6_event));

  std::unique_ptr<ExtensionViewHost> browser_side_panel_host =
      ExtensionViewHostFactory::CreateSidePanelHost(
          *extension, extension->url(), browser, /*tab_interface=*/nullptr);
  EXPECT_EQ(content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT,
            browser_side_panel_host->PreHandleKeyboardEvent(
                browser_side_panel_host->host_contents(), f6_event));

  std::unique_ptr<ExtensionViewHost> tab_side_panel_host =
      ExtensionViewHostFactory::CreateSidePanelHost(
          *extension, extension->url(), /*browser=*/nullptr, active_tab);
  EXPECT_EQ(content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT,
            tab_side_panel_host->PreHandleKeyboardEvent(
                tab_side_panel_host->host_contents(), f6_event));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace extensions
