// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_anchor.h"

namespace contextual_tasks {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnchorElementId);

constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);

}  // namespace

class ContextualTasksExtensionsContainerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

  void TearDown() override {
    browser_window_.reset();
    profile_.reset();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  std::unique_ptr<ContextualTasksExtensionsContainer> CreateContainer(
      content::WebContents* web_contents,
      ContextualTasksExtensionsContainer::AnchorProvider anchor_provider =
          base::NullCallback()) {
    return std::make_unique<ContextualTasksExtensionsContainer>(
        browser_window_.get(), web_contents, std::move(anchor_provider));
  }

  void InitializeExtensionSystemAndToolbarModel(bool wait_for_ready = true) {
    auto* extension_system = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile_.get()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(),
        /*autoupdate_enabled=*/false);
    if (wait_for_ready) {
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile_.get());
    } else {
      extensions::extension_action_test_util::
          CreateToolbarModelForProfileWithoutWaitingForReady(profile_.get());
    }
  }

  scoped_refptr<const extensions::Extension> AddExtensionWithAction(
      const std::string& name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetAction(extensions::ActionInfo::Type::kAction)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
    extensions::ExtensionRegistrar::Get(profile_.get())
        ->AddExtension(extension.get());
    return extension;
  }

  void RemoveExtension(const extensions::ExtensionId& id) {
    extensions::ExtensionRegistrar::Get(profile_.get())
        ->RemoveExtension(id, extensions::UnloadedExtensionReason::DISABLE);
  }

  // Returns an `AnchorProvider` that resolves `kTestAnchorElementId` the same
  // way production code resolves the side panel's super G button, and records
  // how many times it ran in `run_count`.
  ContextualTasksExtensionsContainer::AnchorProvider CreateAnchorProvider(
      int* run_count) {
    return base::BindLambdaForTesting([run_count]() {
      ++*run_count;
      ui::TrackedElement* element =
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              kTestAnchorElementId, kTestContext);
      return element ? views::BubbleAnchor(element) : views::BubbleAnchor();
    });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_window_;
};

TEST_F(ContextualTasksExtensionsContainerTest, GetAndSetActiveWebContents) {
  auto web_contents1 = CreateTestWebContents();
  auto web_contents2 = CreateTestWebContents();

  auto container = CreateContainer(web_contents1.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents1.get());

  container->SetWebContents(web_contents2.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents2.get());

  container->SetWebContents(nullptr);
  EXPECT_EQ(container->GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, WebContentsWeakPtrLifecycle) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());
  EXPECT_EQ(container->GetActiveWebContents(), web_contents.get());

  // Destroying the WebContents should safely clear the weak pointer.
  web_contents.reset();
  EXPECT_EQ(container->GetActiveWebContents(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, ContainerDefaults) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  EXPECT_TRUE(container->IsVisible());
  EXPECT_FALSE(container->HasAnyExtensions());
  EXPECT_EQ(container->GetActionForId("test_extension"), nullptr);
  EXPECT_FALSE(container->IsActionVisibleOnToolbar("test_extension"));
  EXPECT_EQ(container->GetPoppedOutActionId(), std::nullopt);
  EXPECT_FALSE(container->ShowToolbarActionPopupForAPICall("test_extension",
                                                           base::DoNothing()));
  EXPECT_EQ(container->GetFocusManagerForAccelerator(), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest, PopOutActionRunsCallback) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  bool called = false;
  container->PopOutAction("test_extension",
                          base::BindLambdaForTesting([&]() { called = true; }));
  EXPECT_TRUE(called);
}

TEST_F(ContextualTasksExtensionsContainerTest, GetPopupArrow) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());
  EXPECT_EQ(container->GetPopupArrow(), views::BubbleBorder::TOP_LEFT);
}

TEST_F(ContextualTasksExtensionsContainerTest, NullAnchorProvider) {
  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  EXPECT_TRUE(container->GetExtensionsButtonAnchor().IsNull());
  EXPECT_TRUE(container->GetReferenceButtonForPopup("test_extension").IsNull());
}

TEST_F(ContextualTasksExtensionsContainerTest, AnchorIsResolvedOnEveryUse) {
  auto web_contents = CreateTestWebContents();
  int run_count = 0;
  auto container =
      CreateContainer(web_contents.get(), CreateAnchorProvider(&run_count));

  ui::test::TestElement element(kTestAnchorElementId, kTestContext);
  element.Show();

  EXPECT_EQ(container->GetExtensionsButtonAnchor().GetIfElement(), &element);
  EXPECT_EQ(run_count, 1);

  // The anchor must be re-resolved rather than served from a cached copy.
  EXPECT_EQ(
      container->GetReferenceButtonForPopup("test_extension").GetIfElement(),
      &element);
  EXPECT_EQ(run_count, 2);
}

// Regression test for a dangling `raw_ptr`: the container used to cache the
// `views::BubbleAnchor` it was shown with, which outlived the WebUI
// `ui::TrackedElement` backing it when the side panel document went away.
TEST_F(ContextualTasksExtensionsContainerTest,
       AnchorIsNotCachedAcrossElementDestruction) {
  auto web_contents = CreateTestWebContents();
  int run_count = 0;
  auto container =
      CreateContainer(web_contents.get(), CreateAnchorProvider(&run_count));

  {
    ui::test::TestElement element(kTestAnchorElementId, kTestContext);
    element.Show();
    EXPECT_FALSE(container->GetExtensionsButtonAnchor().IsNull());
  }

  // The element is gone, as it would be after the side panel WebUI document is
  // destroyed. The container must report a null anchor instead of handing back
  // a pointer to freed memory.
  EXPECT_TRUE(container->GetExtensionsButtonAnchor().IsNull());
  EXPECT_TRUE(container->GetReferenceButtonForPopup("test_extension").IsNull());

  // Showing the menu with no valid anchor must be a no-op rather than a crash.
  container->ShowExtensionsMenu();
  EXPECT_FALSE(container->IsExtensionsMenuShowing());
}

// Regression test for b/568786978: clicking an extension action in the
// side panel's extensions menu calls
// `ExtensionActionDelegateDesktop::GetPopupOwnerDelegate()`, which looks up the
// popup owner model via `extensions_container_->GetActionForId()`. The
// container must populate and maintain `ExtensionActionViewModel` instances for
// all toolbar actions so `GetActionForId()` returns a valid owner model rather
// than `nullptr`.
TEST_F(ContextualTasksExtensionsContainerTest,
       PopulatesAndUpdatesActionsFromToolbarModel) {
  InitializeExtensionSystemAndToolbarModel();
  auto ext1 = AddExtensionWithAction("Extension 1");

  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  // Pre-existing action when the container is constructed should be populated.
  EXPECT_TRUE(container->HasAnyExtensions());
  ToolbarActionViewModel* action1 = container->GetActionForId(ext1->id());
  ASSERT_NE(action1, nullptr);
  EXPECT_EQ(action1->GetId(), ext1->id());
  auto* ext_action1 = static_cast<ExtensionActionViewModel*>(action1);
  ASSERT_NE(ext_action1->delegate(), nullptr);
  EXPECT_EQ(ext_action1->delegate()->GetActiveWebContents(),
            web_contents.get());

  // Adding a new extension after the container exists should add its action.
  auto ext2 = AddExtensionWithAction("Extension 2");
  ToolbarActionViewModel* action2 = container->GetActionForId(ext2->id());
  ASSERT_NE(action2, nullptr);
  EXPECT_EQ(action2->GetId(), ext2->id());

  // Removing a non-existent action ID is a safe no-op.
  container->OnToolbarActionRemoved("non_existent_action_id");
  EXPECT_EQ(container->GetActionForId(ext1->id()), action1);

  // Removing the first extension while it is the active popup owner should
  // clear `popup_owner_` and remove its action while keeping the second.
  container->SetPopupOwner(action1);
  RemoveExtension(ext1->id());
  container->HideActivePopup();
  EXPECT_EQ(container->GetActionForId(ext1->id()), nullptr);
  EXPECT_NE(container->GetActionForId(ext2->id()), nullptr);
  EXPECT_TRUE(container->HasAnyExtensions());

  // Removing the last extension should leave the container with no extensions.
  RemoveExtension(ext2->id());
  EXPECT_EQ(container->GetActionForId(ext2->id()), nullptr);
  EXPECT_FALSE(container->HasAnyExtensions());
}

TEST_F(ContextualTasksExtensionsContainerTest,
       PopulatesActionsWhenToolbarModelInitializesAfterConstruction) {
  InitializeExtensionSystemAndToolbarModel(/*wait_for_ready=*/false);
  auto ext = AddExtensionWithAction("Extension 1");

  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());

  // Before the extension system signals ready, `ToolbarActionsModel` is not
  // initialized yet.
  EXPECT_FALSE(container->HasAnyExtensions());
  EXPECT_EQ(container->GetActionForId(ext->id()), nullptr);

  // Signaling ready triggers `OnToolbarModelInitialized()`, which populates the
  // container's actions.
  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_.get()))
      ->SetReady();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return container->HasAnyExtensions(); }));

  EXPECT_NE(container->GetActionForId(ext->id()), nullptr);
}

TEST_F(ContextualTasksExtensionsContainerTest,
       OnToolbarActionsModelShutdownClearsActions) {
  InitializeExtensionSystemAndToolbarModel();
  auto ext = AddExtensionWithAction("Extension 1");

  auto web_contents = CreateTestWebContents();
  auto container = CreateContainer(web_contents.get());
  ToolbarActionViewModel* action = container->GetActionForId(ext->id());
  ASSERT_NE(action, nullptr);
  EXPECT_TRUE(container->HasAnyExtensions());

  container->SetPopupOwner(action);
  container->OnToolbarActionsModelShutdown();
  container->HideActivePopup();
  EXPECT_EQ(container->GetActionForId(ext->id()), nullptr);
  EXPECT_FALSE(container->HasAnyExtensions());
}

}  // namespace contextual_tasks
