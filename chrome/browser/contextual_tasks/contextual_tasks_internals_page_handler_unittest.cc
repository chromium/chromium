// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#endif

namespace contextual_tasks {
namespace {

class FakeInternalsPage
    : public contextual_tasks_internals::mojom::ContextualTasksInternalsPage {
 public:
  // contextual_tasks_internals::mojom::ContextualTasksInternalsPage:
  void OnLogMessageAdded(base::Time event_time,
                         const std::string& source_file,
                         int64_t source_line,
                         const std::string& message) override {}
};

class ContextualTasksInternalsPageHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    web_contents_ = web_contents_factory_->CreateWebContents(profile_.get());

    mojo::PendingRemote<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage>
        page_remote;
    page_receiver_ = std::make_unique<mojo::Receiver<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage>>(
        &fake_page_, page_remote.InitWithNewPipeAndPassReceiver());

    handler_ = std::make_unique<ContextualTasksInternalsPageHandler>(
        profile_.get(), /*context_service=*/nullptr,
        /*optimization_guide_keyed_service=*/nullptr,
        handler_remote_.BindNewPipeAndPassReceiver(), std::move(page_remote));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  FakeInternalsPage fake_page_;
  std::unique_ptr<mojo::Receiver<
      contextual_tasks_internals::mojom::ContextualTasksInternalsPage>>
      page_receiver_;
  mojo::Remote<
      contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
      handler_remote_;
  std::unique_ptr<ContextualTasksInternalsPageHandler> handler_;
};

TEST_F(ContextualTasksInternalsPageHandlerTest,
       BuildWebContentsDebugState_UntrackedReturnsNull) {
  EXPECT_FALSE(handler_->BuildWebContentsDebugState(
      web_contents_, /*is_side_panel=*/false,
      /*is_active_tab_in_window=*/true));
}

TEST_F(ContextualTasksInternalsPageHandlerTest,
       BuildWebContentsDebugState_PopulatesUserDataAndInputState) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  base::Uuid task_id =
      base::Uuid::ParseLowercase("7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90");
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);

  user_data->set_pending_task_id(task_id);
  user_data->RecordTabIdMapping(/*tab_handle_id=*/21, /*session_tab_id=*/101);
  base::UnguessableToken upload_token = base::UnguessableToken::Create();
  user_data->OnTabContextUploadStarted(upload_token);

  omnibox::SearchboxConfig config;
  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *mock_handle, config, GURL("https://www.google.com/search?udm=50"),
      /*is_off_the_record=*/false, /*is_signed_in=*/true,
      /*browser_identity_matches_aim_identity=*/true);
  user_data->set_input_state_model(std::move(input_state_model));

  auto debug_state = handler_->BuildWebContentsDebugState(
      web_contents_, /*is_side_panel=*/true,
      /*is_active_tab_in_window=*/false);
  ASSERT_TRUE(debug_state);
  EXPECT_TRUE(debug_state->is_side_panel);
  EXPECT_FALSE(debug_state->is_active_tab_in_window);
  EXPECT_FALSE(debug_state->session_tab_id.has_value());
  EXPECT_FALSE(debug_state->tab_handle_id.has_value());

  ASSERT_TRUE(debug_state->user_data_state);
  const auto& ud = *debug_state->user_data_state;
  EXPECT_TRUE(ud.has_contextual_tasks_user_data);
  EXPECT_TRUE(ud.has_contextual_search_helper);
  EXPECT_EQ(ud.pending_task_id, task_id.AsLowercaseString());
  EXPECT_EQ(ud.effective_task_id, task_id.AsLowercaseString());
  EXPECT_EQ(ud.helper_task_id, task_id.AsLowercaseString());
  EXPECT_EQ(ud.last_sent_context_state, "kNone");
  ASSERT_EQ(ud.tab_id_mappings.size(), 1u);
  EXPECT_EQ(ud.tab_id_mappings[0]->tab_handle_id, 21);
  EXPECT_EQ(ud.tab_id_mappings[0]->session_tab_id, 101);
  ASSERT_EQ(ud.pending_context_upload_tokens.size(), 1u);
  EXPECT_EQ(ud.pending_context_upload_tokens[0], upload_token);

  ASSERT_TRUE(ud.input_state);
  EXPECT_FALSE(ud.input_state->has_valid_config);
  EXPECT_FALSE(ud.input_state->is_smart_tab_sharing_active);
}

TEST_F(ContextualTasksInternalsPageHandlerTest,
       BuildWebContentsDebugState_PopulatesSessionHandleState) {
  auto session_handle = std::make_unique<testing::NiceMock<
      contextual_search::MockContextualSearchSessionHandle>>();
  auto* mock_handle = session_handle.get();

  mock_handle->set_current_url(GURL("https://www.google.com/search?udm=50"));
  mock_handle->set_invocation_source(
      lens::LensOverlayInvocationSource::kCobrowseToolbarButton);
  mock_handle->set_is_contextual_lens_session(true);
  mock_handle->set_auth_user_index(2);
  mock_handle->set_smart_tab_sharing_active(true);
  mock_handle->set_smart_tab_sharing_toggled_since_last_turn(true);
  ON_CALL(*mock_handle, search_session_id())
      .WillByDefault(testing::Return("gsession_test_123"));

  base::UnguessableToken delayed_token = mock_handle->AddPendingDelayedTab(
      /*tab_id=*/105, GURL("https://chromium.org"), "Chromium");

  contextual_search::TabInfo restored_tab;
  restored_tab.tab_id = 109;
  restored_tab.url = GURL("https://source.chromium.org");
  restored_tab.title = "Chromium Code Search";
  restored_tab.restored_from_aim = true;
  mock_handle->SetRestoredTabs({restored_tab});

  mock_handle->set_deselected_tabs_urls(
      {{SessionID::FromSerializedValue(108),
        {GURL("https://mail.google.com"), "Gmail"}}});

  ThreadTurn turn;
  turn.query = "Compare Chromium Mojo patterns";
  mock_handle->AddThreadTurn(turn);

  contextual_search::FileInfo uploaded_file;
  uploaded_file.file_token = base::UnguessableToken::Create();
  uploaded_file.file_name = "design_doc.pdf";
  uploaded_file.file_size_bytes = 2048;
  uploaded_file.mime_type = lens::MimeType::kPdf;
  uploaded_file.mime_type_string = "application/pdf";
  uploaded_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  ON_CALL(*mock_handle, GetUploadedContextFileInfos())
      .WillByDefault(testing::Return(
          std::vector<contextual_search::FileInfo>{uploaded_file}));

  contextual_search::FileInfo submitted_file;
  submitted_file.file_token = base::UnguessableToken::Create();
  submitted_file.file_name = "screenshot.png";
  submitted_file.file_size_bytes = 4096;
  submitted_file.mime_type = lens::MimeType::kImage;
  submitted_file.mime_type_string = "image/png";
  submitted_file.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  ON_CALL(*mock_handle, GetSubmittedContextFileInfos())
      .WillByDefault(testing::Return(
          std::vector<contextual_search::FileInfo>{submitted_file}));

  base::Uuid task_id =
      base::Uuid::ParseLowercase("7f9c2b10-4d3a-4e8b-9a1c-0b2d4e6f8a90");
  auto* mock_tasks_service =
      static_cast<testing::NiceMock<MockContextualTasksService>*>(
          ContextualTasksServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile_.get(),
              base::BindRepeating([](content::BrowserContext*)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<
                    testing::NiceMock<MockContextualTasksService>>();
              })));
  ContextualTask resolved_task(task_id);
  resolved_task.AddThread(
      Thread(ThreadType::kAiMode, "srv_123", "Test Thread Title",
             /*last_turn_time_unix_epoch_millis=*/1000, "turn_456"));
  UrlResource resource(
      base::Uuid::ParseLowercase("11111111-2222-3333-4444-555555555555"),
      GURL("https://chromium.org/doc"));
  resource.tab_id = SessionID::FromSerializedValue(105);
  resource.title = "Chromium Doc";
  resource.context_id = 98765;
  resource.resource_type = ResourceType::kWebpage;
  resolved_task.AddUrlResource(resource);
  ON_CALL(*mock_tasks_service, GetTabsAssociatedWithTask(task_id))
      .WillByDefault(testing::Return(
          std::vector<SessionID>{SessionID::FromSerializedValue(105)}));
  ON_CALL(*mock_tasks_service,
          GetContextualTaskForTab(SessionID::FromSerializedValue(105)))
      .WillByDefault(testing::Return(resolved_task));

  ContextualTasksWebContentsUserData::CreateForWebContents(web_contents_);
  ContextualTasksWebContentsUserData::FromWebContents(web_contents_)
      ->set_pending_task_id(task_id);

  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task_id, std::move(session_handle),
                       /*input_state_model=*/nullptr);

  auto debug_state = handler_->BuildWebContentsDebugState(
      web_contents_, /*is_side_panel=*/true,
      /*is_active_tab_in_window=*/false);
  ASSERT_TRUE(debug_state);
  ASSERT_TRUE(debug_state->session_handle_state);

  const auto& session_handle_state = *debug_state->session_handle_state;
  EXPECT_EQ(session_handle_state.search_session_id, "gsession_test_123");
  EXPECT_EQ(session_handle_state.current_url,
            GURL("https://www.google.com/search?udm=50"));
  EXPECT_EQ(session_handle_state.invocation_source, "kCobrowseToolbarButton");
  EXPECT_TRUE(session_handle_state.is_contextual_lens_session);
  EXPECT_EQ(session_handle_state.auth_user_index, 2u);
  EXPECT_TRUE(session_handle_state.smart_tab_sharing_active);
  EXPECT_TRUE(session_handle_state.smart_tab_sharing_toggled_since_last_turn);

  ASSERT_EQ(session_handle_state.attached_tabs.size(), 1u);
  EXPECT_EQ(session_handle_state.attached_tabs[0]->tab_id, 105);
  EXPECT_EQ(session_handle_state.attached_tabs[0]->title, "Chromium");
  EXPECT_EQ(session_handle_state.attached_tabs[0]->context_token,
            delayed_token);
  EXPECT_TRUE(session_handle_state.attached_tabs[0]->pending_delayed_upload);

  ASSERT_EQ(session_handle_state.restored_tabs.size(), 1u);
  EXPECT_EQ(session_handle_state.restored_tabs[0]->tab_id, 109);
  EXPECT_TRUE(session_handle_state.restored_tabs[0]->restored_from_aim);

  ASSERT_EQ(session_handle_state.deselected_tabs.size(), 1u);
  EXPECT_EQ(session_handle_state.deselected_tabs[0]->tab_id, 108);
  EXPECT_EQ(session_handle_state.deselected_tabs[0]->title, "Gmail");
  EXPECT_EQ(session_handle_state.deselected_tabs[0]->url,
            GURL("https://mail.google.com/"));

  ASSERT_EQ(session_handle_state.pending_delayed_tab_ids.size(), 1u);
  EXPECT_EQ(session_handle_state.pending_delayed_tab_ids[0], 105);

  ASSERT_EQ(session_handle_state.uploaded_files.size(), 1u);
  EXPECT_EQ(session_handle_state.uploaded_files[0]->file_name,
            "design_doc.pdf");
  EXPECT_EQ(session_handle_state.uploaded_files[0]->file_size_bytes, 2048u);
  EXPECT_EQ(session_handle_state.uploaded_files[0]->mime_type,
            "kPdf (application/pdf)");
  EXPECT_EQ(session_handle_state.uploaded_files[0]->upload_status,
            "kUploadSuccessful");

  ASSERT_EQ(session_handle_state.submitted_files.size(), 1u);
  EXPECT_EQ(session_handle_state.submitted_files[0]->file_name,
            "screenshot.png");
  EXPECT_EQ(session_handle_state.submitted_files[0]->file_size_bytes, 4096u);
  EXPECT_EQ(session_handle_state.submitted_files[0]->mime_type,
            "kImage (image/png)");
  EXPECT_EQ(session_handle_state.submitted_files[0]->upload_status,
            "kUploadSuccessful");

  ASSERT_EQ(session_handle_state.previous_turns.size(), 1u);
  EXPECT_EQ(session_handle_state.previous_turns[0],
            "Compare Chromium Mojo patterns");

  EXPECT_EQ(session_handle_state.thread_type, "kAiMode");
  EXPECT_EQ(session_handle_state.thread_server_id, "srv_123");
  EXPECT_EQ(session_handle_state.thread_title, "Test Thread Title");
  EXPECT_EQ(session_handle_state.thread_conversation_turn_id, "turn_456");

  ASSERT_EQ(session_handle_state.url_resources.size(), 1u);
  EXPECT_EQ(session_handle_state.url_resources[0]->url_id,
            "11111111-2222-3333-4444-555555555555");
  EXPECT_EQ(session_handle_state.url_resources[0]->url,
            GURL("https://chromium.org/doc"));
  EXPECT_EQ(session_handle_state.url_resources[0]->tab_id, 105);
  EXPECT_EQ(session_handle_state.url_resources[0]->title, "Chromium Doc");
  EXPECT_EQ(session_handle_state.url_resources[0]->context_id, 98765);
  EXPECT_EQ(session_handle_state.url_resources[0]->resource_type, "kWebpage");
}

TEST_F(ContextualTasksInternalsPageHandlerTest,
       GetSessionAndUserDataState_EmptyWhenNoWindowsOpen) {
  base::test::TestFuture<
      std::vector<contextual_tasks_internals::mojom::WebContentsDebugStatePtr>>
      future;
  handler_->GetSessionAndUserDataState(future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(
    ContextualTasksInternalsPageHandlerTest,
    GetSessionAndUserDataState_IteratesWindowsFiltersProfileAndDeduplicates) {
  using ::testing::NiceMock;
  using ::testing::Return;
  using ::testing::ReturnRef;

  BrowserCollectionObserver* delegate =
      GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();

  // 1. Browser window belonging to a different profile (should be filtered
  // out).
  TestingProfile other_profile;
  content::WebContents* other_wc =
      web_contents_factory_->CreateWebContents(profile_.get());
  ContextualTasksWebContentsUserData::CreateForWebContents(other_wc);
  ui::UnownedUserDataHost other_host;
  NiceMock<MockBrowserWindowInterface> other_browser;
  ON_CALL(other_browser, GetProfile()).WillByDefault(Return(&other_profile));
  ON_CALL(other_browser, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  NiceMock<MockContextualTasksPanelController> other_panel_controller(
      &other_browser);
  ON_CALL(other_panel_controller, GetPanelWebContentsList())
      .WillByDefault(Return(std::vector<content::WebContents*>{other_wc}));

  // 2. Browser window belonging to `profile_` with a side panel `WebContents`,
  // an active tracked tab, a duplicate tab pointing to the side panel
  // `WebContents`, and an untracked tab.
  content::WebContents* panel_wc =
      web_contents_factory_->CreateWebContents(profile_.get());
  ContextualTasksWebContentsUserData::CreateForWebContents(panel_wc);
  base::Uuid panel_task_id =
      base::Uuid::ParseLowercase("11111111-2222-3333-4444-555555555555");
  ContextualTasksWebContentsUserData::FromWebContents(panel_wc)->SetTaskId(
      panel_task_id);

  content::WebContents* active_tab_wc =
      web_contents_factory_->CreateWebContents(profile_.get());
  sessions::SessionTabHelper::CreateForWebContents(
      active_tab_wc,
      base::BindRepeating(
          [](content::WebContents*) -> sessions::SessionTabHelperDelegate* {
            return nullptr;
          }));
  ContextualTasksWebContentsUserData::CreateForWebContents(active_tab_wc);
  base::Uuid tab_task_id =
      base::Uuid::ParseLowercase("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
  ContextualTasksWebContentsUserData::FromWebContents(active_tab_wc)
      ->SetTaskId(tab_task_id);

  content::WebContents* untracked_tab_wc =
      web_contents_factory_->CreateWebContents(profile_.get());

  ui::UnownedUserDataHost main_host;
  NiceMock<MockBrowserWindowInterface> main_browser;
  ON_CALL(main_browser, GetProfile()).WillByDefault(Return(profile_.get()));
  ON_CALL(main_browser, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(main_host));

  NiceMock<MockContextualTasksPanelController> panel_controller(&main_browser);
  ON_CALL(panel_controller, GetPanelWebContentsList())
      .WillByDefault(
          Return(std::vector<content::WebContents*>{panel_wc, panel_wc}));

  NiceMock<MockTabListInterface> tab_list;
  ui::ScopedUnownedUserData<TabListInterface> tab_list_reg(main_host, tab_list);

  tabs::MockTabInterface active_tab;
  ON_CALL(active_tab, GetContents()).WillByDefault(Return(active_tab_wc));
  tabs::MockTabInterface duplicate_tab;
  ON_CALL(duplicate_tab, GetContents()).WillByDefault(Return(panel_wc));
  tabs::MockTabInterface untracked_tab;
  ON_CALL(untracked_tab, GetContents()).WillByDefault(Return(untracked_tab_wc));

  ON_CALL(tab_list, GetActiveTab()).WillByDefault(Return(&active_tab));
  ON_CALL(tab_list, GetTabCount()).WillByDefault(Return(3));
  ON_CALL(tab_list, GetTab(0)).WillByDefault(Return(&active_tab));
  ON_CALL(tab_list, GetTab(1)).WillByDefault(Return(&duplicate_tab));
  ON_CALL(tab_list, GetTab(2)).WillByDefault(Return(&untracked_tab));

  delegate->OnBrowserCreated(&other_browser);
  delegate->OnBrowserCreated(&main_browser);

  base::test::TestFuture<
      std::vector<contextual_tasks_internals::mojom::WebContentsDebugStatePtr>>
      future;
  handler_->GetSessionAndUserDataState(future.GetCallback());
  const auto& states = future.Get();

  delegate->OnBrowserClosed(&main_browser);
  delegate->OnBrowserClosed(&other_browser);

  ASSERT_EQ(states.size(), 2u);
  EXPECT_TRUE(states[0]->is_side_panel);
  EXPECT_FALSE(states[0]->is_active_tab_in_window);
  ASSERT_TRUE(states[0]->user_data_state);
  EXPECT_EQ(states[0]->user_data_state->user_data_task_id,
            panel_task_id.AsLowercaseString());

  EXPECT_FALSE(states[1]->is_side_panel);
  EXPECT_TRUE(states[1]->is_active_tab_in_window);
  EXPECT_TRUE(states[1]->session_tab_id.has_value());
  ASSERT_TRUE(states[1]->user_data_state);
  EXPECT_EQ(states[1]->user_data_state->user_data_task_id,
            tab_task_id.AsLowercaseString());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace contextual_tasks
