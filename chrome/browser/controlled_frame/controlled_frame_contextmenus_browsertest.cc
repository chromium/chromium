// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace controlled_frame {

using testing::Each;
using testing::Eq;

constexpr char kEvalSuccessStr[] = "SUCCESS";

class ControlledFrameContextMenusTest : public ControlledFrameTestBase {
 public:
  ControlledFrameContextMenusTest()
      : ControlledFrameTestBase(
            /*channel=*/version_info::Channel::STABLE,
            /*feature_setting=*/FeatureSetting::ENABLED,
            /*flag_setting=*/FlagSetting::CONTROLLED_FRAME) {}

  void SetUpOnMainThread() override {
    ControlledFrameTestBase::SetUpOnMainThread();
    StartContentServer("web_apps/simple_isolated_app");
  }

  const extensions::MenuItem::Id CreateMenuItemId(
      const extensions::MenuItem::ExtensionKey& extension_key,
      const std::string& string_uid) {
    extensions::MenuItem::Id id;
    id.extension_key = extension_key;
    id.string_uid = string_uid;
    return id;
  }

  void ExpectMenuItemWithIdAndTitle(
      const extensions::MenuItem::ExtensionKey& extension_key,
      const std::string& expected_id,
      const std::string& expected_title) {
    auto* menu_manager = extensions::MenuManager::Get(profile());
    extensions::MenuItem* menu_item =
        menu_manager->GetItemById(CreateMenuItemId(extension_key, expected_id));

    ASSERT_TRUE(menu_item);
    EXPECT_EQ(expected_title, menu_item->title());
  }

  const content::EvalJsResult CreateContextMenuItem(
      content::RenderFrameHost* app_frame,
      const std::string& id,
      const std::string& title) {
    return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise(async (resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.create) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.create is undefined');
          return;
        }
        await frame.contextMenus.create({ title: $2, id: $1 });
        resolve('SUCCESS');
      });
    )",
                                                         id, title));
  }

  const content::EvalJsResult UpdateContextMenuItemTitle(
      content::RenderFrameHost* app_frame,
      const std::string& id,
      const std::string& new_title) {
    return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise(async (resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.update) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.update is undefined');
          return;
        }

        await frame.contextMenus.update(/*id=*/$1, { title: $2 });
        resolve('SUCCESS');
      });
  )",
                                                         id, new_title));
  }

  const content::EvalJsResult RemoveContextMenuItem(
      content::RenderFrameHost* app_frame,
      const std::string& id) {
    return content::EvalJs(app_frame, content::JsReplace(R"(
      new Promise(async (resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.remove) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.remove is undefined');
          return;
        }

        await frame.contextMenus.remove(/*id=*/$1);
        resolve('SUCCESS');
      });
  )",
                                                         id));
  }

  const content::EvalJsResult RemoveAllContextMenuItems(
      content::RenderFrameHost* app_frame) {
    return content::EvalJs(app_frame, R"(
      new Promise(async (resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.removeAll) {
          reject('FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.removeAll is undefined');
          return;
        }

        await frame.contextMenus.removeAll();
        resolve('SUCCESS');
      });
  )");
  }

  void SimulateOpenContextMenu(content::RenderFrameHost* controlled_frame) {
    CHECK(controlled_frame);
    content::WaitForHitTestData(
        content::WebContents::FromRenderFrameHost(controlled_frame));

    auto context_menu_interceptor =
        std::make_unique<content::ContextMenuInterceptor>(controlled_frame);
    gfx::Point click_pos(1, 1);
    content::SimulateMouseClickAt(
        content::WebContents::FromRenderFrameHost(controlled_frame),
        blink::WebInputEvent::kNoModifiers,
        blink::WebMouseEvent::Button::kRight, click_pos);
    context_menu_interceptor->Wait();
  }

  void SimulateClickContextMenuItem(
      content::RenderFrameHost* controlled_frame) {
    CHECK(controlled_frame);
    // Create and build our test context menu.
    std::unique_ptr<TestRenderViewContextMenu> menu(
        TestRenderViewContextMenu::Create(
            controlled_frame, controlled_frame->GetLastCommittedURL()));
    // Look for the extension item in the menu, and execute it.
    int command_id =
        extensions::ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
    CHECK(menu->IsCommandIdEnabled(command_id));
    menu->ExecuteCommand(command_id, /*event_flags=*/0);
  }
};

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, CreateShowContextClick) {
  constexpr std::string kItemID = "107";
  // Create IWA with ControlledFrame
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
// Add JS with test open and click handlers
  auto add_handler_script = content::JsReplace(
      R"(
    document.onShowHandler = function() {
      document.onShowCount = (document.onShowCount ?? 0) + 1;
    };

    document.onClickedHandler = function(info) {
      document.clickedMenuItemId =
          [...(document.clickedMenuItemId ?? []), info.menuItem.id];
      document.globalOnClickedCount = (document.globalOnClickedCount ?? 0) + 1;
    };

    new Promise(async (resolve, reject) => {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.contextMenus || !frame.contextMenus.create) {
        reject('FAIL: frame, frame.contextMenus, or ' +
            'frame.contextMenus.create is undefined');
        return;
      }

      frame.contextMenus.addEventListener('show', document.onShowHandler);
      frame.contextMenus.addEventListener('click', document.onClickedHandler);

      await frame.contextMenus.create(
      {
        title: 'test_title',
        id: $2,
      });
      resolve('SUCCESS');
    });
  )", kEvalSuccessStr, kItemID);

  ASSERT_EQ(content::EvalJs(app_frame, add_handler_script), kEvalSuccessStr);
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);
  content::RenderFrameHost* controlled_frame =
      web_view_guest->GetGuestMainFrame();
  ASSERT_TRUE(controlled_frame);

  // Simulate right click and expect the listener to be triggered.
  SimulateOpenContextMenu(controlled_frame);
  ASSERT_EQ(content::EvalJs(app_frame, "document.onShowCount"), 1);

  // Simulate the click on an item expect click and item id be registered
  SimulateClickContextMenuItem(controlled_frame);
  EXPECT_EQ(content::EvalJs(app_frame, "document.globalOnClickedCount"), 1);
  EXPECT_THAT(content::EvalJs(app_frame, "document.clickedMenuItemId")
                  .TakeValue()
                  .TakeList(),
              Each(Eq(kItemID)));

  // We don't need any clean-up after
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, Create) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"",
      web_view_guest->owner_rfh()->GetProcess()->GetDeprecatedID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  EXPECT_EQ(0u, menu_manager->MenuItemsSize(extension_key));

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem2ID = "2";
  static constexpr std::string kItem2Title = "Test2";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem2ID, kItem2Title));
  ASSERT_EQ(2u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem2ID, kItem2Title);

  static constexpr std::string kItem3ID = "3";
  static constexpr std::string kItem3Title = "Test3";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem3ID, kItem3Title));
  ASSERT_EQ(3u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem3ID, kItem3Title);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, Update) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"",
      web_view_guest->owner_rfh()->GetProcess()->GetDeprecatedID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem1NewTitle = "Test1";
  EXPECT_EQ(kEvalSuccessStr,
            UpdateContextMenuItemTitle(app_frame, kItem1ID, kItem1NewTitle));

  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1NewTitle);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, Remove) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test1";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_frame, kItem1ID, kItem1Title));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveContextMenuItem(app_frame, kItem1ID));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"",
      web_view_guest->owner_rfh()->GetProcess()->GetDeprecatedID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));

  extensions::MenuItem* deleted_item =
      menu_manager->GetItemById(CreateMenuItemId(extension_key, kItem1ID));
  EXPECT_FALSE(deleted_item);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, RemoveAll) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  auto* menu_manager = extensions::MenuManager::Get(profile());

  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"1",
                                                   /*title=*/"Test1"));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_frame, /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveAllContextMenuItems(app_frame));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"",
      web_view_guest->owner_rfh()->GetProcess()->GetDeprecatedID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(0u, menu_manager->MenuItemsSize(extension_key));
}

// TODO(crbug.com/392208013): Fix and enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowEvent DISABLED_ShowEvent
#else
#define MAYBE_ShowEvent ShowEvent
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, MAYBE_ShowEvent) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  auto add_handler_script = content::JsReplace(
      R"(
document.onShowHandler = function() {
  document.onShowCount = (document.onShowCount ?? 0) + 1;
};

(function() {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    return ('FAIL: frame or frame.contextMenus is undefined');
  }

  frame.contextMenus.addEventListener('show', document.onShowHandler);
  return $1;
})();
)",
      kEvalSuccessStr);

  // Add a listener for 'show' then simulate right click and expect the
  // listener to be triggered.
  ASSERT_EQ(content::EvalJs(app_frame, add_handler_script), kEvalSuccessStr);

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);
  content::RenderFrameHost* controlled_frame =
      web_view_guest->GetGuestMainFrame();
  ASSERT_TRUE(controlled_frame);
  SimulateOpenContextMenu(controlled_frame);
  ASSERT_EQ(content::EvalJs(app_frame, "document.onShowCount"), 1);

  auto remove_handler_script = content::JsReplace(
      R"(
(function() {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    return ('FAIL: frame or frame.contextMenus is undefined');
  }

  frame.contextMenus.removeEventListener('show', document.onShowHandler);
  return $1;
})();
)",
      kEvalSuccessStr);

  // Remove the listener for 'onShow' then simulate right click and expect the
  // listener to not be triggered.
  ASSERT_EQ(content::EvalJs(app_frame, remove_handler_script), kEvalSuccessStr);

  SimulateOpenContextMenu(controlled_frame);
  ASSERT_EQ(content::EvalJs(app_frame, "document.onShowCount"), 1);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, NoLegacyOnShowEvent) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  const auto* check_legacy_event_script = R"(
new Promise(async (resolve, reject) => {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    reject('FAIL: frame or frame.contextMenus is undefined');
    return;
  }

  if (frame.contextMenus.onShow) {
    reject('FAIL: contextMenus object contains an onShow attribute');
    return;
  }
  resolve('SUCCESS');
});
    )";

  ASSERT_EQ(content::EvalJs(app_frame, check_legacy_event_script),
            kEvalSuccessStr);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, ClickEvent) {
  constexpr std::string kItemID = "107";

  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  auto create_context_menu_script = content::JsReplace(R"(
new Promise(async (resolve, reject) => {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus || !frame.contextMenus.create) {
    reject('FAIL: frame, frame.contextMenus, or ' +
        'frame.contextMenus.create is undefined');
    return;
  }
  await frame.contextMenus.create(
  {
    title: 'test_title',
    id: $1,
  });
  resolve('SUCCESS');
});
    )",
                                                       kItemID);

  // Create a ContextMenu item with a inline listener.
  ASSERT_EQ(content::EvalJs(app_frame, create_context_menu_script),
            kEvalSuccessStr);

  auto add_handler_script = content::JsReplace(
      R"(
document.onClickedHandler = function(info) {
  document.clickedMenuItemId =
      [...(document.clickedMenuItemId ?? []), info.menuItem.id];
  document.globalOnClickedCount = (document.globalOnClickedCount ?? 0) + 1;
};

(function() {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    return ('FAIL: frame or frame.contextMenus is undefined');
  }

  frame.contextMenus.addEventListener('click', document.onClickedHandler);
  return $1;
})();
)",
      kEvalSuccessStr);

  // Add a global listener for the 'clicked' event, then simulate clicking on
  // menu item.
  ASSERT_EQ(content::EvalJs(app_frame, add_handler_script), kEvalSuccessStr);

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  ASSERT_TRUE(web_view_guest);
  content::RenderFrameHost* controlled_frame =
      web_view_guest->GetGuestMainFrame();
  ASSERT_TRUE(controlled_frame);
  SimulateClickContextMenuItem(controlled_frame);

  EXPECT_EQ(content::EvalJs(app_frame, "document.globalOnClickedCount"), 1);
  EXPECT_THAT(content::EvalJs(app_frame, "document.clickedMenuItemId")
                  .TakeValue()
                  .TakeList(),
              Each(Eq(kItemID)));

  auto remove_handler_script = content::JsReplace(
      R"(
(function() {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    return ('FAIL: frame or frame.contextMenus is undefined');
  }

  frame.contextMenus.removeEventListener('click', document.onClickedHandler);
  return $1;
})();
)",
      kEvalSuccessStr);

  // Remove the global listener for 'click' then simulate clicking on menu
  // item.
  ASSERT_EQ(content::EvalJs(app_frame, remove_handler_script), kEvalSuccessStr);

  SimulateClickContextMenuItem(controlled_frame);
  EXPECT_EQ(content::EvalJs(app_frame, "document.globalOnClickedCount"), 1);
  EXPECT_THAT(
      content::EvalJs(app_frame, "document.clickedMenuItemId").ExtractList(),
      Each(Eq(kItemID)));
}

IN_PROC_BROWSER_TEST_F(ControlledFrameContextMenusTest, NoLegacyOnClickEvent) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(web_app::ManifestBuilder());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  ASSERT_TRUE(CreateControlledFrame(
      app_frame, embedded_https_test_server().GetURL("/index.html")));

  const auto* check_legacy_event_script = R"(
new Promise(async (resolve, reject) => {
  const frame = document.getElementsByTagName('controlledframe')[0];
  if (!frame || !frame.contextMenus) {
    reject('FAIL: frame or frame.contextMenus is undefined');
    return;
  }

  if (frame.contextMenus.onClick) {
    reject('FAIL: contextMenus object contains an onClick attribute');
    return;
  }
  resolve('SUCCESS');
});
    )";

  ASSERT_EQ(content::EvalJs(app_frame, check_legacy_event_script),
            kEvalSuccessStr);
}

}  // namespace controlled_frame
