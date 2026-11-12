// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::base::test::TestFuture;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::AnnotatedPageContent;
using ::testing::NiceMock;
using ::testing::Return;

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  using password_manager::StubPasswordManagerClient::GetPasswordManager;
  MOCK_METHOD(const password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (const, override));
};

class ChangePasswordToolTest : public testing::Test {
 public:
  ChangePasswordToolTest() = default;
  ~ChangePasswordToolTest() override = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents_, GURL("https://example.com"));
    ON_CALL(*mock_tab_, GetContents()).WillByDefault(Return(web_contents_));
    actor_surface_ = std::make_unique<ActorSurfaceImpl>(
        ActorSurfaceHandle::CreateForTesting(), mock_tab_->GetHandle());

    ON_CALL(password_client_, GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&form_cache_));
    ON_CALL(form_cache_, GetFormManagers())
        .WillByDefault([this]() -> base::span<const std::unique_ptr<
                                    password_manager::PasswordFormManager>> {
          return form_managers_;
        });

    password_manager::ContentPasswordManagerDriverFactory::CreateForWebContents(
        web_contents_, &password_client_);
  }

  void TearDown() override {
    form_managers_.clear();
    form_fetchers_.clear();
    DestroyTab();
  }

  void DestroyTab() {
    actor_surface_.reset();
    mock_tab_.reset();
    if (web_contents_) {
      web_contents_factory_.DestroyWebContents(
          web_contents_.ExtractAsDangling());
    }
  }

  password_manager::ContentPasswordManagerDriver* MainFrameDriver() {
    return password_manager::ContentPasswordManagerDriver::
        GetForRenderFrameHost(web_contents_->GetPrimaryMainFrame());
  }

  autofill::FormFieldData MakeFormField(
      int renderer_id,
      autofill::FormControlType control_type =
          autofill::FormControlType::kInputPassword) {
    autofill::FormFieldData field;
    field.set_renderer_id(autofill::FieldRendererId(renderer_id));
    field.set_form_control_type(control_type);
    return field;
  }

  password_manager::PasswordFormManager* AddFormManager(
      std::vector<autofill::FormFieldData> fields,
      password_manager::ContentPasswordManagerDriver* driver = nullptr,
      autofill::FormRendererId form_id = autofill::FormRendererId(1)) {
    if (!driver) {
      driver = MainFrameDriver();
    }
    autofill::FormData form_data;
    form_data.set_url(GURL("https://example.com"));
    form_data.set_action(GURL("https://example.com/submit"));
    form_data.set_renderer_id(form_id);
    form_data.set_fields(std::move(fields));

    auto fetcher = std::make_unique<password_manager::FakeFormFetcher>();
    auto save_manager =
        std::make_unique<password_manager::PasswordSaveManagerImpl>(
            std::make_unique<password_manager::StubFormSaver>(),
            /*account_form_saver=*/nullptr);
    auto manager = std::make_unique<password_manager::PasswordFormManager>(
        &password_client_, driver->AsWeakPtr(), form_data, fetcher.get(),
        std::move(save_manager), /*metrics_recorder=*/nullptr);
    fetcher->NotifyFetchCompleted();

    password_manager::PasswordFormManager* raw_manager = manager.get();
    form_fetchers_.push_back(std::move(fetcher));
    form_managers_.push_back(std::move(manager));
    return raw_manager;
  }

  std::string GetMainFrameDocToken() {
    return DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
               web_contents_->GetPrimaryMainFrame())
        ->serialized_token();
  }

  AnnotatedPageContent CreateObservationWithFieldIds(
      const std::string& doc_token,
      std::initializer_list<int> node_ids) {
    AnnotatedPageContent observation;
    observation.mutable_main_frame_data()
        ->mutable_document_identifier()
        ->set_serialized_token(doc_token);
    auto* root_node = observation.mutable_root_node();
    for (int node_id : node_ids) {
      root_node->add_children_nodes()
          ->mutable_content_attributes()
          ->set_common_ancestor_dom_node_id(node_id);
    }
    return observation;
  }

  MockToolDelegate& delegate() { return delegate_; }
  ActorSurface& actor_surface() { return *actor_surface_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  NiceMock<MockPasswordManagerClient> password_client_;
  NiceMock<password_manager::MockPasswordManager> password_manager_;
  NiceMock<password_manager::MockPasswordFormCache> form_cache_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::vector<std::unique_ptr<password_manager::FakeFormFetcher>>
      form_fetchers_;
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers_;
  MockToolDelegate delegate_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<ActorSurfaceImpl> actor_surface_;
};

TEST_F(ChangePasswordToolTest, Validate_Succeeds) {
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/
      PageTarget(DomNode{.node_id = 10, .document_identifier = "doc_token_1"}),
      /*new_password_field=*/
      PageTarget(DomNode{.node_id = 20, .document_identifier = "doc_token_1"}),
      /*confirm_new_password_field=*/
      PageTarget(DomNode{.node_id = 30, .document_identifier = "doc_token_1"}),
      /*confirm_button=*/PageTarget(gfx::Point(50, 60)));

  TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(ChangePasswordToolTest, Validate_CoordinatePasswordField_Fails) {
  ChangePasswordTool tool(TaskId(1), delegate(), actor_surface(),
                          /*current_password_field=*/std::nullopt,
                          /*new_password_field=*/PageTarget(gfx::Point(10, 20)),
                          /*confirm_new_password_field=*/std::nullopt,
                          /*confirm_button=*/std::nullopt);

  TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  EXPECT_EQ(future.Get()->code, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(ChangePasswordToolTest, Validate_NoPasswordFieldsProvided_Fails) {
  ChangePasswordTool tool(TaskId(1), delegate(), actor_surface(),
                          /*current_password_field=*/std::nullopt,
                          /*new_password_field=*/std::nullopt,
                          /*confirm_new_password_field=*/std::nullopt,
                          /*confirm_button=*/std::nullopt);

  TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  EXPECT_EQ(future.Get()->code, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(ChangePasswordToolTest, Properties) {
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/
      PageTarget(DomNode{.node_id = 10, .document_identifier = "doc_token_1"}),
      /*new_password_field=*/
      PageTarget(DomNode{.node_id = 20, .document_identifier = "doc_token_1"}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/PageTarget(gfx::Point(50, 60)));

  EXPECT_EQ(tool.DebugString(),
            "ChangePasswordTool[current_password_field=DomNode[id=10 "
            "doc_id=doc_token_1], new_password_field=DomNode[id=20 "
            "doc_id=doc_token_1], confirm_new_password_field=null, "
            "confirm_button=50,60]");
  EXPECT_EQ(tool.JournalEvent(), "ChangePassword");
  EXPECT_EQ(tool.GetTargetActorSurface(), actor_surface().GetHandle());
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_Succeeds) {
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)});
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 30, .document_identifier = doc_token}),
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20, 30});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  ExpectOkResult(*result);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_TabWentAway) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  actor_surface_.reset();
  mock_tab_.reset();
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kTabWentAway);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_NoLastObservation) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(nullptr);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingNoLastTabObservation);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_FieldNotFound) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 999, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20, 30});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kFormFillingFieldNotFound);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_FieldsInDifferentFrames) {
  content::RenderFrameHost* main_rfh = web_contents_->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("subframe");
  std::unique_ptr<content::NavigationSimulator> nav_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/subframe"), child_rfh);
  nav_simulator->Commit();
  child_rfh = nav_simulator->GetFinalRenderFrameHost();

  std::string main_doc_token = GetMainFrameDocToken();
  std::string child_doc_token =
      DocumentIdentifierUserData::GetOrCreateForCurrentDocument(child_rfh)
          ->serialized_token();

  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(main_doc_token, {10});
  auto* iframe_node = observation.mutable_root_node()->add_children_nodes();
  iframe_node->mutable_content_attributes()
      ->mutable_iframe_data()
      ->mutable_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(child_doc_token);
  iframe_node->add_children_nodes()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(20);

  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = main_doc_token}),
      PageTarget(
          DomNode{.node_id = 20, .document_identifier = child_doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_DuplicateFieldIds) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_FrameWentAway) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  web_contents_->RemoveUserData(
      password_manager::ContentPasswordManagerDriverFactory::UserDataKey());

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_NullPasswordManager_Fails) {
  EXPECT_CALL(password_client_, GetPasswordManager()).WillOnce(Return(nullptr));
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_NullFormCache_Fails) {
  EXPECT_CALL(password_manager_, GetPasswordFormCache())
      .WillOnce(Return(nullptr));
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordToolTest, TimeOfUseValidation_EmptyFormCache_Fails) {
  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_MultipleManagersInCache_MatchesTargetForm) {
  AddFormManager({MakeFormField(99)}, MainFrameDriver(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 MainFrameDriver(), autofill::FormRendererId(2));

  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 30, .document_identifier = doc_token}),
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20, 30});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  ExpectOkResult(*result);
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_ManagerForDifferentDriver_Fails) {
  content::RenderFrameHost* main_rfh = web_contents_->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("subframe");
  std::unique_ptr<content::NavigationSimulator> nav_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/subframe"), child_rfh);
  nav_simulator->Commit();
  child_rfh = nav_simulator->GetFinalRenderFrameHost();
  password_manager::ContentPasswordManagerDriver* child_driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          child_rfh);
  ASSERT_TRUE(child_driver);

  AddFormManager({MakeFormField(20)}, child_driver);

  std::string main_doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      /*current_password_field=*/std::nullopt,
      PageTarget(DomNode{.node_id = 20, .document_identifier = main_doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(main_doc_token, {20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_UnownedPasswordFields_Succeeds) {
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 MainFrameDriver(), autofill::FormRendererId());

  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 30, .document_identifier = doc_token}),
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20, 30});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  ExpectOkResult(*result);
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_FieldsSplitAcrossManagers_Succeeds) {
  AddFormManager({MakeFormField(10)}, MainFrameDriver(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(20)}, MainFrameDriver(),
                 autofill::FormRendererId(2));

  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  ExpectOkResult(*result);
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_PartiallyUnmanagedFields_Fails) {
  AddFormManager({MakeFormField(10)});

  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordToolTest,
       TimeOfUseValidation_NonPasswordFieldInManagedForm_Fails) {
  AddFormManager(
      {MakeFormField(10, autofill::FormControlType::kInputText),
       MakeFormField(20, autofill::FormControlType::kInputPassword)});

  std::string doc_token = GetMainFrameDocToken();
  ChangePasswordTool tool(
      TaskId(1), delegate(), actor_surface(),
      PageTarget(DomNode{.node_id = 10, .document_identifier = doc_token}),
      PageTarget(DomNode{.node_id = 20, .document_identifier = doc_token}),
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt);
  AnnotatedPageContent observation =
      CreateObservationWithFieldIds(doc_token, {10, 20});

  mojom::ActionResultPtr result = tool.TimeOfUseValidation(&observation);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result->message,
            "Target field is not a valid password field: "
            "current_password_field (id 10).");
}

}  // namespace
}  // namespace actor
