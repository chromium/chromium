// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include "base/one_shot_event.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_manager.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

// A StateStore entry using the legacy format that relied on
// api::content_scripts::ContentScript and hand-modification.
constexpr char kOldFormatEntry[] =
    R"([{
         "all_frames": true,
         "exclude_matches": ["http://exclude.example/*"],
         "id": "_dc_foo",
         "js": ["script.js"],
         "match_origin_as_fallback": true,
         "matches": ["http://example.com/*"],
         "run_at": "document_end",
         "world":"ISOLATED"
       }])";

}  // namespace

class ExtensionUserScriptLoaderBrowserTest : public ExtensionApiTest {
 public:
  ExtensionUserScriptLoaderBrowserTest() = default;
  ~ExtensionUserScriptLoaderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  StateStore* dynamic_scripts_store() {
    return extension_system()->dynamic_user_scripts_store();
  }

  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(profile());
  }

  void FlushScriptStore() {
    base::RunLoop run_loop;
    dynamic_scripts_store()->FlushForTesting(run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  void WaitForSystemReady() {
    base::RunLoop run_loop;
    extension_system()->ready().Post(FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }
};

// This series of tests exercises that the migration we have in place for our
// serializations of user scripts works properly, preserving old records. It is
// split into three steps.
// TODO(crbug.com/40286091): We can remove this test once the migration
// is fully complete.
// Step 1: Load an extension and populate it with old-style data.
IN_PROC_BROWSER_TEST_F(ExtensionUserScriptLoaderBrowserTest,
                       PRE_PRE_OldDynamicContentScriptEntriesAreMigrated) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("scripting/dynamic_scripts_stub"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(dynamic_scripts_store());

  // We hard-code the entries in the state store, since writing them newly would
  // use the new format.
  dynamic_scripts_store()->SetExtensionValue(
      extension->id(), scripting::kRegisteredScriptsStorageKey,
      base::test::ParseJson(kOldFormatEntry));
  FlushScriptStore();
  URLPatternSet patterns;
  patterns.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "http://example.com/*"));
  scripting::SetPersistentScriptURLPatterns(profile(), extension->id(),
                                            std::move(patterns));
}

// Step 2: Restart the browser, and ensure the scripts are still appropriately
// registered.
IN_PROC_BROWSER_TEST_F(ExtensionUserScriptLoaderBrowserTest,
                       PRE_OldDynamicContentScriptEntriesAreMigrated) {
  WaitForSystemReady();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  EXPECT_EQ(u"script injected",
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  const Extension* extension = nullptr;
  for (const auto& entry :
       ExtensionRegistry::Get(profile())->enabled_extensions()) {
    if (entry->name() == "Dynamic Content Scripts Stub") {
      extension = entry.get();
      break;
    }
  }

  ASSERT_TRUE(extension);

  ExtensionUserScriptLoader* loader =
      extension_system()
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension->id());

  ASSERT_TRUE(loader);

  // We don't currently auto-migrate scripts. This means that to trigger the
  // update to the new type, we remove and re-add the scripts.
  const UserScriptList& loaded_dynamic_scripts =
      loader->GetLoadedDynamicScripts();
  ASSERT_EQ(1u, loaded_dynamic_scripts.size());
  UserScriptList copied_scripts;
  copied_scripts.push_back(
      UserScript::CopyMetadataFrom(*loaded_dynamic_scripts[0]));
  {
    base::test::TestFuture<const std::optional<std::string>&> future;
    loader->ClearDynamicScripts(UserScript::Source::kDynamicContentScript,
                                future.GetCallback());
    EXPECT_EQ("<no error>", future.Get().value_or("<no error>"));
  }
  {
    std::string id = copied_scripts[0]->id();
    base::test::TestFuture<const std::optional<std::string>&> future;
    loader->AddPendingDynamicScriptIDs({id});
    loader->AddDynamicScripts(std::move(copied_scripts), {id},
                              future.GetCallback());
    EXPECT_EQ("<no error>", future.Get().value_or("<no error>"));
  }

  EXPECT_EQ(1u, loader->GetLoadedDynamicScripts().size());

  // Verify as well that the serialized values are now migrated to the new type.
  FlushScriptStore();

  base::test::TestFuture<std::optional<base::Value>> value_future;
  dynamic_scripts_store()->GetExtensionValue(
      extension->id(), scripting::kRegisteredScriptsStorageKey,
      value_future.GetCallback());

  std::optional<base::Value> value = value_future.Take();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  ASSERT_EQ(1u, value->GetList().size());
  const base::Value& entry = value->GetList()[0];
  ASSERT_TRUE(entry.is_dict());

  // The presence (and validity) of a "source" entry are an indication that the
  // serialization is using the new type.
  const std::string* source_string = entry.GetDict().FindString("source");
  ASSERT_TRUE(source_string);
  EXPECT_EQ("DYNAMIC_CONTENT_SCRIPT", *source_string);
}

// Step 3: Restart the browser a third and final time. Scripts should still
// inject, having loaded from the new format.
IN_PROC_BROWSER_TEST_F(ExtensionUserScriptLoaderBrowserTest,
                       OldDynamicContentScriptEntriesAreMigrated) {
  WaitForSystemReady();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  EXPECT_EQ(u"script injected",
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

}  // namespace extensions
