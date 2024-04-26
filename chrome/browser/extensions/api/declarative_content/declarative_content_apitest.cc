// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace {

static constexpr char kDeclarativeContentManifest[] =
    R"({
      "name": "Declarative Content apitest",
      "version": "0.1",
      "manifest_version": 2,
      "description": "end-to-end browser test for the declarative Content API",
      "background": {
        "scripts": ["background.js"],
        "persistent": true
      },
      "page_action": {},
      "permissions": ["declarativeContent", "bookmarks"],
      "incognito": "%s"
    })";

constexpr char kIncognitoSpecificBackground[] =
    R"(var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
       var ShowAction = chrome.declarativeContent.ShowAction;
       var inIncognitoContext = chrome.extension.inIncognitoContext;

       var hostPrefix = chrome.extension.inIncognitoContext ?
           'test_split' : 'test_normal';
       var rule = {
         conditions: [new PageStateMatcher({
             pageUrl: {hostPrefix: hostPrefix}})],
         actions: [new ShowAction()]
       };

       var onPageChanged = chrome.declarativeContent.onPageChanged;
       onPageChanged.removeRules(undefined, function() {
         onPageChanged.addRules([rule], function() {
           chrome.test.sendMessage(
               !inIncognitoContext ? 'ready' : 'ready (split)');
         });
       });)";

constexpr char kBackgroundHelpers[] =
    R"(var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
       var ShowAction = chrome.declarativeContent.ShowAction;
       var onPageChanged = chrome.declarativeContent.onPageChanged;

       function setRulesInPageEnvironment(rules, responseString) {
         return new Promise(resolve => {
           onPageChanged.removeRules(undefined, function() {
             onPageChanged.addRules(rules, function() {
               if (chrome.runtime.lastError) {
                 resolve(chrome.runtime.lastError.message);
                 return;
               }
               resolve(responseString);
             });
           });
         });
       };

       function setRules(rules, responseString) {
         onPageChanged.removeRules(undefined, function() {
           onPageChanged.addRules(rules, function() {
             if (chrome.runtime.lastError) {
               chrome.test.sendScriptResult(chrome.runtime.lastError.message);
               return;
             }
             chrome.test.sendScriptResult(responseString);
           });
         });
       };

       function addRules(rules, responseString) {
         onPageChanged.addRules(rules, function() {
           if (chrome.runtime.lastError) {
             chrome.test.sendScriptResult(chrome.runtime.lastError.message);
             return;
           }
           chrome.test.sendScriptResult(responseString);
         });
       };

       function removeRule(id, responseString) {
         onPageChanged.removeRules([id], function() {
           if (chrome.runtime.lastError) {
             chrome.test.sendScriptResult(chrome.runtime.lastError.message);
             return;
           }
           chrome.test.sendScriptResult(responseString);
         });
       };)";

using ContextType = ExtensionBrowserTest::ContextType;

class DeclarativeContentApiTest : public ExtensionApiTest {
 public:
  explicit DeclarativeContentApiTest(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}

  DeclarativeContentApiTest(const DeclarativeContentApiTest&) = delete;
  DeclarativeContentApiTest& operator=(const DeclarativeContentApiTest&) =
      delete;

 protected:
  enum IncognitoMode { SPANNING, SPLIT };

  // Checks that the rules are correctly evaluated for an extension in incognito
  // mode |mode| when the extension's incognito enable state is
  // |is_enabled_in_incognito|.
  void CheckIncognito(IncognitoMode mode, bool is_enabled_in_incognito);

  // Checks that the rules matching a bookmarked state of |is_bookmarked| are
  // correctly evaluated on bookmark events.
  void CheckBookmarkEvents(bool is_bookmarked);

  static std::string FormatManifest(IncognitoMode mode);

  TestExtensionDir ext_dir_;
};

std::string DeclarativeContentApiTest::FormatManifest(IncognitoMode mode) {
  const char* const mode_string = mode == SPANNING ? "spanning" : "split";
  return base::StringPrintf(kDeclarativeContentManifest, mode_string);
}

void DeclarativeContentApiTest::CheckIncognito(IncognitoMode mode,
                                               bool is_enabled_in_incognito) {
  std::string manifest = FormatManifest(mode);
  ext_dir_.WriteManifest(manifest);
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                     kIncognitoSpecificBackground);

  ExtensionTestMessageListener ready("ready");
  ExtensionTestMessageListener ready_incognito("ready (split)");

  const Extension* extension = LoadExtension(
      ext_dir_.UnpackedPath(), {.allow_in_incognito = is_enabled_in_incognito});
  ASSERT_TRUE(extension);

  Browser* incognito_browser = CreateIncognitoBrowser();
  const ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_browser->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(incognito_action);

  ASSERT_TRUE(ready.WaitUntilSatisfied());
  if (is_enabled_in_incognito && mode == SPLIT)
    ASSERT_TRUE(ready_incognito.WaitUntilSatisfied());

  content::WebContents* const incognito_tab =
      incognito_browser->tab_strip_model()->GetWebContentsAt(0);
  const int incognito_tab_id = ExtensionTabUtil::GetTabId(incognito_tab);

  EXPECT_FALSE(incognito_action->GetIsVisible(incognito_tab_id));

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(incognito_tab, GURL("http://test_split/")));
  if (mode == SPLIT) {
    EXPECT_EQ(is_enabled_in_incognito,
              incognito_action->GetIsVisible(incognito_tab_id));
  } else {
    EXPECT_FALSE(incognito_action->GetIsVisible(incognito_tab_id));
  }

  EXPECT_FALSE(NavigateInRenderer(incognito_tab, GURL("http://test_normal/")));
  if (mode != SPLIT) {
    EXPECT_EQ(is_enabled_in_incognito,
              incognito_action->GetIsVisible(incognito_tab_id));
  } else {
    EXPECT_FALSE(incognito_action->GetIsVisible(incognito_tab_id));
  }

  // Verify that everything works as expected in the non-incognito browser.
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  EXPECT_FALSE(action->GetIsVisible(tab_id));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test_normal/")));
  EXPECT_TRUE(action->GetIsVisible(tab_id));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test_split/")));
  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

void DeclarativeContentApiTest::CheckBookmarkEvents(bool match_is_bookmarked) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test1/")));
  EXPECT_FALSE(action->GetIsVisible(tab_id));

  static constexpr char kSetIsBookmarkedRule[] =
      R"(setRules([{
           conditions: [new PageStateMatcher({isBookmarked: %s})],
           actions: [new ShowAction()]
         }], 'test_rule');)";

  EXPECT_EQ("test_rule",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                base::StringPrintf(kSetIsBookmarkedRule,
                                   match_is_bookmarked ? "true" : "false")));
  EXPECT_EQ(!match_is_bookmarked, action->GetIsVisible(tab_id));

  // Check rule evaluation on add/remove bookmark.
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* node = bookmark_model->AddURL(
      bookmark_model->other_node(), 0, u"title", GURL("http://test1/"));
  EXPECT_EQ(match_is_bookmarked, action->GetIsVisible(tab_id));

  bookmark_model->Remove(
      node, bookmarks::metrics::BookmarkEditSource::kExtension, FROM_HERE);
  EXPECT_EQ(!match_is_bookmarked, action->GetIsVisible(tab_id));

  // Check rule evaluation on navigate to bookmarked and non-bookmarked URL.
  bookmark_model->AddURL(bookmark_model->other_node(), 0, u"title",
                         GURL("http://test2/"));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test2/")));
  EXPECT_EQ(match_is_bookmarked, action->GetIsVisible(tab_id));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test3/")));
  EXPECT_EQ(!match_is_bookmarked, action->GetIsVisible(tab_id));
}

class DeclarativeContentApiTestWithContextType
    : public DeclarativeContentApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  DeclarativeContentApiTestWithContextType()
      : DeclarativeContentApiTest(GetParam()) {}

  DeclarativeContentApiTestWithContextType(
      const DeclarativeContentApiTestWithContextType&) = delete;
  DeclarativeContentApiTestWithContextType& operator=(
      const DeclarativeContentApiTestWithContextType&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         DeclarativeContentApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
// These tests use page_action, which is unavailable in MV3.
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         DeclarativeContentApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorkerMV2));

// TODO(crbug.com/40260777): Convert this to run in both modes.
IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest, Overview) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(var declarative = chrome.declarative;

         var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
         var ShowAction = chrome.declarativeContent.ShowAction;

         var rule = {
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test1'}}),
                        new PageStateMatcher({
                            css: ["input[type='password']"]})],
           actions: [new ShowAction()]
         }

         var testEvent = chrome.declarativeContent.onPageChanged;

         testEvent.removeRules(undefined, function() {
           testEvent.addRules([rule], function() {
             chrome.test.sendMessage('ready')
           });
         });)");
  ExtensionTestMessageListener ready("ready");
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(ready.WaitUntilSatisfied());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // Observer to track page action visibility. This helps avoid
  // flakes by waiting to check visibility until there is an
  // actual update to the page action.
  ChromeExtensionTestNotificationObserver test_observer(browser());

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test1/")));
  // The declarative API should show the page action instantly, rather
  // than waiting for the extension to run.
  test_observer.WaitForPageActionVisibilityChangeTo(1);
  EXPECT_TRUE(action->GetIsVisible(tab_id));

  // Make sure leaving a matching page unshows the page action.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://not_checked/")));
  test_observer.WaitForPageActionVisibilityChangeTo(0);
  EXPECT_FALSE(action->GetIsVisible(tab_id));

  // Insert a password field to make sure that's noticed.
  // Notice that we touch offsetTop to force a synchronous layout.
  ASSERT_TRUE(content::ExecJs(
      tab, R"(document.body.innerHTML = '<input type="password">';
              document.body.offsetTop;)"));

  test_observer.WaitForPageActionVisibilityChangeTo(1);
  EXPECT_TRUE(action->GetIsVisible(tab_id))
      << "Adding a matching element should show the page action.";

  // Remove it again to make sure that reverts the action.
  // Notice that we touch offsetTop to force a synchronous layout.
  ASSERT_TRUE(content::ExecJs(tab, R"(document.body.innerHTML = 'Hello world';
                                     document.body.offsetTop;)"));

  test_observer.WaitForPageActionVisibilityChangeTo(0);
  EXPECT_FALSE(action->GetIsVisible(tab_id))
      << "Removing the matching element should hide the page action again.";
}

// Test that adds two rules pointing to single action instance.
// Regression test for http://crbug.com/574149.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       ReusedActionInstance) {
  static constexpr char kBackgroundScript[] =
      R"(var declarative = chrome.declarative;

         var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
         var ShowAction = chrome.declarativeContent.ShowAction;
         var actionInstance = new ShowAction();

         var rule1 = {
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test1'}})],
           actions: [actionInstance]
         };
         var rule2 = {
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test'}})],
           actions: [actionInstance]
         };

         var testEvent = chrome.declarativeContent.onPageChanged;

         testEvent.removeRules(undefined, function() {
           testEvent.addRules([rule1, rule2], function() {
             chrome.test.sendMessage('ready');
           });
         });)";
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);

  ExtensionTestMessageListener ready("ready");
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(ready.WaitUntilSatisfied());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // This navigation matches both rules.
  // TODO(crbug.com/40764017): Understand why this is EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test1/")));

  EXPECT_TRUE(action->GetIsVisible(tab_id));
}

// Tests that the rules are evaluated at the time they are added or removed.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       RulesEvaluatedOnAddRemove) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test1/")));

  const std::string kAddTestRules =
      R"(addRules([{
           id: '1',
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test1'}})],
           actions: [new ShowAction()]
         }, {
           id: '2',
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test2'}})],
           actions: [new ShowAction()]
         }], 'add_rules');)";
  EXPECT_EQ("add_rules",
            ExecuteScriptInBackgroundPage(extension->id(), kAddTestRules));

  EXPECT_TRUE(action->GetIsVisible(tab_id));

  const std::string kRemoveTestRule1 = "removeRule('1', 'remove_rule1');";
  EXPECT_EQ("remove_rule1",
            ExecuteScriptInBackgroundPage(extension->id(), kRemoveTestRule1));

  EXPECT_FALSE(action->GetIsVisible(tab_id));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test2/")));

  EXPECT_TRUE(action->GetIsVisible(tab_id));

  const std::string kRemoveTestRule2 = "removeRule('2', 'remove_rule2');";
  EXPECT_EQ("remove_rule2",
            ExecuteScriptInBackgroundPage(extension->id(), kRemoveTestRule2));

  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

struct ShowActionParams {
  constexpr ShowActionParams(const char* show_type, ContextType context_type)
      : show_type(show_type), context_type(context_type) {}
  const char* show_type;
  ContextType context_type;
};

class ParameterizedShowActionDeclarativeContentApiTest
    : public DeclarativeContentApiTest,
      public testing::WithParamInterface<ShowActionParams> {
 public:
  ParameterizedShowActionDeclarativeContentApiTest()
      : DeclarativeContentApiTest(GetParam().context_type) {}

  ParameterizedShowActionDeclarativeContentApiTest(
      const ParameterizedShowActionDeclarativeContentApiTest&) = delete;
  ParameterizedShowActionDeclarativeContentApiTest& operator=(
      const ParameterizedShowActionDeclarativeContentApiTest&) = delete;

  ~ParameterizedShowActionDeclarativeContentApiTest() override {}

  void TestShowAction(std::optional<ActionInfo::Type> action_type);
};

void ParameterizedShowActionDeclarativeContentApiTest::TestShowAction(
    std::optional<ActionInfo::Type> action_type) {
  static constexpr char kManifestTemplate[] =
      R"({
           "name": "Declarative Content Show Action",
           "version": "0.1",
           "manifest_version": %d,
           %s
           "permissions": ["declarativeContent"]
         })";
  std::string action_declaration;
  int manifest_version = 2;
  if (action_type) {
    action_declaration = base::StringPrintf(
        R"("%s": {},)", ActionInfo::GetManifestKeyForActionType(*action_type));
    manifest_version = GetManifestVersionForActionType(*action_type);
  }

  // Since this test uses the action API (which is restricted to MV3), we drive
  // the interaction through pages, rather than the background script.
  ext_dir_.WriteManifest(base::StringPrintf(kManifestTemplate, manifest_version,
                                            action_declaration.c_str()));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("page.html"),
                     R"("<html><script src="page.js"></script></html>)");
  ext_dir_.WriteFile(FILE_PATH_LITERAL("page.js"), kBackgroundHelpers);

  ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();

  ExtensionAction* action = ExtensionActionManager::Get(browser()->profile())
                                ->GetExtensionAction(*extension);
  // Extensions that don't provide an action are given a page action by default
  // (for visibility reasons).
  ASSERT_TRUE(action);

  // Ensure actions are hidden (so that the ShowAction() rule has an effect).
  if (action->default_state() == ActionInfo::DefaultState::kDisabled) {
    action->SetIsVisible(ExtensionAction::kDefaultTabId, false);
  }

  // Open the tab to invoke the APIs, as well as test the action visibility.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  static constexpr char kScript[] =
      R"(setRulesInPageEnvironment([{
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test'}})],
           actions: [new chrome.declarativeContent.%s()]
         }], 'test_rule');)";
  static constexpr char kSuccessStr[] = "test_rule";

  std::string result =
      content::EvalJs(tab, base::StringPrintf(kScript, GetParam().show_type))
          .ExtractString();

  // Since extensions with no action provided are given a page action by default
  // (for visibility reasons) and ShowAction() should also work with
  // browser actions, both of these should pass.
  EXPECT_THAT(result, testing::HasSubstr(kSuccessStr));

  // TODO(crbug.com/40764017): Understand why this is EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test/")));

  const int tab_id = sessions::SessionTabHelper::IdForTab(tab).id();
  EXPECT_TRUE(action->GetIsVisible(tab_id));

  // If an extension had no action specified in the manifest, it will get a
  // synthesized page action.
  ActionInfo::Type expected_type =
      action_type.value_or(ActionInfo::Type::kPage);
  EXPECT_EQ(expected_type, action->action_type());
  EXPECT_EQ(expected_type == ActionInfo::Type::kPage ? 1u : 0u,
            extension_action_test_util::GetVisiblePageActionCount(tab));
}

IN_PROC_BROWSER_TEST_P(ParameterizedShowActionDeclarativeContentApiTest,
                       NoActionInManifest) {
  TestShowAction(std::nullopt);
}

IN_PROC_BROWSER_TEST_P(ParameterizedShowActionDeclarativeContentApiTest,
                       PageActionInManifest) {
  TestShowAction(ActionInfo::Type::kPage);
}

IN_PROC_BROWSER_TEST_P(ParameterizedShowActionDeclarativeContentApiTest,
                       BrowserActionInManifest) {
  TestShowAction(ActionInfo::Type::kBrowser);
}

IN_PROC_BROWSER_TEST_P(ParameterizedShowActionDeclarativeContentApiTest,
                       ActionInManifest) {
  TestShowAction(ActionInfo::Type::kAction);
}

// Tests that rules from manifest are added and evaluated properly.
IN_PROC_BROWSER_TEST_P(ParameterizedShowActionDeclarativeContentApiTest,
                       RulesAddedFromManifest) {
  static constexpr char manifest[] =
      R"({
           "name": "Declarative Content apitest",
           "version": "0.1",
           "manifest_version": 2,
           "page_action": {},
           "permissions": ["declarativeContent"],
           "event_rules": [{
             "event": "declarativeContent.onPageChanged",
             "actions": [{
                "type": "declarativeContent.%s"
             }],
             "conditions": [{
               "type": "declarativeContent.PageStateMatcher",
               "pageUrl": {"hostPrefix": "test1"}
             }]
           }]
         })";
  ext_dir_.WriteManifest(base::StringPrintf(manifest, GetParam().show_type));
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://blank/")));
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test1/")));
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test2/")));
  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

INSTANTIATE_TEST_SUITE_P(
    LegacyShowActionKey_PB,
    ParameterizedShowActionDeclarativeContentApiTest,
    ::testing::Values(ShowActionParams("ShowPageAction",
                                       ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ModernShowActionKey_PB,
    ParameterizedShowActionDeclarativeContentApiTest,
    ::testing::Values(ShowActionParams("ShowAction",
                                       ContextType::kPersistentBackground)));

// Tests that rules are not evaluated in incognito browser windows when the
// extension specifies spanning incognito mode but is not enabled for incognito.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       DisabledForSpanningIncognito) {
  CheckIncognito(SPANNING, false);
}

// Tests that rules are evaluated in incognito browser windows when the
// extension specifies spanning incognito mode and is enabled for incognito.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       EnabledForSpanningIncognito) {
  CheckIncognito(SPANNING, true);
}

// Tests that rules are not evaluated in incognito browser windows when the
// extension specifies split incognito mode but is not enabled for incognito.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       DisabledForSplitIncognito) {
  CheckIncognito(SPLIT, false);
}

// Tests that rules are evaluated in incognito browser windows when the
// extension specifies split incognito mode and is enabled for incognito.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       EnabledForSplitIncognito) {
  CheckIncognito(SPLIT, true);
}

// Tests that rules are evaluated for an incognito tab that exists at the time
// the rules are added.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       RulesEvaluatedForExistingIncognitoTab) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  content::WebContents* const incognito_tab =
      incognito_browser->tab_strip_model()->GetWebContentsAt(0);
  const int incognito_tab_id = ExtensionTabUtil::GetTabId(incognito_tab);

  // TODO(crbug.com/40764017): Understand why this is EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(incognito_tab, GURL("http://test_normal/")));

  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"),
                     kIncognitoSpecificBackground);
  ExtensionTestMessageListener ready("ready");
  const Extension* extension =
      LoadExtension(ext_dir_.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready.WaitUntilSatisfied());

  const ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_browser->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(incognito_action);

  // The page action should be shown.
  EXPECT_TRUE(incognito_action->GetIsVisible(incognito_tab_id));
}

constexpr char kRulesExtensionName[] =
    "Declarative content persistence apitest";

// TODO(crbug.com/41189874): Flaky on Windows release builds and on LACROS.
#if (BUILDFLAG(IS_WIN) && defined(NDEBUG)) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PRE_RulesPersistence DISABLED_PRE_RulesPersistence
#else
#define MAYBE_PRE_RulesPersistence PRE_RulesPersistence
#endif
// Sets up rules matching http://test1/ in a normal and incognito browser.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       MAYBE_PRE_RulesPersistence) {
  ExtensionTestMessageListener ready("ready");
  ExtensionTestMessageListener ready_split("ready (split)");
  // An on-disk extension is required so that it can be reloaded later in the
  // RulesPersistence test.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("declarative_content")
                        .AppendASCII("persistence"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(kRulesExtensionName, extension->name());
  ASSERT_TRUE(ready.WaitUntilSatisfied());

  CreateIncognitoBrowser();
  ASSERT_TRUE(ready_split.WaitUntilSatisfied());
}

// TODO(crbug.com/41189874): Flaky on Windows release builds and on LACROS.
#if (BUILDFLAG(IS_WIN) && defined(NDEBUG)) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_RulesPersistence DISABLED_RulesPersistence
#else
#define MAYBE_RulesPersistence RulesPersistence
#endif
// Reloads the extension from PRE_RulesPersistence and checks that the rules
// continue to work as expected after being persisted and reloaded.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       MAYBE_RulesPersistence) {
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  ASSERT_EQ(kRulesExtensionName, extension->name());

  // Check non-incognito browser.
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // Observer to track page action visibility. This helps avoid
  // flakes by waiting to check visibility until there is an
  // actual update to the page action.
  ChromeExtensionTestNotificationObserver test_observer(browser());

  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_FALSE(action->GetIsVisible(tab_id));

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test_normal/")));
  test_observer.WaitForPageActionVisibilityChangeTo(1);
  EXPECT_TRUE(action->GetIsVisible(tab_id));

  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test_split/")));
  test_observer.WaitForPageActionVisibilityChangeTo(0);
  EXPECT_FALSE(action->GetIsVisible(tab_id));

  ExtensionTestMessageListener ready_split("second run ready (split)");

  // Check incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ready_split.WaitUntilSatisfied());
  content::WebContents* const incognito_tab =
      incognito_browser->tab_strip_model()->GetWebContentsAt(0);
  const int incognito_tab_id = ExtensionTabUtil::GetTabId(incognito_tab);

  ChromeExtensionTestNotificationObserver incognito_test_observer(
      incognito_browser);

  const ExtensionAction* incognito_action =
      ExtensionActionManager::Get(incognito_browser->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(incognito_action);

  EXPECT_FALSE(NavigateInRenderer(incognito_tab, GURL("http://test_split/")));
  incognito_test_observer.WaitForPageActionVisibilityChangeTo(1);
  EXPECT_TRUE(incognito_action->GetIsVisible(incognito_tab_id));

  EXPECT_FALSE(NavigateInRenderer(incognito_tab, GURL("http://test_normal/")));
  incognito_test_observer.WaitForPageActionVisibilityChangeTo(0);
  EXPECT_FALSE(incognito_action->GetIsVisible(incognito_tab_id));
}

// http://crbug.com/304373
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       UninstallWhileActivePageAction) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  std::string script =
      kBackgroundHelpers + std::string("\nchrome.test.sendMessage('ready');");
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), script);
  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  const std::string extension_id = extension->id();
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  const std::string kTestRule =
      R"(setRules([{
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test'}})],
           actions: [new ShowAction()]
         }], 'test_rule');)";
  EXPECT_EQ("test_rule",
            ExecuteScriptInBackgroundPage(extension_id, kTestRule));

  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // TODO(crbug.com/40764017): Understand why these are EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test/")));

  EXPECT_TRUE(action->GetIsVisible(tab_id));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  EXPECT_EQ(1u, extension_action_test_util::GetVisiblePageActionCount(tab));
  EXPECT_EQ(1u, extension_action_test_util::GetTotalPageActionCount(tab));

  ExtensionTestMessageListener reload_ready_listener("ready");
  ReloadExtension(extension_id);  // Invalidates action and extension.
  ASSERT_TRUE(reload_ready_listener.WaitUntilSatisfied());

  EXPECT_EQ("test_rule",
            ExecuteScriptInBackgroundPage(extension_id, kTestRule));
  // TODO(jyasskin): Apply new rules to existing tabs, without waiting for a
  // navigation.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test/")));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(1));
  EXPECT_EQ(1u, extension_action_test_util::GetVisiblePageActionCount(tab));
  EXPECT_EQ(1u, extension_action_test_util::GetTotalPageActionCount(tab));

  UnloadExtension(extension_id);
  // Wait for declarative rules to be removed.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://test/")));
  EXPECT_TRUE(WaitForPageActionVisibilityChangeTo(0));
  EXPECT_EQ(0u, extension_action_test_util::GetVisiblePageActionCount(tab));
  EXPECT_EQ(0u, extension_action_test_util::GetTotalPageActionCount(tab));
}

// This tests against a renderer crash that was present during development.
// TODO(crbug.com/40260777): Convert this to run in both modes.
IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       AddExtensionMatchingExistingTabWithDeadFrames) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  ASSERT_TRUE(content::ExecJs(
      tab, R"(document.body.innerHTML = '<iframe src="http://test2">';)"));
  // Replace the iframe to destroy its WebFrame.
  ASSERT_TRUE(content::ExecJs(
      tab, R"(document.body.innerHTML = '<span class="foo">';)"));

  // Observer to track page action visibility. This helps avoid flakes by
  // waiting to check visibility until there is an update to the page action.
  ChromeExtensionTestNotificationObserver test_observer(browser());

  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_FALSE(action->GetIsVisible(tab_id));

  static constexpr char kRuleScript[] =
      R"(setRules([{
           conditions: [new PageStateMatcher({css: ["span[class=foo]"]})],
           actions: [new ShowAction()]
         }], 'rule0');)";
  EXPECT_EQ("rule0",
            ExecuteScriptInBackgroundPage(extension->id(), kRuleScript));

  test_observer.WaitForPageActionVisibilityChangeTo(1);
  EXPECT_FALSE(tab->IsCrashed());
  EXPECT_TRUE(action->GetIsVisible(tab_id))
      << "Loading an extension when an open page matches its rules "
      << "should show the page action.";

  static constexpr char kRemoveScript[] =
      R"(onPageChanged.removeRules(undefined, function() {
           chrome.test.sendScriptResult('removed');
         });)";
  EXPECT_EQ("removed",
            ExecuteScriptInBackgroundPage(extension->id(), kRemoveScript));
  test_observer.WaitForPageActionVisibilityChangeTo(0);
  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

// TODO(crbug.com/40260777): Convert this to run in both modes.
IN_PROC_BROWSER_TEST_F(DeclarativeContentApiTest,
                       CanonicalizesPageStateMatcherCss) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
         function Return(obj) {
            chrome.test.sendScriptResult('' + obj);
         })");
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);

  static constexpr char kValidSelectorScript[] =
      R"(var psm = new PageStateMatcher({css: ["input[type='password']"]});
         Return(psm.css);)";
  EXPECT_EQ(
      R"(input[type="password"])",
      ExecuteScriptInBackgroundPage(extension->id(), kValidSelectorScript));

  static constexpr char kSelectorNotAnArrayScript[] =
      R"(try {
           new PageStateMatcher({css: 'Not-an-array'});
           Return('Failed to throw');
         } catch (e) {
           Return(e.message);
         })";
  base::Value result =
      ExecuteScriptInBackgroundPage(extension->id(), kSelectorNotAnArrayScript);
  ASSERT_TRUE(result.is_string());
  EXPECT_THAT(result.GetString(),
              testing::ContainsRegex("css.*xpected '?array'?"));

  // CSS selector is not a string.
  static constexpr char kSelectorNotStringScript[] =
      R"(try {
           new PageStateMatcher({css: [null]});
           Return('Failed to throw');
         } catch (e) {
           Return(e.message);
         })";
  result =
      ExecuteScriptInBackgroundPage(extension->id(), kSelectorNotStringScript);
  ASSERT_TRUE(result.is_string());
  EXPECT_THAT(result.GetString(),
              testing::ContainsRegex("css.*0.*xpected '?string'?"));

  // Invalid CSS selector.
  static constexpr char kInvalidSelectorScript[] =
      R"(try {
           new PageStateMatcher({css: ["input''"]});
           Return('Failed to throw');
         } catch (e) {
           Return(e.message);
         })";
  result =
      ExecuteScriptInBackgroundPage(extension->id(), kInvalidSelectorScript);
  ASSERT_TRUE(result.is_string());
  EXPECT_THAT(result.GetString(), testing::ContainsRegex("valid.*: input''$"));

  // "Complex" CSS selector.
  static constexpr char kComplexSelectorScript[] =
      R"(try {
           new PageStateMatcher({css: ['div input']});
           Return('Failed to throw');
         } catch (e) {
           Return(e.message);
         })";
  result =
      ExecuteScriptInBackgroundPage(extension->id(), kComplexSelectorScript);
  ASSERT_TRUE(result.is_string());
  EXPECT_THAT(result.GetString(),
              testing::ContainsRegex("selector.*: div input$"));
}

// Tests that the rules with isBookmarked: true are evaluated when handling
// bookmarking events.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       IsBookmarkedRulesEvaluatedOnBookmarkEvents) {
  CheckBookmarkEvents(true);
}

// Tests that the rules with isBookmarked: false are evaluated when handling
// bookmarking events.
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       NotBookmarkedRulesEvaluatedOnBookmarkEvents) {
  CheckBookmarkEvents(false);
}

// https://crbug.com/497586
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       WebContentsWithoutTabAddedNotificationAtOnLoaded) {
  // Add a web contents to the tab strip in a way that doesn't trigger
  // NOTIFICATION_TAB_ADDED.
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), false);

  // The actual extension contents don't matter here -- we're just looking to
  // trigger OnExtensionLoaded.
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  ASSERT_TRUE(LoadExtension(ext_dir_.UnpackedPath()));
}

// https://crbug.com/501225
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       PendingWebContentsClearedOnRemoveRules) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  // Create two tabs.
  content::WebContents* const tab1 =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  ASSERT_FALSE(
      AddTabAtIndex(1, GURL("http://test2/"), ui::PAGE_TRANSITION_LINK));
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Add a rule matching the second tab.
  const std::string kAddTestRules =
      R"(addRules([{
           id: '1',
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test1'}})],
           actions: [new ShowAction()]
         }, {
           id: '2',
           conditions: [new PageStateMatcher({pageUrl: {hostPrefix: 'test2'}})],
           actions: [new ShowAction()]
         }], 'add_rules');)";
  EXPECT_EQ("add_rules",
            ExecuteScriptInBackgroundPage(extension->id(), kAddTestRules));
  EXPECT_TRUE(action->GetIsVisible(ExtensionTabUtil::GetTabId(tab2)));

  // Remove the rule.
  const std::string kRemoveTestRule1 = "removeRule('2', 'remove_rule1');";
  EXPECT_EQ("remove_rule1",
            ExecuteScriptInBackgroundPage(extension->id(), kRemoveTestRule1));

  // Remove the second tab, then trigger a rule evaluation for the remaining
  // tab.
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(tab2));
  // TODO(crbug.com/40764017): Understand why this is EXPECT_FALSE.
  EXPECT_FALSE(NavigateInRenderer(tab1, GURL("http://test1/")));
  EXPECT_TRUE(action->GetIsVisible(ExtensionTabUtil::GetTabId(tab1)));
}

// https://crbug.com/517492
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       RemoveAllRulesAfterExtensionUninstall) {
  ext_dir_.WriteManifest(FormatManifest(SPANNING));
  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundHelpers);

  // Load the extension, add a rule, then uninstall the extension.
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);

  const std::string kAddTestRule =
      R"(addRules([{
           id: '1',
           conditions: [],
           actions: [new ShowAction()]
         }], 'add_rule');)";
  EXPECT_EQ("add_rule",
            ExecuteScriptInBackgroundPage(extension->id(), kAddTestRule));

  ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  std::u16string error;
  ASSERT_TRUE(extension_service->UninstallExtension(
      extension->id(),
      UNINSTALL_REASON_FOR_TESTING,
      &error));
  ASSERT_EQ(u"", error);

  // Reload the extension, then add and remove a rule.
  extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);

  EXPECT_EQ("add_rule",
            ExecuteScriptInBackgroundPage(extension->id(), kAddTestRule));

  const std::string kRemoveTestRule1 = "removeRule('1', 'remove_rule1');";
  EXPECT_EQ("remove_rule1",
            ExecuteScriptInBackgroundPage(extension->id(), kRemoveTestRule1));
}

// Test that an extension with a RequestContentScript rule in its manifest can
// be loaded.
// Regression for crbug.com/1211316, which could cause this test to flake if
// RulesRegistry::OnExtensionLoaded() was called before
// UserScriptManager::OnExtensionLoaded().
IN_PROC_BROWSER_TEST_P(DeclarativeContentApiTestWithContextType,
                       RequestContentScriptRule) {
  constexpr char kManifest[] = R"(
      {
        "name": "Declarative Content apitest",
        "version": "0.1",
        "manifest_version": 2,
        "background": {
          "scripts": ["background.js"],
          "persistent": true
        },
        "page_action": {},
        "permissions": [
          "declarativeContent"
        ],
        "event_rules": [{
          "event": "declarativeContent.onPageChanged",
          "actions": [{
            "type": "declarativeContent.RequestContentScript",
            "js": ["asdf.js"]
          }],
          "conditions": [{
            "type": "declarativeContent.PageStateMatcher",
            "pageUrl": {"hostPrefix": "test1"}
          }]
        }]
      }
  )";

  ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  ext_dir_.WriteManifest(kManifest);
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);

  RulesRegistryService* rules_registry_service =
      extensions::RulesRegistryService::Get(browser()->profile());
  scoped_refptr<RulesRegistry> rules_registry =
      rules_registry_service->GetRulesRegistry(
          RulesRegistryService::kDefaultRulesRegistryID,
          "declarativeContent.onPageChanged");
  DCHECK(rules_registry);

  std::vector<const api::events::Rule*> rules;
  rules_registry->GetAllRules(extension->id(), &rules);
  EXPECT_EQ(1u, rules.size());
}

// TODO(wittman): Once ChromeContentRulesRegistry operates on condition and
// action interfaces, add a test that checks that a navigation always evaluates
// consistent URL state for all conditions. i.e.: if condition1 evaluates to
// false on url0 and true on url1, and condition2 evaluates to true on url0 and
// false on url1, navigate from url0 to url1 and validate that no action is
// triggered. Do the same when navigating back to url0. This kind of test is
// unfortunately not feasible with the current implementation and the existing
// supported conditions and actions.

}  // namespace
}  // namespace extensions
