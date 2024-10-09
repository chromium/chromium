// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
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
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

namespace extensions {

namespace utils = api_test_utils;

namespace {

const char kTestCacheGuid[] = "TestCacheGuid";
// Fake session tabs (used to construct arbitrary device info) and tab IDs
// (used to construct arbitrary tab info) to use in all tests.
const char* const kSessionTags[] = {"tag0", "tag1", "tag2", "tag3", "tag4"};
const SessionID::id_type kTabIDs[] = {5, 10, 13, 17};
const int kActiveTabIndex = 2;
const int kActiveTabId = kTabIDs[kActiveTabIndex];
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

testing::AssertionResult CheckSessionModels(const base::Value::List& devices,
                                            size_t num_sessions) {
  EXPECT_EQ(5u, devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const base::Value& device_value = devices[i];
    EXPECT_TRUE(device_value.is_dict());
    const base::Value::Dict device = utils::ToDict(device_value);
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "info"));
    EXPECT_EQ(kSessionTags[i], api_test_utils::GetString(device, "deviceName"));
    const base::Value::List sessions =
        api_test_utils::GetList(device, "sessions");
    EXPECT_EQ(num_sessions, sessions.size());
    // Because this test is hurried, really there are only ever 0 or 1
    // sessions, and if 1, that will be a Window. Grab it.
    if (num_sessions == 0)
      continue;
    const base::Value::Dict session = utils::ToDict(sessions[0]);
    const base::Value::Dict window = api_test_utils::GetDict(session, "window");
    // Only the tabs are interesting.
    const base::Value::List tabs = api_test_utils::GetList(window, "tabs");
    EXPECT_EQ(std::size(kTabIDs), tabs.size());
    for (size_t j = 0; j < tabs.size(); ++j) {
      const base::Value::Dict tab = utils::ToDict(tabs[j]);
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

}  // namespace

class ExtensionSessionsTest : public InProcessBrowserTest {
 public:
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

  scoped_refptr<const Extension> extension_;
};

void ExtensionSessionsTest::SetUpCommandLine(base::CommandLine* command_line) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  command_line->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void ExtensionSessionsTest::SetUpOnMainThread() {
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
  request.authenticated_account_id = CoreAccountId::FromGaiaId("SomeAccountId");

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(browser()->profile());

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
    std::vector<SessionID::id_type> tab_list(kTabIDs,
                                             kTabIDs + std::size(kTabIDs));
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
  base::Value::List result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(),
          "[{\"maxResults\": 0}]", browser()->profile()));
  EXPECT_TRUE(CheckSessionModels(result, 0u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesMaxResults) {
  CreateSessionModels();
  base::Value::List result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          browser()->profile()));
  EXPECT_TRUE(CheckSessionModels(result, 1u));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetDevicesListEmpty) {
  base::Value::List devices(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          browser()->profile())));

  EXPECT_TRUE(devices.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionWindow) {
  CreateSessionModels();

  const base::Value::Dict restored_window_session =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"tag3.3\"]",
          browser()->profile(), api_test_utils::FunctionMode::kIncognito));

  base::Value::List windows(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<WindowsGetAllFunction>(true).get(), "[]",
          browser()->profile())));

  EXPECT_EQ(2u, windows.size());
  const base::Value::Dict restored_window =
      api_test_utils::GetDict(restored_window_session, "window");
  base::Value::Dict window;
  int restored_id = api_test_utils::GetInteger(restored_window, "id");
  for (base::Value& window_value : windows) {
    window = utils::ToDict(std::move(window_value));
    if (api_test_utils::GetInteger(window, "id") == restored_id)
      break;
  }
  EXPECT_EQ(restored_id, api_test_utils::GetInteger(window, "id"));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreForeignSessionInvalidId) {
  CreateSessionModels();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"tag3.0\"]",
          browser()->profile()),
      "Invalid session id: \"tag3.0\"."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreInIncognito) {
  CreateSessionModels();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"1\"]",
          CreateIncognitoBrowser()->profile()),
      "Can not restore sessions in incognito mode."));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, RestoreNonEditableTabstrip) {
  CreateSessionModels();

  // Set up a browser with a non-editable tabstrip, simulating one in the midst
  // of a tab dragging session.
  std::unique_ptr<TestBrowserWindow> browser_window =
      std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(browser()->profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window.get();
  std::unique_ptr<Browser> browser =
      std::unique_ptr<Browser>(Browser::Create(params));
  browser_window->SetIsTabStripEditable(false);

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          CreateFunction<SessionsRestoreFunction>(true).get(), "[\"1\"]",
          browser->profile()),
      ExtensionTabUtil::kTabStripNotEditableError));
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedIncognito) {
  base::Value::List sessions(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
          CreateIncognitoBrowser()->profile())));
  EXPECT_TRUE(sessions.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, GetRecentlyClosedMaxResults) {
  const size_t kTabCount = 3;
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  for (size_t i = 0; i < kTabCount; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("data:text/html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    int tab_index = 1;
    content::WebContentsDestroyedWatcher destroyed_watcher(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
    browser()->tab_strip_model()->CloseWebContentsAt(
        tab_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }

  {
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(), "[]",
        browser()->profile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(kTabCount, result->GetList().size());
  }
  {
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
        "[{\"maxResults\": 0}]", browser()->profile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(0u, result->GetList().size());
  }
  {
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        CreateFunction<SessionsGetRecentlyClosedFunction>(true).get(),
        "[{\"maxResults\": 2}]", browser()->profile());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    EXPECT_EQ(2u, result->GetList().size());
  }
}

// http://crbug.com/251199
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_SessionsApis) {
  ASSERT_TRUE(RunExtensionTest("sessions", {.extension_url = "sessions.html"}))
      << message_;
}

// Verify that the correct tab is active based on its tab ID (instead of relying
// on tab index, since the tabs list might be filtered or sorted).
IN_PROC_BROWSER_TEST_F(ExtensionSessionsTest, CheckActiveTabStatus) {
  CreateSessionModels();

  const base::Value::List result =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          CreateFunction<SessionsGetDevicesFunction>(true).get(), "[]",
          browser()->profile()));

  ASSERT_FALSE(result.empty()) << "No devices found.";

  const base::Value::Dict* device_dict = result[0].GetIfDict();
  ASSERT_TRUE(device_dict) << "Failed to retrieve device information.";

  const base::Value::List* sessions = device_dict->FindList("sessions");
  ASSERT_TRUE(sessions) << "No sessions found in the device.";
  ASSERT_FALSE(sessions->empty()) << "No session data found in the device.";

  const base::Value::Dict* session_dict = (*sessions)[0].GetIfDict();
  ASSERT_TRUE(session_dict) << "Failed to retrieve session information.";

  const base::Value::Dict* window_dict = session_dict->FindDict("window");
  ASSERT_TRUE(window_dict) << "Window information is missing from the session.";

  const base::Value::List* tabs = window_dict->FindList("tabs");
  ASSERT_TRUE(tabs) << "No tabs found in the session window.";
  ASSERT_FALSE(tabs->empty()) << "Tabs list is empty.";

  const base::Value::Dict* active_tab = nullptr;
  for (const auto& tab_value : *tabs) {
    const base::Value::Dict* tab_dict = tab_value.GetIfDict();
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

}  // namespace extensions
