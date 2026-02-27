// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <array>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_closed_waiter.h"
#include "chrome/test/base/browser_created_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/mock_data_type_worker.h"
#include "components/sync_sessions/session_store.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/test/test_support_jni_headers/TabWindowManagerNativeTestSupport_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace utils = api_test_utils;

namespace {

const char kTestCacheGuid[] = "TestCacheGuid";
// Fake session tabs (used to construct arbitrary device info) and tab IDs
// (used to construct arbitrary tab info) to use in all tests.
constexpr std::array kSessionTags = {"tag0", "tag1", "tag2", "tag3", "tag4"};
constexpr auto kTabIDs = std::to_array<SessionID::id_type>({5, 10, 13, 17});
constexpr int kActiveTabIndex = 2;
constexpr int kActiveTabId = kTabIDs[kActiveTabIndex];

// Helps with line wrapping.
int NowSeconds() {
  return base::Time::Now().ToTimeT();
}

void BuildSessionSpecifics(const std::string& tag,
                           sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(tag);
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  header->set_client_name(tag);
}

void BuildWindowSpecifics(int window_id,
                          const std::vector<int>& tab_list,
                          sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  window->set_window_id(window_id);
  window->set_selected_tab_index(kActiveTabIndex);
  window->set_browser_type(sync_pb::SyncEnums_BrowserType_TYPE_TABBED);
  for (int tab : tab_list) {
    window->add_tab(tab);
  }
}

void BuildTabSpecifics(const std::string& tag,
                       SessionID::id_type tab_id,
                       int tab_node_id,
                       sync_pb::SessionSpecifics* tab_base) {
  tab_base->set_session_tag(tag);
  tab_base->set_tab_node_id(tab_node_id);
  sync_pb::SessionTab* tab = tab_base->mutable_tab();
  tab->set_tab_id(tab_id);
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_virtual_url("http://foo/1");
  navigation->set_favicon_url("http://foo/favicon.ico");
  navigation->set_referrer("MyReferrer");
  navigation->set_title("MyTitle");
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
}

testing::AssertionResult CheckSessionModels(const base::ListValue& devices,
                                            size_t num_sessions) {
  EXPECT_EQ(5u, devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const base::Value& device_value = devices[i];
    EXPECT_TRUE(device_value.is_dict());
    const base::DictValue device = utils::ToDict(device_value);
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "info"));
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "deviceName"));
    const base::ListValue sessions =
        api_test_utils::GetList(device, "sessions");
    EXPECT_EQ(num_sessions, sessions.size());
    // Because this test is hurried, really there are only ever 0 or 1
    // sessions, and if 1, that will be a Window. Grab it.
    if (num_sessions == 0) {
      continue;
    }
    const base::DictValue session = utils::ToDict(sessions[0]);
    const base::DictValue window = api_test_utils::GetDict(session, "window");
    // Only the tabs are interesting.
    const base::ListValue tabs = api_test_utils::GetList(window, "tabs");
    EXPECT_EQ(std::size(kTabIDs), tabs.size());
    for (size_t j = 0; j < tabs.size(); ++j) {
      const base::DictValue tab = utils::ToDict(tabs[j]);
      EXPECT_FALSE(tab.contains("id"));  // sessions API does not give tab IDs
      EXPECT_EQ(static_cast<int>(j), api_test_utils::GetInteger(tab, "index"));
      EXPECT_EQ(0, api_test_utils::GetInteger(tab, "windowId"));
      // Selected/highlighted tabs should always be false.
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "selected"));
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "highlighted"));
      EXPECT_FALSE(api_test_utils::GetBoolean(tab, "incognito"));
      EXPECT_TRUE(api_test_utils::GetBoolean(tab, "pinned"));
      EXPECT_EQ("http://foo/1", api_test_utils::GetString(tab, "url"));
      EXPECT_EQ("MyTitle", api_test_utils::GetString(tab, "title"));
      EXPECT_EQ("http://foo/favicon.ico",
                api_test_utils::GetString(tab, "favIconUrl"));
      EXPECT_EQ(base::StringPrintf("%s.%d", kSessionTags[i], kTabIDs[j]),
                api_test_utils::GetString(tab, "sessionId"));
    }
  }
  return testing::AssertionSuccess();
}

syncer::ClientTagHash TagHashFromSpecifics(
    const sync_pb::SessionSpecifics& specifics) {
  return syncer::ClientTagHash::FromUnhashed(
      syncer::SESSIONS, sync_sessions::SessionStore::GetClientTag(specifics));
}

// Closes a browser window and waits for it to finish closing.
void CloseWindowAndWait(BrowserWindowInterface* window) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, headless TabModelSelector creation happens after
  // OnBrowserClosed(), so grab the ID here to check later.
  JNIEnv* env = base::android::AttachCurrentThread();
  TabListInterface* tab_list = TabListInterface::From(window);
  ASSERT_TRUE(tab_list);
  TabModel* tab_model = static_cast<TabModel*>(tab_list);
  int window_id = Java_TabWindowManagerNativeTestSupport_getWindowIdForModel(
      env, tab_model->GetJavaObject());
  ASSERT_NE(window_id, -1);
#endif

  // Close the window and wait for OnBrowserClosed() notification.
  BrowserClosedWaiter waiter(window);
  window->GetWindow()->Close();
  waiter.Wait();

#if BUILDFLAG(IS_ANDROID)
  // On Android we have to wait for the headless TabModelSelector to be created.
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  Java_TabWindowManagerNativeTestSupport_waitForTabModelSelectorWithId(
      env, window_id,
      base::android::ToJniCallback(env, std::move(quit_closure)));
  run_loop.Run();
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

class ExtensionSessionsTest : public ExtensionBrowserTest {
 public:
  ExtensionSessionsTest();
  ~ExtensionSessionsTest() override = default;

  // ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  void CreateTestExtension();
  void CreateSessionModels();

  template <class T>
  scoped_refptr<T> CreateFunction(bool has_callback) {
    scoped_refptr<T> fn(new T());
    fn->set_extension(extension_.get());
    fn->set_has_callback(has_callback);
    return fn;
  }

#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_ANDROID)

  scoped_refptr<const Extension> extension_;
};

ExtensionSessionsTest::ExtensionSessionsTest() {
#if BUILDFLAG(IS_ANDROID)
  // kRecentlyClosedTabsAndWindows is required for Java-side window restore.
  // kLoadAllTabsAtStartup is required to force WebContents for tabs not to be
  // null, see browser_extension_window_controller.cc.
  feature_list_.InitWithFeatures(
      {chrome::android::kRecentlyClosedTabsAndWindows,
       chrome::android::kLoadAllTabsAtStartup},
      {});
#endif  // BUILDFLAG(IS_ANDROID)
}

void ExtensionSessionsTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS)
  command_line->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void ExtensionSessionsTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  CreateTestExtension();
}

void ExtensionSessionsTest::CreateTestExtension() {
  extension_ = ExtensionBuilder("Test")
                   .AddAPIPermissions({"sessions", "tabs"})
                   .SetLocation(mojom::ManifestLocation::kInternal)
                   .Build();
}

void ExtensionSessionsTest::CreateSessionModels() {
  syncer::DataTypeActivationRequest request;
  request.error_handler = base::DoNothing();
  request.cache_guid = kTestCacheGuid;
  request.authenticated_gaia_id = GaiaId("SomeGaiaId");

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(GetProfile());

  base::test::TestFuture<std::unique_ptr<syncer::DataTypeActivationResponse>>
      sync_start_future;
  service->GetControllerDelegate()->OnSyncStarting(
      request, sync_start_future.GetCallback());
  std::unique_ptr<syncer::MockDataTypeWorker> worker =
      syncer::MockDataTypeWorker::CreateWorkerAndConnectSync(
          sync_start_future.Take());

  const base::Time time_now = base::Time::Now();
  syncer::SyncDataList initial_data;
  for (size_t index = 0; index < std::size(kSessionTags); ++index) {
    // Fill an instance of session specifics with a foreign session's data.
    sync_pb::EntitySpecifics header_entity;
    BuildSessionSpecifics(kSessionTags[index], header_entity.mutable_session());
    std::vector<SessionID::id_type> tab_list = base::ToVector(kTabIDs);
    BuildWindowSpecifics(index, tab_list, header_entity.mutable_session());
    std::vector<sync_pb::SessionSpecifics> tabs(tab_list.size());
    for (size_t i = 0; i < tab_list.size(); ++i) {
      BuildTabSpecifics(kSessionTags[index], tab_list[i], /*tab_node_id=*/i,
                        &tabs[i]);
    }

    // We need to provide a recent timestamp to prevent garbage collection of
    // sessions (anything older than 14 days), so we cannot use
    // MockDataTypeWorker's convenience functions, which internally use very
    // old timestamps.
    syncer::EntityData header_entity_data;
    header_entity_data.client_tag_hash =
        TagHashFromSpecifics(header_entity.session());
    header_entity_data.id =
        "FakeId:" + header_entity_data.client_tag_hash.value();
    header_entity_data.specifics = header_entity;
    header_entity_data.creation_time = time_now - base::Seconds(index);
    header_entity_data.modification_time = header_entity_data.creation_time;

    syncer::UpdateResponseData header_update;
    header_update.entity = std::move(header_entity_data);
    header_update.response_version = 1;
    syncer::UpdateResponseDataList updates;
    updates.push_back(std::move(header_update));
    worker->UpdateFromServer(std::move(updates));

    for (const auto& tab : tabs) {
      sync_pb::EntitySpecifics tab_entity;
      *tab_entity.mutable_session() = tab;
      worker->UpdateFromServer(TagHashFromSpecifics(tab_entity.session()),
                               tab_entity);
    }
  }

  // Let the processor receive and honor all updates, which requires running
  // the runloop because there is a DataTypeProcessorProxy in between, posting
  // tasks.
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevices) {
  CreateSessionModels();
  base::ListValue result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[{\"maxResults\": 0}]", GetProfile()));
  EXPECT_TRUE(CheckSessionModels(result, 0u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesMaxResults) {
  CreateSessionModels();
  base::ListValue result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          GetProfile()));
  EXPECT_TRUE(CheckSessionModels(result, 1u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesListEmpty) {
  base::ListValue devices(utils::ToList(utils::RunFunctionAndReturnSingleResult(
      CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
      GetProfile())));

  EXPECT_TRUE(devices.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreMostRecentlyClosedWindow) {
  // Open a second window.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);

  // Ensure 2 tabs exist.
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list2);
  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab.
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_EQ(2, tab_list2->GetTabCount());

  // Pin and activate the first tab so its metadata has non-default values.
  tabs::TabHandle tab_handle = tab_list2->GetTab(0)->GetHandle();
  tab_list2->PinTab(tab_handle);
  tab_list2->ActivateTab(tab_handle);
  ASSERT_TRUE(tab_list2->GetTab(0)->IsPinned());
  ASSERT_TRUE(tab_list2->GetTab(0)->IsActivated());

  // Navigate the tabs, otherwise window close does not persist it in the tab
  // restore service.
  content::WebContents* contents0 = tab_list2->GetTab(0)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents0, GURL("chrome://version/")));
  content::WebContents* contents1 = tab_list2->GetTab(1)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents1, GURL("chrome://credits/")));

  // Close the second window and wait for it to close.
  CloseWindowAndWait(browser2);

  // Get ready for a browser to be created.
  BrowserCreatedWaiter browser_waiter;

  // Run chrome.sessions.restore() with no arguments.
  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      CreateFunction<SessionsRestoreFunction>(true).get(), "[]", GetProfile());

  // The result is a session dictionary.
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  const base::DictValue* session_dict = result->GetIfDict();
  ASSERT_TRUE(session_dict);

  // The session contains a window.
  const base::DictValue* window_dict = session_dict->FindDict("window");
  ASSERT_TRUE(window_dict) << "Window information is missing from the session.";

  // The window contains 2 tabs.
  const base::ListValue* tabs = window_dict->FindList("tabs");
  ASSERT_TRUE(tabs);
  EXPECT_EQ(2u, tabs->size());

  const base::DictValue* tab0 = (*tabs)[0].GetIfDict();
  const base::DictValue* tab1 = (*tabs)[1].GetIfDict();
  ASSERT_TRUE(tab0);
  ASSERT_TRUE(tab1);
#if !BUILDFLAG(IS_ANDROID)
  // The tab URLs are chrome://version/ and chrome://credits/.
  // NOTE: On Android, the tabs are still navigating when the return value of
  // the API function is computed, so the "committed" URLs used by the API are
  // not available. However, we verify the loading URLs below in the test.
  EXPECT_EQ("chrome://version/", api_test_utils::GetString(*tab0, "url"));
  EXPECT_EQ("chrome://credits/", api_test_utils::GetString(*tab1, "url"));
#endif  // BUILDFLAG(IS_ANDROID)

  // The first tab is pinned and active.
  EXPECT_TRUE(api_test_utils::GetBoolean(*tab0, "pinned"));
  EXPECT_TRUE(api_test_utils::GetBoolean(*tab0, "active"));

  // The second tab is not pinned and not active.
  EXPECT_FALSE(api_test_utils::GetBoolean(*tab1, "pinned"));
  EXPECT_FALSE(api_test_utils::GetBoolean(*tab1, "active"));

  // Wait for the browser to be created (it may be asynchronous).
  BrowserWindowInterface* browser3 = browser_waiter.Wait();
  ASSERT_TRUE(browser3);

  // The restored browser has tabs at chrome://version/ and chrome://credits/.
  auto* tab_list3 = TabListInterface::From(browser3);
  ASSERT_TRUE(tab_list3);
  ASSERT_EQ(2, tab_list3->GetTabCount());
  EXPECT_EQ(GURL("chrome://version/"),
            tab_list3->GetTab(0)->GetContents()->GetVisibleURL());
  EXPECT_EQ(GURL("chrome://credits/"),
            tab_list3->GetTab(1)->GetContents()->GetVisibleURL());

  // The first tab is pinned and active.
  EXPECT_TRUE(tab_list3->GetTab(0)->IsPinned());
  EXPECT_TRUE(tab_list3->GetTab(0)->IsActivated());

  // The second tab is not pinned and not active.
  EXPECT_FALSE(tab_list3->GetTab(1)->IsPinned());
  EXPECT_FALSE(tab_list3->GetTab(1)->IsActivated());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreWindowBySessionId) {
  // Open a second window.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);

  // Ensure a tab exists.
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list2);
  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab.
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  ASSERT_EQ(1, tab_list2->GetTabCount());

  // Navigate the tabs, otherwise window close does not persist it in the tab
  // restore service.
  content::WebContents* contents0 = tab_list2->GetTab(0)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents0, GURL("chrome://version/")));

  // Close the second window and wait for it to close.
  CloseWindowAndWait(browser2);

  // chrome.sessions.getRecentlyClosed() should return 1 entry.
  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
      GetProfile());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1u, result->GetList().size());

  // The entry should be a session dict.
  const base::DictValue* session_dict = result->GetList()[0].GetIfDict();
  ASSERT_TRUE(session_dict);

  // The session contains a window dict.
  const base::DictValue* window_dict = session_dict->FindDict("window");
  ASSERT_TRUE(window_dict);

  // The window contains a session ID.
  std::string session_id = api_test_utils::GetString(*window_dict, "sessionId");
  ASSERT_TRUE(!session_id.empty());

  // Get ready for a browser to be created.
  BrowserCreatedWaiter browser_waiter;

  // Run chrome.sessions.restore() with the session id.
  std::string args = "[\"" + session_id + "\"]";
  result = utils::RunFunctionAndReturnSingleResult(
      CreateFunction<SessionsRestoreFunction>(true).get(), args, GetProfile());

  // Wait for the browser to be created (it may be asynchronous).
  BrowserWindowInterface* browser3 = browser_waiter.Wait();
  ASSERT_TRUE(browser3);

  // The restored browser has a tab at chrome://version/.
  auto* tab_list3 = TabListInterface::From(browser3);
  ASSERT_TRUE(tab_list3);
  ASSERT_EQ(1, tab_list3->GetTabCount());
  EXPECT_EQ(GURL("chrome://version/"),
            tab_list3->GetTab(0)->GetContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionWindow) {
  CreateSessionModels();

  const base::DictValue restored_window_session =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"tag3.3\"]",
          GetProfile(), api_test_utils::FunctionMode::kIncognito));

  base::ListValue windows(utils::ToList(utils::RunFunctionAndReturnSingleResult(
      CreateFunction<WindowsGetAllFunction>(true).get(), "[]", GetProfile())));

  EXPECT_EQ(2u, windows.size());
  const base::DictValue restored_window =
      api_test_utils::GetDict(restored_window_session, "window");
  base::DictValue window;
  int restored_id = api_test_utils::GetInteger(restored_window, "id");
  for (base::Value& window_value : windows) {
    window = utils::ToDict(std::move(window_value));
    if (api_test_utils::GetInteger(window, "id") == restored_id) {
      break;
    }
  }
  EXPECT_EQ(restored_id, api_test_utils::GetInteger(window, "id"));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionInvalidId) {
  CreateSessionModels();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"tag3.0\"]",
          GetProfile()),
      "Invalid session id: \"tag3.0\"."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreInIncognito) {
  CreateSessionModels();

  std::string error = utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(), "[\"1\"]",
      CreateIncognitoBrowserWindow()->GetProfile());
  EXPECT_TRUE(
      base::MatchPattern(error, "Can not restore sessions in incognito mode."))
      << error;
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreNonEditableTabstrip) {
  CreateSessionModels();

  // Disable tab strip editing, simulating a browser window in the midst of a
  // tab dragging session.
  base::AutoReset<bool> disable_tab_list_editing =
      ExtensionTabUtil::DisableTabListEditingForTesting();

  std::string error = utils::RunFunctionAndReturnError(
      CreateFunction<SessionsRestoreFunction>(true).get(), "[\"1\"]",
      GetProfile());
  EXPECT_TRUE(
      base::MatchPattern(error, ExtensionTabUtil::kTabStripNotEditableError))
      << error;
}

// Tests chrome.sessions.getRecentlyClosed() for windows. Opens a second browser
// window with two tabs, closes it, then calls the extension API function and
// verifies one window with two tabs was recently closed.
IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedWindow) {
  // Open a second window.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);

  // Ensure 2 tabs are created.
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list2);
  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab.
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_EQ(2, tab_list2->GetTabCount());

  // Pin and activate the first tab so its metadata has non-default values.
  tabs::TabHandle tab_handle = tab_list2->GetTab(0)->GetHandle();
  tab_list2->PinTab(tab_handle);
  tab_list2->ActivateTab(tab_handle);
  ASSERT_TRUE(tab_list2->GetTab(0)->IsPinned());
  ASSERT_TRUE(tab_list2->GetTab(0)->IsActivated());

  // Navigate each tab, otherwise window close does not persist them in the tab
  // restore service. Use different URLs.
  content::WebContents* contents0 = tab_list2->GetTab(0)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents0, GURL("chrome://version/")));
  content::WebContents* contents1 = tab_list2->GetTab(1)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents1, GURL("chrome://credits/")));

  // Close the second window and wait for it to close.
  CloseWindowAndWait(browser2);

  // NOTE: At this point persistent tab state may not yet be initialized on
  // Android. SessionsGetRecentlyClosedFunction copes with this by using a
  // Java-side callback that is not invoked until the window data is available.
  // Sleeping here or spinning the message loop would work around the issue,
  // but is the wrong solution.

  // chrome.sessions.getRecentlyClosed() should return 1 entry.
  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
      GetProfile());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_list());
  EXPECT_EQ(1u, result->GetList().size());

  // The entry should be a session dict.
  const base::DictValue* session_dict = result->GetList()[0].GetIfDict();
  ASSERT_TRUE(session_dict);

  // The session contains a window dict.
  const base::DictValue* window_dict = session_dict->FindDict("window");
  ASSERT_TRUE(window_dict) << "Window information is missing from the session.";

  // The window has a list of 2 tabs.
  const base::ListValue* tabs = window_dict->FindList("tabs");
  ASSERT_TRUE(tabs) << "Tabs information missing from the window dict.";
  EXPECT_EQ(2u, tabs->size());

  // The first URL is chrome://version/.
  const base::DictValue* tab0 = (*tabs)[0].GetIfDict();
  ASSERT_TRUE(tab0);
  EXPECT_EQ("chrome://version/", api_test_utils::GetString(*tab0, "url"));
  EXPECT_EQ("About Version", api_test_utils::GetString(*tab0, "title"));
  EXPECT_EQ(0, api_test_utils::GetInteger(*tab0, "index"));

  // The first tab is pinned and active.
  EXPECT_TRUE(api_test_utils::GetBoolean(*tab0, "pinned"));
  EXPECT_TRUE(api_test_utils::GetBoolean(*tab0, "active"));

  // The second URL is chrome://credits/.
  const base::DictValue* tab1 = (*tabs)[1].GetIfDict();
  ASSERT_TRUE(tab1);
  EXPECT_EQ("chrome://credits/", api_test_utils::GetString(*tab1, "url"));
  EXPECT_EQ("Credits", api_test_utils::GetString(*tab1, "title"));
  EXPECT_EQ(1, api_test_utils::GetInteger(*tab1, "index"));

  // The second tab is not pinned and not active.
  EXPECT_FALSE(api_test_utils::GetBoolean(*tab1, "pinned"));
  EXPECT_FALSE(api_test_utils::GetBoolean(*tab1, "active"));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedIncognito) {
  base::ListValue sessions(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
          CreateIncognitoBrowserWindow()->GetProfile())));
  EXPECT_TRUE(sessions.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedMaxResults) {
  // Start with an empty tab restore service.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);
  ASSERT_EQ(0u, service->entries().size());

  // The browser starts the test with 1 tab.
  ASSERT_EQ(1, GetTabCount());
  // Open 3 more tabs, closing each one after it is opened.
  const size_t kTabCount = 3;
  for (size_t i = 0; i < kTabCount; ++i) {
    // Open a new tab. This method automatically waits for load stop.
    NavigateToURLInNewTab(GURL("about:blank"));
    // Close the tab we just opened (at index 1).
    int tab_index = 1;
    content::WebContents* tab = GetWebContentsAt(tab_index);
    // On Android, historical tabs are automatically created when a tab is
    // closed via the HistoricalTabSaver; there is no need to manually create
    // them like on desktop.
#if !BUILDFLAG(IS_ANDROID)
    // Our cross-platform utility function to close a tab doesn't allow
    // requesting the creation of a historical tab, so do it manually.
    service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetOrCreateForWebContents(tab), tab_index);
#endif  // !BUILDFLAG(IS_ANDROID)
    // Close the tab (and wait for its destruction internally).
    CloseTabForWebContents(tab);
  }

  {
    // Querying for all recently closed tabs should return 3 tabs.
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
        GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(kTabCount, result->GetList().size());
  }
  {
    // Querying with a maxResults limit of 0 should return 0 tabs.
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
        "[{\"maxResults\": 0}]", GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(0u, result->GetList().size());
  }
  {
    // Querying with a maxResults limit of 2 should return 2 tabs.
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
        "[{\"maxResults\": 2}]", GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(2u, result->GetList().size());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest,
                       GetRecentlyClosedMaxResultsWithWindow) {
  // Start with an empty tab restore service.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);
  ASSERT_EQ(0u, service->entries().size());

  // The main browser starts the test with 1 tab.
  ASSERT_EQ(1, GetTabCount());
  // Open one more tab.
  NavigateToURLInNewTab(GURL("about:blank"));

  // Prepare to close the tab we just opened (at index 1).
  const int tab_index = 1;
  content::WebContents* tab = GetWebContentsAt(tab_index);

  // On Android, historical tabs are automatically created when a tab is
  // closed via the HistoricalTabSaver; there is no need to manually create
  // them like on desktop.
#if !BUILDFLAG(IS_ANDROID)
  // Our cross-platform utility function to close a tab doesn't allow
  // requesting the creation of a historical tab, so do it manually.
  service->CreateHistoricalTab(
      sessions::ContentLiveTab::GetOrCreateForWebContents(tab), tab_index);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Close the tab (and wait for its destruction internally).
  CloseTabForWebContents(tab);

  // Open a second window.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);

  // Ensure 1 tab exists.
  auto* tab_list2 = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list2);

  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab.
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  ASSERT_EQ(1, tab_list2->GetTabCount());

  // Navigate the tab, otherwise window close does not persist it in the tab
  // restore service.
  content::WebContents* contents0 = tab_list2->GetTab(0)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents0, GURL("chrome://version/")));

  // Close the second window and wait for it to close.
  CloseWindowAndWait(browser2);

  {
    // Querying for all recently closed entries should return 2 results, a
    // window and a tab. Ensure the window is recorded as being closed most
    // recently, as the timestamps under the hood only have second-level
    // precision, so the test can be flaky if time runs unmodified.
    auto function = CreateFunction<SessionsGetRecentlyClosedFunction>(true);
    function->set_window_last_modified_for_test(NowSeconds() + 10);
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), "[]", GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(2u, result->GetList().size());

    const base::DictValue session0 = utils::ToDict(result->GetList()[0]);
    const base::DictValue window = api_test_utils::GetDict(session0, "window");
    EXPECT_FALSE(window.empty());
    const base::DictValue session1 = utils::ToDict(result->GetList()[1]);
    const base::DictValue tab_value = api_test_utils::GetDict(session1, "tab");
    EXPECT_FALSE(tab_value.empty());
  }
  {
    // Querying with a maxResults limit of 0 should return 0 results.
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
        "[{\"maxResults\": 0}]", GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(0u, result->GetList().size());
  }
  {
    // Querying with a maxResults limit of 1 should return 1 window and not the
    // tab. Historically there was a regression with this behavior on Android.
    auto function = CreateFunction<SessionsGetRecentlyClosedFunction>(true);
    function->set_window_last_modified_for_test(NowSeconds() + 10);
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), "[{\"maxResults\": 1}]", GetProfile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(1u, result->GetList().size());

    const base::DictValue session0 = utils::ToDict(result->GetList()[0]);
    const base::DictValue window = api_test_utils::GetDict(session0, "window");
    EXPECT_FALSE(window.empty());
  }
}

// http://crbug.com/40322238
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_SessionsApis) {
  ASSERT_TRUE(RunExtensionTest("sessions", {.extension_url = "sessions.html"}))
      << message_;
}

// Verify that the correct tab is active based on its tab ID (instead of relying
// on tab index, since the tabs list might be filtered or sorted).
IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, CheckActiveTabStatus) {
  CreateSessionModels();

  const base::ListValue result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          GetProfile()));

  ASSERT_FALSE(result.empty()) << "No devices found.";

  const base::DictValue* device_dict = result[0].GetIfDict();
  ASSERT_TRUE(device_dict) << "Failed to retrieve device information.";

  const base::ListValue* sessions = device_dict->FindList("sessions");
  ASSERT_TRUE(sessions) << "No sessions found in the device.";
  ASSERT_FALSE(sessions->empty()) << "No session data found in the device.";

  const base::DictValue* session_dict = (*sessions)[0].GetIfDict();
  ASSERT_TRUE(session_dict) << "Failed to retrieve session information.";

  const base::DictValue* window_dict = session_dict->FindDict("window");
  ASSERT_TRUE(window_dict) << "Window information is missing from the session.";

  const base::ListValue* tabs = window_dict->FindList("tabs");
  ASSERT_TRUE(tabs) << "No tabs found in the session window.";
  ASSERT_FALSE(tabs->empty()) << "Tabs list is empty.";

  const base::DictValue* active_tab = nullptr;
  for (const auto& tab_value : *tabs) {
    const base::DictValue* tab_dict = tab_value.GetIfDict();
    ASSERT_TRUE(tab_dict) << "Failed to retrieve tab information.";

    // Extract the sessionId as a string
    const std::string* session_id = tab_dict->FindString("sessionId");
    ASSERT_TRUE(session_id) << "Session ID is missing.";

    // Find the position of the '.' in the sessionId to separate the prefix and
    // tabId
    size_t dot_pos = session_id->find('.');
    ASSERT_TRUE(dot_pos != std::string::npos) << "Invalid sessionId format.";

    // Extract the tabId part from the sessionId (after the '.')
    std::string tab_id_str = session_id->substr(dot_pos + 1);
    // Convert string tabId to int using base::StringToInt
    int tab_id;
    ASSERT_TRUE(base::StringToInt(tab_id_str, &tab_id))
        << "Failed to convert tabId to int.";

    // Compare the extracted tabId with the activeTabId
    if (tab_id == kActiveTabId) {
      active_tab = tab_dict;
      break;
    }
  }

  ASSERT_TRUE(active_tab) << "Failed to retrieve active tab information.";

  std::optional<bool> tab_active_status = active_tab->FindBool("active");
  ASSERT_TRUE(tab_active_status) << "Active state for the tab is missing.";

  EXPECT_TRUE(*tab_active_status) << "The selected tab should be active.";
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, OnChangedEvent) {
  // Simulate a listener being added (otherwise events won't fire).
  EventListenerInfo info(api::sessions::OnChanged::kEventName, "extension_id",
                         GURL(), nullptr, profile());
  SessionsAPI::GetFactoryInstance()->Get(profile())->OnListenerAdded(info);

  // Open a second window.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);

  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab. Ensure there is one tab.
  auto* tab_list2 = TabListInterface::From(browser2);
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  ASSERT_EQ(1, tab_list2->GetTabCount());

  // Navigate the tab, otherwise window close does not persist it in the tab
  // restore service.
  content::WebContents* contents0 = tab_list2->GetTab(0)->GetContents();
  ASSERT_TRUE(NavigateToURL(contents0, GURL("chrome://version/")));

  // Listen for events.
  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  // Close the second window and wait for it to close.
  BrowserClosedWaiter waiter(browser2);
  browser2->GetWindow()->Close();
  waiter.Wait();

  // An OnChanged event should fire.
  event_observer.WaitForEventWithName(api::sessions::OnChanged::kEventName);
}

}  // namespace extensions
