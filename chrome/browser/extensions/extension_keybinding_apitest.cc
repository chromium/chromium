// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/javascript_test_observer.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"

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

#if BUILDFLAG(IS_MAC)
const char kBookmarkKeybinding[] = "Command+D";
#else
const char kBookmarkKeybinding[] = "Ctrl+D";
#endif  // BUILDFLAG(IS_MAC)

bool SendBookmarkKeyPressSync(Browser* browser) {
  return ui_test_utils::SendKeyPressSync(browser, ui::VKEY_D,
#if BUILDFLAG(IS_MAC)
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

  DomMessageListener(const DomMessageListener&) = delete;
  DomMessageListener& operator=(const DomMessageListener&) = delete;

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

// Programmatically (from the extension) sets the action of |extension| to be
// visible on the tab with the given |tab_id|. Expects the action is *not*
// visible to start.
void SetActionVisibleOnTab(Profile* profile,
                           const Extension& extension,
                           int tab_id) {
  ExtensionActionManager* action_manager = ExtensionActionManager::Get(profile);
  const ExtensionAction* extension_action =
      action_manager->GetExtensionAction(extension);
  ASSERT_TRUE(extension_action);
  EXPECT_FALSE(extension_action->GetIsVisible(tab_id));

  static constexpr char kScriptTemplate[] =
      R"(chrome.pageAction.show(%d, () => {
           chrome.test.sendScriptResult(
               chrome.runtime.lastError ?
                   chrome.runtime.lastError.message :
                   'success');
         });)";

  base::Value set_result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile, extension.id(), base::StringPrintf(kScriptTemplate, tab_id));
  EXPECT_EQ("success", set_result);
  EXPECT_TRUE(extension_action->GetIsVisible(tab_id));
}

// Sends a keypress with the given |keyboard_code| to the specified |extension|.
// If |expect_dispatch| is true, expects pageAction.onClicked to be dispatched
// to the extension. Otherwise, expects it is not sent.
void SendKeyPressToAction(Browser* browser,
                          const Extension& extension,
                          ui::KeyboardCode keyboard_code,
                          const char* event_name,
                          bool expect_dispatch) {
  ExtensionTestMessageListener click_listener("clicked");
  click_listener.set_extension_id(extension.id());

  Profile* profile = browser->profile();
  EventRouter* event_router = EventRouter::Get(profile);
  TestEventRouterObserver event_tracker(event_router);
  // Activate the shortcut (Alt+Shift+F).
  if (!ui_test_utils::SendKeyPressSync(browser, keyboard_code, false, true,
                                       true, false)) {
    ADD_FAILURE() << "Could not send key press!";
    return;
  }
  base::RunLoop().RunUntilIdle();
  // Check that the event was dispatched if and only if we expected it to be.
  EXPECT_EQ(expect_dispatch,
            base::Contains(event_tracker.dispatched_events(), event_name));

  // Do a round-trip to the extension renderer. This serves as a pseudo-
  // RunUntilIdle()-type of method for the extension renderer itself, since
  // test.sendMessage() is FIFO.
  // This allows us to return the result of click_listener.was_satisfied(),
  // rather than using WaitUntilSatisfied(), which in turn allows this method
  // to exercise both the case of expecting dispatch and expecting *not* to
  // dispatch.
  static constexpr char kScript[] =
      R"(chrome.test.sendMessage(
             'run loop hack',
             () => {
               chrome.test.sendScriptResult('success');
             });)";
  base::Value set_result = browsertest_util::ExecuteScriptInBackgroundPage(
      profile, extension.id(), kScript);
  EXPECT_EQ("success", set_result);
  EXPECT_EQ(expect_dispatch, click_listener.was_satisfied());
}

// Given an |action_type|, returns the corresponding command key.
const char* GetCommandKeyForActionType(ActionInfo::Type action_type) {
  const char* command_key = nullptr;
  switch (action_type) {
    case ActionInfo::Type::kBrowser:
      command_key = manifest_values::kBrowserActionCommandEvent;
      break;
    case ActionInfo::Type::kPage:
      command_key = manifest_values::kPageActionCommandEvent;
      break;
    case ActionInfo::Type::kAction:
      command_key = manifest_values::kActionCommandEvent;
      break;
  }

  return command_key;
}

}  // namespace

class CommandsApiTest : public ExtensionApiTest {
 public:
  CommandsApiTest() {}
  ~CommandsApiTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
#if BUILDFLAG(IS_MAC)
    // ExtensionKeybindingRegistryViews doesn't get registered until BrowserView
    // is activated at least once.
    // TODO(crbug.com/41386956): Registry creation should happen independent of
    // activation. Focus manager lifetime may make this tricky to untangle.
    // TODO(crbug.com/40486728): Reassess after activation is restored in the
    // focus manager.
    ui_test_utils::BrowserActivationWaiter waiter(browser());
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
    waiter.WaitForActivation();
    ASSERT_TRUE(browser()->window()->IsActive());
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Some builders are flaky due to slower loading interacting with
    // deferred commits. This primarily impacts chromeos for the test
    // CommandsApiTest.ContinuePropagation.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

 protected:
  bool IsGrantedForTab(const Extension* extension,
                       const content::WebContents* web_contents) {
    return extension->permissions_data()->HasAPIPermissionForTab(
        sessions::SessionTabHelper::IdForTab(web_contents).id(),
        mojom::APIPermissionID::kTab);
  }

  // Returns true if the extension with the given |extension_id| has an active
  // command associated with an action of the given |action_type|.
  bool HasActiveActionCommand(const ExtensionId& extension_id,
                              ActionInfo::Type action_type) {
    bool active = false;
    Command command;
    CommandService* const command_service =
        CommandService::Get(browser()->profile());
    bool found_command = command_service->GetExtensionActionCommand(
        extension_id, action_type, CommandService::ALL, &command, &active);
    return found_command && active;
  }

  // Navigates to a test URL and return the ID of the navigated tab.
  int NavigateToTestURLAndReturnTabId() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/extensions/test_file.txt")));
    return sessions::SessionTabHelper::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
        ->session_id()
        .id();
  }
};

class IncognitoCommandsApiTest : public CommandsApiTest,
                                 public testing::WithParamInterface<bool> {};

// A parameterized version to allow testing with different action types.
class ActionCommandsApiTest
    : public CommandsApiTest,
      public testing::WithParamInterface<ActionInfo::Type> {};

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

  // Test that there are two browser actions in the toolbar.
  ExtensionsToolbarContainer* extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ASSERT_EQ(2, extensions_container->GetNumberOfActionsForTesting());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt")));

  // activeTab shouldn't have been granted yet.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  EXPECT_FALSE(IsGrantedForTab(extension, tab));

  ExtensionTestMessageListener test_listener;  // Won't reply.
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

IN_PROC_BROWSER_TEST_F(CommandsApiTest, InactivePageActionDoesntTrigger) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  const int tab_id = NavigateToTestURLAndReturnTabId();

  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(profile());
  const ExtensionAction* extension_action =
      action_manager->GetExtensionAction(*extension);
  ASSERT_TRUE(extension_action);
  EXPECT_FALSE(extension_action->GetIsVisible(tab_id));

  // If the page action is disabled / hidden, the event shouldn't be dispatched.
  bool expect_dispatch = false;
  SendKeyPressToAction(browser(), *extension, ui::VKEY_F,
                       "pageAction.onClicked", expect_dispatch);
}

// Tests that a page action that is unpinned and only shown within the
// extensions menu will still properly trigger when the keybinding is used.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, UnpinnedPageActionTriggers) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  ExtensionsToolbarContainer* extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  RunScheduledLayouts();
  EXPECT_FALSE(extensions_container->IsActionVisibleOnToolbar(extension->id()));

  const int tab_id = NavigateToTestURLAndReturnTabId();
  SetActionVisibleOnTab(profile(), *extension, tab_id);

  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  constexpr bool kExpectDispatch = true;
  SendKeyPressToAction(browser(), *extension, ui::VKEY_F,
                       "pageAction.onClicked", kExpectDispatch);
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

  const int tab_id = NavigateToTestURLAndReturnTabId();

  SetActionVisibleOnTab(profile(), *extension, tab_id);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  bool expect_dispatch = true;
  SendKeyPressToAction(browser(), *extension, ui::VKEY_G,
                       "pageAction.onClicked", expect_dispatch);
}

// Verify that keyboard shortcut takes effect without reloading the extension.
// Regression test for https://crbug.com/1190476.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, ActionKeyUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Simulate the user changing the keybinding.
  CommandService* command_service = CommandService::Get(browser()->profile());
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kActionCommandEvent, "Ctrl+Shift+Y");

  // Verify that the action event occurs for the new keyboard shortcut.
  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_Y, true, true,
                                              false, false));
  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(CommandsApiTest, PageActionOverrideChromeShortcut) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/page_action")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());
// Simulate the user setting the keybinding to override the print shortcut.
#if BUILDFLAG(IS_MAC)
  std::string print_shortcut = "Command+P";
#else
  std::string print_shortcut = "Ctrl+P";
#endif
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kPageActionCommandEvent,
      print_shortcut);

  const int tab_id = NavigateToTestURLAndReturnTabId();

  SetActionVisibleOnTab(profile(), *extension, tab_id);
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));

  ExtensionTestMessageListener test_listener;  // Won't reply.
  test_listener.set_extension_id(extension->id());

  // Note: The following incantation uses too many custom bits to comfortably
  // fit into SendKeyPressToAction(); do it manually.
  bool control_is_modifier = false;
  bool command_is_modifier = false;
#if BUILDFLAG(IS_MAC)
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt")));

  // Activate the regular shortcut (Alt+Shift+F).
  ExtensionTestMessageListener alt_shift_f_listener("alt_shift_f");
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, true, true, false));
  EXPECT_TRUE(alt_shift_f_listener.WaitUntilSatisfied());

  // Try to activate the Ctrl+F shortcut (shouldn't work).
  // Since keypresses are sent synchronously, we can check this by first sending
  // Ctrl+F (which shouldn't work), followed by Alt+Shift+F (which should work),
  // and listening for both. If, by the time we receive the Alt+Shift+F
  // response, we haven't received a response for Ctrl+F, it is safe to say we
  // won't receive one.
  ExtensionTestMessageListener ctrl_f_listener("ctrl_f");
  alt_shift_f_listener.Reset();
  // Send Ctrl+F.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F, true,
                                              false, false, false));
  // Send Alt+Shift+F.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F, false,
                                              true, true, false));
  EXPECT_TRUE(alt_shift_f_listener.WaitUntilSatisfied());
  EXPECT_FALSE(ctrl_f_listener.was_satisfied());
}

// This test validates that user-set override of the Chrome bookmark shortcut in
// an extension that does not request it does supersede the same keybinding by
// web pages.
IN_PROC_BROWSER_TEST_F(CommandsApiTest,
                       OverwriteBookmarkShortcutByUserOverridesWebKeybinding) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ASSERT_TRUE(RunExtensionTest("keybinding/basics")) << message_;

  CommandService* command_service = CommandService::Get(browser()->profile());

  const Extension* extension = GetSingleLoadedExtension();
  // Simulate the user setting the keybinding to Ctrl+D.
  command_service->UpdateKeybindingPrefs(
      extension->id(), manifest_values::kBrowserActionCommandEvent,
      kBookmarkKeybinding);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file_with_ctrl-d_keybinding.html")));

  ExtensionTestMessageListener test_listener;
  // Activate the shortcut (Ctrl+D) which should be handled by the extension.
  ASSERT_TRUE(SendBookmarkKeyPressSync(browser()));
  EXPECT_TRUE(test_listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string("basics browser action"), test_listener.message());
}

#if BUILDFLAG(IS_WIN)
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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 with different keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_reassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+J.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_J, accelerator.key_code());
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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it has a command of Alt+Shift+F.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_F, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());

  // Update to version 2 without keybinding assigned.
  EXPECT_TRUE(UpdateExtension(kId, path_v2_unassigned, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify it is set to nothing.
  ui::Accelerator accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_UNKNOWN, accelerator.key_code());

  // Simulate the user setting the keybinding to Alt+Shift+G.
  command_service->UpdateKeybindingPrefs(
      kId, manifest_values::kBrowserActionCommandEvent, kAltShiftG);

  // Update to version 2 with keybinding.
  EXPECT_TRUE(UpdateExtension(kId, path_v2, 0));
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

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
  EXPECT_TRUE(registry->enabled_extensions().GetByID(kId) != nullptr);

  // Verify the keybinding is still set.
  accelerator = command_service->FindCommandByName(
      kId, manifest_values::kBrowserActionCommandEvent).accelerator();
  EXPECT_EQ(ui::VKEY_G, accelerator.key_code());
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsShiftDown());
  EXPECT_TRUE(accelerator.IsAltDown());
}

//
#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(NDEBUG)
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt")));

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
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CommandsApiTest, ChromeOSConversions) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(RunExtensionTest("keybinding/chromeos_conversions")) << message_;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt")));

  ResultCatcher catcher;

  // Send all expected keys (Search+Shift+{Left, Up, Right, Down}).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              true, false, true));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              true, false, true));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RIGHT, false,
                                              true, false, true));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              true, false, true));

  ASSERT_TRUE(catcher.GetNextResult());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Make sure component extensions retain keybindings after removal then
// re-adding.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, AddRemoveAddComponentExtension) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      RunExtensionTest("keybinding/component", {}, {.load_as_component = true}))
      << message_;

  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->component_loader()
      ->Remove("pkplfbidichfdicaijlchgnapepdginl");

  ASSERT_TRUE(
      RunExtensionTest("keybinding/component", {}, {.load_as_component = true}))
      << message_;
}

// Validate parameters sent along with an extension event, in response to
// command being triggered.
IN_PROC_BROWSER_TEST_F(CommandsApiTest, TabParameter) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("keybinding/tab_parameter")) << message_;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.txt")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  ResultCatcher catcher;
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_Y, true, true,
                                              false, false));  // Ctrl+Shift+Y
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test Keybinding in incognito mode.
IN_PROC_BROWSER_TEST_P(IncognitoCommandsApiTest, IncognitoMode) {
  ASSERT_TRUE(embedded_test_server()->Start());

  bool is_incognito_enabled = GetParam();

  ASSERT_TRUE(RunExtensionTest("keybinding/basics", {},
                               {.allow_in_incognito = is_incognito_enabled}))
      << message_;

  // Open incognito window and navigate to test page.
  Browser* incognito_browser = OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser,
      embedded_test_server()->GetURL("/extensions/test_file.txt")));

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

IN_PROC_BROWSER_TEST_P(ActionCommandsApiTest,
                       TriggeringCommandTriggersListener) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const ActionInfo::Type action_type = GetParam();

  // Load a test extension that has a command that invokes the action, and sends
  // a message when the action is invoked.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Extension Action Listener Test",
      "manifest_version": %d,
      "version": "0.1",
      "commands": {
        "%s": {
          "suggested_key": {
            "default": "Alt+Shift+U"
          }
        }
      },
      "%s": {},
      "background": { %s }
    }
  )";
  constexpr char kBackgroundScriptTemplate[] = R"(
      chrome.%s.onClicked.addListener(() => {
        chrome.test.sendMessage('clicked');
      });
      chrome.test.sendMessage('ready');
  )";
  const char* background_specification =
      action_type == ActionInfo::Type::kAction
          ? R"("service_worker": "background.js")"
          : R"("scripts": ["background.js"])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(action_type),
      GetCommandKeyForActionType(action_type),
      ActionInfo::GetManifestKeyForActionType(action_type),
      background_specification));
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kBackgroundScriptTemplate,
                                        GetAPINameForActionType(action_type)));

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_TRUE(HasActiveActionCommand(extension->id(), action_type));

  const int tab_id = NavigateToTestURLAndReturnTabId();

  // If the action is a page action, it's hidden by default. Show it.
  if (action_type == ActionInfo::Type::kPage) {
    SetActionVisibleOnTab(profile(), *extension, tab_id);
    ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  }

  ExtensionTestMessageListener click_listener("clicked");
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_U, false,
                                              true, true, false));
  EXPECT_TRUE(click_listener.WaitUntilSatisfied());
}

// This test validates that commands.getAll() returns commands associated with
// a registered [page/browser] action.
IN_PROC_BROWSER_TEST_P(ActionCommandsApiTest, GetAllReturnsActionCommand) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const ActionInfo::Type action_type = GetParam();

  // Load a test extension that has a command for the current action type.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Extension Commands Get All Test",
      "manifest_version": %d,
      "version": "0.1",
      "commands": {
        "%s": {
          "suggested_key": {
            "default": "Ctrl+Shift+5"
          }
        }
      },
      "%s": {},
      "background": { %s }
    }
  )";
  constexpr char kBackgroundScriptTemplate[] = R"(
      var platformBinding =
        /Mac/.test(navigator.platform) ? '⇧⌘5' : 'Ctrl+Shift+5';
      chrome.commands.getAll(function(commands) {
        chrome.test.assertEq(1, commands.length);

        chrome.test.assertEq("%s",            commands[0].name);
        chrome.test.assertEq("",              commands[0].description);
        chrome.test.assertEq(platformBinding, commands[0].shortcut);

        chrome.test.notifyPass();
      });
  )";
  const char* background_specification =
      action_type == ActionInfo::Type::kAction
          ? R"("service_worker": "background.js")"
          : R"("scripts": ["background.js"])";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(action_type),
      GetCommandKeyForActionType(action_type),
      ActionInfo::GetManifestKeyForActionType(action_type),
      background_specification));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(kBackgroundScriptTemplate,
                         GetCommandKeyForActionType(action_type)));

  EXPECT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

// Tests that triggering a command associated with an action opens an
// extension's popup.
IN_PROC_BROWSER_TEST_P(ActionCommandsApiTest, TriggeringCommandTriggersPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const ActionInfo::Type action_type = GetParam();

  // Load an extension that specifies a command to invoke the action, and has
  // a default popup.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Extension Action Listener Test",
      "manifest_version": %d,
      "version": "0.1",
      "commands": {
        "%s": {
          "suggested_key": {
            "default": "Alt+Shift+U"
          }
        }
      },
      "%s": {"default_popup": "popup.html"}
    }
  )";
  constexpr char kPopupHtml[] = R"(
      <!doctype html>
      <html>
        <script src="popup.js"></script>
      </html>
  )";
  constexpr char kPopupJs[] = "chrome.test.notifyPass();";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, GetManifestVersionForActionType(action_type),
      GetCommandKeyForActionType(action_type),
      ActionInfo::GetManifestKeyForActionType(action_type)));
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.js"), kPopupJs);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(HasActiveActionCommand(extension->id(), action_type));

  const int tab_id = NavigateToTestURLAndReturnTabId();

  if (action_type == ActionInfo::Type::kPage) {
    // Note: We don't use SetActionVisibleOnTab() here because it relies on a
    // background page, which this extension doesn't have.
    ExtensionActionManager::Get(profile())
        ->GetExtensionAction(*extension)
        ->SetIsVisible(tab_id, true);
    ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  }

  // Invoke the action, and wait for the popup to show.
  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_U, false,
                                              true, true, false));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Verify popup is shown.
  ExtensionsToolbarContainer* extensions_container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ToolbarActionViewController* popup_owner =
      extensions_container->popup_owner_for_testing();
  EXPECT_TRUE(popup_owner);
  EXPECT_TRUE(popup_owner->GetPopupNativeView());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActionCommandsApiTest,
                         testing::Values(ActionInfo::Type::kBrowser,
                                         ActionInfo::Type::kPage,
                                         ActionInfo::Type::kAction));

INSTANTIATE_TEST_SUITE_P(All, IncognitoCommandsApiTest, testing::Bool());

}  // namespace extensions
