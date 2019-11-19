// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/javascript_test_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace extensions {

namespace {

// This extension ID is used for tests require a stable ID over multiple
// extension installs.
const char kId[] = "pgoakhfeplldmjheffidklpoklkppipp";

// Default keybinding to use for emulating user-defined shortcut overrides. The
// test extensions use Alt+Shift+F and Alt+Shift+H.
const char kAltShiftG[] = "Alt+Shift+G";

// Name of the command for the "basics" test extension.
const char kBasicsShortcutCommandName[] = "toggle-feature";
// Name of the command for the overwrite_bookmark_shortcut test extension.
const char kOverwriteBookmarkShortcutCommandName[] = "send message";

#if defined(OS_MACOSX)
const char kBookmarkKeybinding[] = "Command+D";
#else
const char kBookmarkKeybinding[] = "Ctrl+D";
#endif  // defined(OS_MACOSX)

bool SendBookmarkKeyPressSync(Browser* browser) {
  return ui_test_utils::SendKeyPressSync(
      browser, ui::VKEY_D,
#if defined(OS_MACOSX)
      false, false, false, true
#else
      true, false, false, false
#endif
      );
}

// Named command for media key overwrite test.
const char kMediaKeyTestCommand[] = "test_mediakeys_update";

// A scoped observer that listens for dom automation messages.
class DomMessageListener : public content::TestMessageHandler {
 public:
  explicit DomMessageListener(content::WebContents* web_contents);
  ~DomMessageListener() override;

  // Wait until a message is received.
  void Wait();

  // Clears and resets the observer.
  void Clear();

  const std::string& message() const { return message_; }

 private:
  // content::TestMessageHandler:
  MessageResponse HandleMessage(const std::string& json) override;
  void Reset() override;

  // The message received. Note that this will be JSON, so if it is a string,
  // it will be wrapped in quotes.
  std::string message_;

  content::JavascriptTestObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(DomMessageListener);
};

DomMessageListener::DomMessageListener(content::WebContents* web_contents)
    : observer_(web_contents, this) {
}

DomMessageListener::~DomMessageListener() {
}

void DomMessageListener::Wait() {
  observer_.Run();
}

void DomMessageListener::Clear() {
  // We don't just call this in DomMessageListener::Reset() because the
  // JavascriptTestObserver's Reset() method also resets its handler (this).
  observer_.Reset();
}

content::TestMessageHandler::MessageResponse DomMessageListener::HandleMessage(
    const std::string& json) {
  message_ = json;
  return DONE;
}

void DomMessageListener::Reset() {
  TestMessageHandler::Reset();
  message_.clear();
}

} // namespace

class CommandsApiTest : public ExtensionApiTest {
 public:
  CommandsApiTest() {}
  ~CommandsApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
#if defined(OS_MACOSX)
    // ExtensionKeybindingRegistryViews doesn't get registered until BrowserView
    // is activated at least once.
    // TODO(crbug.com/839469): Registry creation should happen independent of
    // activation. Focus manager lifetime may make this tricky to untangle.
    // TODO(crbug.com/650859): Reassess after activation is restored in the
    // focus manager.
    ui_test_utils::BrowserActivationWaiter waiter(browser());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    waiter.WaitForActivation();
    ASSERT_TRUE(browser()->window()->IsActive());
#endif
  }

 protected:
  bool IsGrantedForTab(const Extension* extension,
                       const content::WebContents* web_contents) {
    return extension->permissions_data()->HasAPIPermissionForTab(
        SessionTabHelper::IdForTab(web_contents).id(), APIPermission::kTab);
  }

#if defined(OS_CHROMEOS)
  void RunChromeOSConversionTest(const std::string& extension_path) {
    // Setup the environment.
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    ASSERT_TRUE(RunExtensionTest(extension_path)) << message_;
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

    ResultCatcher catcher;

    // Send all expected keys (Search+Shift+{Left, Up, Right, Down}).
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_LEFT, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_UP, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_RIGHT, false, true, false, true));
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_DOWN, false, true, false, true));

    ASSERT_TRUE(catcher.GetNextResult());
  }
#endif  // OS_CHROMEOS
};

class IncognitoCommandsApiTest : public CommandsApiTest,
                                 public testing::WithParamInterface<bool> {};

// Test the basic functionality of the Keybinding API:
// - That pressing the shortcut keys should perform actions (activate the
//   browser action or send an event).
// - Note: Page action keybindings are tested in PageAction test below.
// - The shortcut keys taken by one extension are not overwritten by the last
//   installed extension.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Load this extension, which uses the same keybindings but sets the page
  // to different colors. This is so we can see that it doesn't interfere. We
  // don't test this extension in any other way (it should otherwise be
  // immaterial to this test).
  ASSERT_TRUE(RunExtensionTest("keybinding/conflicting")) << message_;

  auto browser_actions_bar = BrowserActionTestUtil::Create(browser());
  // Test that there are two browser actions in the toolbar.
  ASSERT_EQ(2, browser_actions_bar->NumberOfBrowserActions());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

  // activeTab shouldn't have been granted yet.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(IsGrantedForTab(extension, tab));

  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  // Activate the browser action shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  // activeTab should now be granted.
  EXPECT_TRUE(IsGrantedForTab(extension, tab));
  // Verify the command worked.
  EXPECT_EQ(std::string("basics browser action"), test_listener.message());

  test_listener.Reset();
  // Activate the command shortcut (Ctrl+Shift+Y).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_Y, true, true, false, false));
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string(kBasicsShortcutCommandName), test_listener.message());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, PageAction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Load a page, the extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  // Make sure it appears and is the right one.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  int tab_id = SessionTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())->session_id().id();
  ExtensionAction* action = ExtensionActionManager::Get(browser()->profile())
                                ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_EQ(ActionInfo::TYPE_PAGE, action->action_type());
  EXPECT_EQ("Send message", action->GetTitle(tab_id));

  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  test_listener.set_extension_id(extension->id());

  // Activate the shortcut (Alt+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, true, true, false));

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ("clicked", test_listener.message());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, PageActionKeyUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());
  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kPageActionCommandEvent, kAltShiftG);

  {
    // Load a page. The extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  test_listener.set_extension_id(extension->id());

  // Activate the shortcut.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_G, false, true, true, false));

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ("clicked", test_listener.message());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, PageActionOverrideChromeShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());
// Simulate the user setting the keybinding to override the print shortcut.
#if defined(OS_MACOSX)
  std::string print_shortcut = "Command+P";
#else
  std::string print_shortcut = "Ctrl+P";
#endif
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kPageActionCommandEvent,
      print_shortcut);

  {
    // Load a page. The extension will detect the navigation and request to show
    // the page action icon.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  test_listener.set_extension_id(extension->id());

  bool control_is_modifier = false;
  bool command_is_modifier = false;
#if defined(OS_MACOSX)
  command_is_modifier = true;
#else
  control_is_modifier = true;
#endif

  // Activate the omnibox. This checks to ensure that the extension shortcut
  // still works even if the WebContents isn't focused.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_L,
                                              control_is_modifier, false, false,
                                              command_is_modifier));

  // Activate the shortcut.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_P,
                                              control_is_modifier, false, false,
                                              command_is_modifier));

  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ("clicked", test_listener.message());
}

// This test validates that the getAll query API function returns registered
// commands as well as synthesized ones and that inactive commands (like the
// synthesized ones are in nature) have no shortcuts.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, SynthesizedCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/synthesized")) << message_;
}

// This test validates that an extension cannot request a shortcut that is
// already in use by Chrome.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, DontOverwriteSystemShortcuts) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(RunExtensionTest("keybinding/dont_overwrite_system")) << message_;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

  // Activate the regular shortcut (Alt+Shift+F).
  ExtensionTestMessageListener alt_shift_f_listener("alt_shift_f", false);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, true, true, false));
  EXPECT_TRUE(alt_shift_f_listener.WaitUntilSatisfied());

  // Try to activate the bookmark shortcut (Ctrl+D). This should not work
  // without requesting via chrome_settings_overrides.
  //
  // Since keypresses are sent synchronously, we can check this by first sending
  // Ctrl+D (which shouldn't work), followed by Alt+Shift+F (which should work),
  // and listening for both. If, by the time we receive the Alt+Shift+F
  // response, we haven't received a response for Ctrl+D, it is safe to say we
  // won't receive one.
  {
    ExtensionTestMessageListener ctrl_d_listener("ctrl_d", false);
    alt_shift_f_listener.Reset();
    // Send Ctrl+D.
    ASSERT_TRUE(SendBookmarkKeyPressSync(browser()));
    // Send Alt+Shift+F.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_F, false, true, true, false));
    EXPECT_TRUE(alt_shift_f_listener.WaitUntilSatisfied());
    EXPECT_FALSE(ctrl_d_listener.was_satisfied());
  }

  // Try to activate the Ctrl+F shortcut (shouldn't work).
  {
    ExtensionTestMessageListener ctrl_f_listener("ctrl_f", false);
    alt_shift_f_listener.Reset();
    // Send Ctrl+F.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_F, true, false, false, false));
    // Send Alt+Shift+F.
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), ui::VKEY_F, false, true, true, false));
    EXPECT_TRUE(alt_shift_f_listener.WaitUntilSatisfied());
    EXPECT_FALSE(ctrl_f_listener.was_satisfied());
  }
}

// This test validates that an extension can remove the Chrome bookmark shortcut
// if it has requested to do so.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, RemoveBookmarkShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/remove_bookmark_shortcut"))
      << message_;

  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));
}

// This test validates that an extension cannot remove the Chrome bookmark
// shortcut without being given permission with a feature flag.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       RemoveBookmarkShortcutWithoutPermission) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_TRUE(RunExtensionTestIgnoreManifestWarnings(
      "keybinding/remove_bookmark_shortcut"));

  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));
}

// This test validates that an extension that removes the Chrome bookmark
// shortcut continues to remove the bookmark shortcut with a user-assigned
// Ctrl+D shortcut (i.e. it does not trigger the overwrite functionality).
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       RemoveBookmarkShortcutWithUserKeyBinding) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/remove_bookmark_shortcut"))
      << message_;

  // Check that the shortcut is removed.
  CommandService* command_service = CommandService::Get(browser()->profile());
  const Extension* extension = GetSingleLoadedExtension();
  // Simulate the user setting a keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  // Force the command enable state to be recalculated.
  browser()->command_controller()->ExtensionStateChanged();

  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));
}

// This test validates that an extension can override the Chrome bookmark
// shortcut if it has requested to do so.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, OverwriteBookmarkShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

  // Activate the shortcut (Ctrl+D) to send a test message.
  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  ASSERT_TRUE(SendBookmarkKeyPressSync(browser()));
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string(kOverwriteBookmarkShortcutCommandName),
            test_listener.message());
}

// This test validates that an extension that requests to override the Chrome
// bookmark shortcut, but does not get the keybinding, does not remove the
// bookmark UI.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutWithoutKeybinding) {
  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  const Extension* extension = GetSingleLoadedExtension();
  CommandService* command_service = CommandService::Get(browser()->profile());
  CommandMap commands;
  // Verify the expected command is present.
  EXPECT_TRUE(command_service->GetNamedCommands(
      extension->id(), CommandService::SUGGESTED, CommandService::ANY_SCOPE,
      &commands));
  EXPECT_EQ(1u, commands.count(kOverwriteBookmarkShortcutCommandName));

  // Simulate the user removing the Ctrl+D keybinding from the command.
  command_service->RemoveKeybindingPrefs(
      extension->id(), kOverwriteBookmarkShortcutCommandName);

  // Force the command enable state to be recalculated.
  browser()->command_controller()->ExtensionStateChanged();

  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_BOOKMARK_THIS_TAB));
}

// This test validates that an extension override of the Chrome bookmark
// shortcut does not supersede the same keybinding by web pages.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutDoesNotOverrideWebKeybinding) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/overwrite_bookmark_shortcut"))
      << message_;

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file_with_ctrl-d_keybinding.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Activate the shortcut (Ctrl+D) which should be handled by the page and send
  // a test message.
  DomMessageListener listener(tab);
  ASSERT_TRUE(SendBookmarkKeyPressSync(browser()));
  listener.Wait();
  EXPECT_EQ(std::string("\"web page received\""), listener.message());
}

// This test validates that user-set override of the Chrome bookmark shortcut in
// an extension that does not request it does supersede the same keybinding by
// web pages.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutByUserOverridesWebKeybinding) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // This functionality requires a feature flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--enable-override-bookmarks-ui", "1");

  ASSERT_TRUE(RunExtensionTest("keybinding/basics"))
      << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());

  const Extension* extension = GetSingleLoadedExtension();
  // Simulate the user setting the keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file_with_ctrl-d_keybinding.html"));

  ExtensionTestMessageListener test_listener(false);  // Won't reply.
  // Activate the shortcut (Ctrl+D) which should be handled by the extension.
  ASSERT_TRUE(SendBookmarkKeyPressSync(browser()));
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string("basics browser action"), test_listener.message());
}

#if defined(OS_WIN)
// Currently this feature is implemented on Windows only.
#define MAYBE_AllowDuplicatedMediaKeys AllowDuplicatedMediaKeys
#else
#define MAYBE_AllowDuplicatedMediaKeys DISABLED_AllowDuplicatedMediaKeys
#endif

// Test that media keys go to all extensions that register for them.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_AllowDuplicatedMediaKeys) {
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_0"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(RunExtensionTest("keybinding/non_global_media_keys_1"))
      << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  // Activate the Media Stop key.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_MEDIA_STOP, false, false, false, false));

  // We should get two success result.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutAddedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v1_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v1_unassigned.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v2"),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutChangedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_reassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_reassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+H.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_H, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, ShortcutRemovedOnUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_unassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the keybinding gets set to nothing.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutAddedOnUpdateAfterBeingAssignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v1_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v1_unassigned.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v2"),
                               scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
                               pem_path, base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension without keybinding assigned.
  ASSERT_TRUE(InstallExtension(path_v1_unassigned, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the previously-set keybinding is still set.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutChangedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_reassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_reassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+G.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

// Test that Media keys do not overwrite previous settings.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
    MediaKeyShortcutChangedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("mk_v1"),
      scoped_temp_dir.GetPath().AppendASCII("mk_v1.crx"), pem_path,
      base::FilePath());
  base::FilePath path_v2_reassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("mk_v2"),
      scoped_temp_dir.GetPath().AppendASCII("mk_v2.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of MediaPlayPause.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, kMediaKeyTestCommand).accelerator();
  EXPECT_EQ(ui::VKEY_MEDIA_PLAY_PAUSE, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_FALSE(accelerator.IsShiftDown());
  EXPECT_FALSE(accelerator.IsAltDown());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, kMediaKeyTestCommand, kAltShiftG);

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+G.
  accelerator = command_service->FindCommandByName(
      kId, kMediaKeyTestCommand).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       ShortcutRemovedOnUpdateAfterBeingReassignedByUser) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath pem_path = test_data_dir_.
      AppendASCII("keybinding").AppendASCII("keybinding.pem");
  base::FilePath path_v1 =
      PackExtensionWithOptions(test_data_dir_.AppendASCII("keybinding")
                                   .AppendASCII("update")
                                   .AppendASCII("v1"),
                               scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
                               pem_path, base::FilePath());
  base::FilePath path_v2_unassigned = PackExtensionWithOptions(
      test_data_dir_.AppendASCII("keybinding")
          .AppendASCII("update")
          .AppendASCII("v2_unassigned"),
      scoped_temp_dir.GetPath().AppendASCII("v2_unassigned.crx"), pem_path,
      base::FilePath());

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  CommandService* command_service = CommandService::Get(browser()->profile());

  // Install v1 of the extension.
  ASSERT_TRUE(InstallExtension(path_v1, 1));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Simulate the user reassigning the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->GetExtensionById(kId, ExtensionRegistry::ENABLED) !=
              NULL);

  // Verify the keybinding is still set.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

//
#if defined(OS_CHROMEOS) && !defined(NDEBUG)
// TODO(dtseng): Test times out on Chrome OS debug. See http://crbug.com/412456.
#define MAYBE_ContinuePropagation DISABLED_ContinuePropagation
#else
#define MAYBE_ContinuePropagation ContinuePropagation
#endif

IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_ContinuePropagation) {
  // Setup the environment.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(RunExtensionTest("keybinding/continue_propagation")) << message_;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt"));

  ResultCatcher catcher;

  // Activate the shortcut (Ctrl+Shift+F). The page should capture the
  // keystroke and not the extension since |onCommand| has no event listener
  // initially.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());

  // Now, the extension should have added an |onCommand| event listener.
  // Send the same key, but the |onCommand| listener should now receive it.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());

  // The extension should now have removed its |onCommand| event listener.
  // Finally, the page should again receive the key.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, true, true, false, false));
  ASSERT_TRUE(catcher.GetNextResult());
}

// Test is only applicable on Chrome OS.
#if defined(OS_CHROMEOS)
// http://crbug.com/410534
#if defined(USE_OZONE)
#define MAYBE_ChromeOSConversions DISABLED_ChromeOSConversions
#else
#define MAYBE_ChromeOSConversions ChromeOSConversions
#endif
IN_PROC_BROWSER_TEST_F(CommandsApiTest, MAYBE_ChromeOSConversions) {
  RunChromeOSConversionTest("keybinding/chromeos_conversions");
}
#endif  // OS_CHROMEOS

// Make sure component extensions retain keybindings after removal then
// re-adding.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, AddRemoveAddComponentExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunComponentExtensionTest("keybinding/component")) << message_;

  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->component_loader()
      ->Remove("pkplfbidichfdicaijlchgnapepdginl");

  ASSERT_TRUE(RunComponentExtensionTest("keybinding/component")) << message_;
}

// Test Keybinding in incognito mode.
IN_PROC_BROWSER_TEST_P(IncognitoCommandsApiTest, IncognitoMode) {
  ASSERT_TRUE(embedded_test_server()->Start());

  bool is_incognito_enabled = GetParam();

  if (is_incognito_enabled)
    ASSERT_TRUE(RunExtensionTestIncognito("keybinding/basics")) << message_;
  else
    ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;

  // Open incognito window and navigate to test page.
  Browser* incognito_browser = OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  ui_test_utils::NavigateToURL(
      incognito_browser,
      embedded_test_server()->GetURL("/extensions/test_file.txt"));

  TestEventRouterObserver test_observer(
      EventRouter::Get(incognito_browser->profile()));

  // Activate the browser action shortcut (Ctrl+Shift+F).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(incognito_browser, ui::VKEY_F,
                                              true, true, false, false));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(is_incognito_enabled,
            base::Contains(test_observer.dispatched_events(),
                           "browserAction.onClicked"));

  test_observer.ClearEvents();

  // Activate the command shortcut (Ctrl+Shift+Y).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(incognito_browser, ui::VKEY_Y,
                                              true, true, false, false));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      is_incognito_enabled,
      base::Contains(test_observer.dispatched_events(), "commands.onCommand"));
}

INSTANTIATE_TEST_SUITE_P(, IncognitoCommandsApiTest, testing::Bool());

}  // namespace extensions
