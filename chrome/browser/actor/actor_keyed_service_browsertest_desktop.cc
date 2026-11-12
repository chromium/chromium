// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_browsertest.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/headless_web_contents_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace actor {
namespace {

// Opens `url` in a new foreground tab of `browser` and returns the tab.
tabs::TabInterface* OpenTab(BrowserWindowInterface* browser, const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, ::ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  return browser->GetTabStripModel()->GetActiveTab();
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest, StopPausedTask) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  // Open a second tab to prevent the browser from closing when the active tab
  // is closed.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"),
      ::ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Navigate the active tab to a new page.
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("/actor/blank.html")));

  {
    actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
    actor::AddTabToTask(*active_tab(), *task);

    task->Pause(/*from_actor=*/false);
    CHECK(!task->IsCompleted());
  }
  base::RunLoop run_loop;
  auto discard = active_tab()->RegisterWillDetach(base::BindRepeating(
      [](base::RepeatingClosure run_loop_closure, tabs::TabInterface* tab,
         tabs::TabInterface::DetachReason reason) { run_loop_closure.Run(); },
      run_loop.QuitClosure()));
  active_tab()->Close();
  run_loop.Run();

  {
    base::RunLoop run_loop2;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop2.QuitClosure());
    run_loop2.Run();
  }

  // The task should be destroyed.
  EXPECT_FALSE(actor_keyed_service()->GetTask(task_id));
}

IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       ObserveTabOnceCleanupOnClose) {
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());

  actor::ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  // Open a second tab to observe.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"),
      ::ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* second_tab =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  ASSERT_TRUE(second_tab);
  tabs::TabHandle second_tab_handle = second_tab->GetHandle();
  const ActorSurfaceHandle second_actor_surface_handle =
      ActorSurfaceHandle::From(second_tab_handle);

  // Observe the second tab.
  task->ObserveTabOnce(second_tab_handle);

  // Verify it is in LastActedActorSurfaces.
  EXPECT_TRUE(
      task->GetLastActedActorSurfaces().contains(second_actor_surface_handle));

  // Close the second tab.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);

  // Flush message loop.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Verify it is NO LONGER in LastActedActorSurfaces.
  EXPECT_FALSE(
      task->GetLastActedActorSurfaces().contains(second_actor_surface_handle));
}

// A promoted headless page moves, with its state, into a new selected tab
// that keeps the surface's handle, so a task using the surface still has it.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       PromoteHeadlessActorSurfaceMovesPageIntoTab) {
  ScopedHeadlessActorSurface headless =
      actor_keyed_service()->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  content::WebContents* contents = handle.Get()->GetWebContents();
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(
      contents, embedded_https_test_server().GetURL("/actor/blank.html")));
  ASSERT_TRUE(content::ExecJs(contents, "window.promotionMarker = 'kept';"));

  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);
  base::test::TestFuture<mojom::ActionResultPtr> added;
  task->AddActorSurface(handle, /*stop_task_on_detach=*/true,
                        added.GetCallback());
  ASSERT_TRUE(added.Wait());
  ASSERT_TRUE(task->HasActorSurface(handle));

  const int tab_count = browser()->tab_strip_model()->count();
  tabs::TabInterface* tab =
      actor_keyed_service()->PromoteHeadlessActorSurface(headless, *browser());

  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetContents(), contents);
  EXPECT_EQ(browser()->tab_strip_model()->count(), tab_count + 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), contents);
  EXPECT_EQ(content::EvalJs(contents, "window.promotionMarker"), "kept");
  EXPECT_TRUE(handle.Get()->IsTab());
  EXPECT_EQ(
      actor_keyed_service()->GetSurfaceRegistry().GetForTab(tab->GetHandle()),
      handle.Get());
  EXPECT_TRUE(task->HasActorSurface(handle));
  EXPECT_TRUE(headless.actor_surface_handle().is_null());

  // Dropping the now-empty owner leaves the tab and the task alone.
  {
    ScopedHeadlessActorSurface dropped = std::move(headless);
  }
  EXPECT_EQ(browser()->tab_strip_model()->count(), tab_count + 1);
  EXPECT_FALSE(task->IsCompleted());
}

// Promotion fails without side effects for a window of another profile.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       PromoteHeadlessActorSurfaceRejectsOtherProfileWindow) {
  ScopedHeadlessActorSurface headless =
      actor_keyed_service()->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  BrowserWindowInterface* incognito = CreateIncognitoBrowser();
  const int tab_count = incognito->GetTabStripModel()->count();

  EXPECT_FALSE(
      actor_keyed_service()->PromoteHeadlessActorSurface(headless, *incognito));

  EXPECT_EQ(headless.actor_surface_handle(), handle);
  ASSERT_TRUE(handle.Get());
  EXPECT_FALSE(handle.Get()->IsTab());
  EXPECT_EQ(incognito->GetTabStripModel()->count(), tab_count);
}

// Promotion fails without side effects for a window that isn't a normal tabbed
// window, even though a popup window has a tab strip.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       PromoteHeadlessActorSurfaceRejectsPopupWindow) {
  ScopedHeadlessActorSurface headless =
      actor_keyed_service()->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();
  BrowserWindowInterface* popup =
      CreateBrowserForPopup(browser()->GetProfile());
  const int tab_count = popup->GetTabStripModel()->count();

  EXPECT_FALSE(
      actor_keyed_service()->PromoteHeadlessActorSurface(headless, *popup));

  EXPECT_EQ(headless.actor_surface_handle(), handle);
  ASSERT_TRUE(handle.Get());
  EXPECT_FALSE(handle.Get()->IsTab());
  EXPECT_EQ(popup->GetTabStripModel()->count(), tab_count);
}

// A demoted tab's page leaves the tab strip, with its state and its surface's
// handle and ActorTabData, and lives on as a headless page owned by the
// returned object.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       DemoteTabToHeadlessActorSurfaceMovesPageOutOfTab) {
  // Demote a second tab, so the window stays open after the tab closes.
  tabs::TabInterface* tab = OpenTab(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html"));
  const tabs::TabHandle tab_handle = tab->GetHandle();
  content::WebContents* contents = tab->GetContents();
  ASSERT_TRUE(content::ExecJs(contents, "window.demotionMarker = 'kept';"));
  const ActorSurfaceHandle handle = ActorSurfaceHandle::From(tab_handle);
  ActorSurface* surface = handle.Get();
  ASSERT_TRUE(surface);
  optimization_guide::AIPageContentResult observation;
  observation.proto.mutable_main_frame_data()->set_title("observed");
  surface->GetActorTabData()->DidObserveContent(observation, ApcSource::kActor);
  const int tab_count = browser()->tab_strip_model()->count();
  HeadlessWebContentsManager& headless_manager =
      actor_keyed_service()->GetHeadlessWebContentsManagerForTesting();
  ASSERT_FALSE(headless_manager.Contains(contents));

  std::optional<ScopedHeadlessActorSurface> headless =
      actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle);

  ASSERT_TRUE(headless);
  EXPECT_EQ(headless->actor_surface_handle(), handle);
  EXPECT_EQ(browser()->tab_strip_model()->count(), tab_count - 1);
  EXPECT_FALSE(tab_handle.Get());
  EXPECT_TRUE(headless_manager.Contains(contents));
  EXPECT_EQ(handle.Get(), surface);
  EXPECT_FALSE(surface->IsTab());
  EXPECT_EQ(surface->GetWebContents(), contents);
  EXPECT_FALSE(tabs::TabInterface::MaybeGetFromContents(contents));
  ActorSurfaceRegistry& registry = actor_keyed_service()->GetSurfaceRegistry();
  EXPECT_FALSE(registry.GetForTab(tab_handle));
  EXPECT_EQ(registry.GetForHeadless(contents), surface);
  EXPECT_EQ(content::EvalJs(contents, "window.demotionMarker"), "kept");
  const optimization_guide::proto::AnnotatedPageContent* observed =
      surface->GetActorTabData()->GetLastObservedPageContent();
  ASSERT_TRUE(observed);
  EXPECT_EQ(observed->main_frame_data().title(), "observed");

  // Dropping the owner destroys the page and the surface.
  content::WebContentsDestroyedWatcher watcher(contents);
  headless.reset();
  EXPECT_TRUE(watcher.IsDestroyed());
  EXPECT_FALSE(handle.Get());
}

// Demoting and then promoting a page keeps the same page and surface handle.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       DemoteThenPromoteActorSurfaceKeepsPageAndHandle) {
  tabs::TabInterface* tab = OpenTab(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html"));
  content::WebContents* contents = tab->GetContents();
  const ActorSurfaceHandle handle = ActorSurfaceHandle::From(tab->GetHandle());

  std::optional<ScopedHeadlessActorSurface> headless =
      actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle);
  ASSERT_TRUE(headless);
  HeadlessWebContentsManager& headless_manager =
      actor_keyed_service()->GetHeadlessWebContentsManagerForTesting();
  EXPECT_TRUE(headless_manager.Contains(contents));
  tabs::TabInterface* new_tab =
      actor_keyed_service()->PromoteHeadlessActorSurface(*headless, *browser());

  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents(), contents);
  EXPECT_FALSE(headless_manager.Contains(contents));
  EXPECT_EQ(ActorSurfaceHandle::From(new_tab->GetHandle()), handle);
  ASSERT_TRUE(handle.Get());
  EXPECT_TRUE(handle.Get()->IsTab());
  EXPECT_TRUE(headless->actor_surface_handle().is_null());
}

// A surface that a task uses can be demoted; the task keeps the surface and
// isn't stopped, since the page it acts on is still alive.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       DemoteTabToHeadlessActorSurfaceKeepsTaskUsingSurface) {
  tabs::TabInterface* tab = OpenTab(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html"));
  content::WebContents* contents = tab->GetContents();
  const ActorSurfaceHandle handle = ActorSurfaceHandle::From(tab->GetHandle());
  TaskId task_id = actor_keyed_service()->CreateTask(
      TestTaskSourceInfo(), NoEnterprisePolicyChecker());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);
  AddTabToTask(*tab, *task);
  ASSERT_TRUE(task->HasActorSurface(handle));

  std::optional<ScopedHeadlessActorSurface> headless =
      actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle);

  ASSERT_TRUE(headless);
  ASSERT_TRUE(handle.Get());
  EXPECT_FALSE(handle.Get()->IsTab());
  EXPECT_TRUE(
      actor_keyed_service()->GetHeadlessWebContentsManagerForTesting().Contains(
          contents));
  EXPECT_TRUE(task->HasActorSurface(handle));
  EXPECT_FALSE(task->IsCompleted());
}

// A tab showing tab-modal UI isn't demoted, since the UI would go away with
// the tab.
IN_PROC_BROWSER_TEST_F(
    ActorKeyedServiceBrowserTest,
    DemoteTabToHeadlessActorSurfaceRejectsTabShowingModalUI) {
  tabs::TabInterface* tab = OpenTab(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html"));
  const ActorSurfaceHandle handle = ActorSurfaceHandle::From(tab->GetHandle());
  std::unique_ptr<tabs::ScopedTabModalUI> modal_ui = tab->ShowModalUI();
  const int tab_count = browser()->tab_strip_model()->count();

  EXPECT_FALSE(actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle));

  EXPECT_EQ(browser()->tab_strip_model()->count(), tab_count);
  ASSERT_TRUE(handle.Get());
  EXPECT_TRUE(handle.Get()->IsTab());
}

// Only tab-backed surfaces can be demoted.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       DemoteTabToHeadlessActorSurfaceRejectsNonTabSurfaces) {
  ScopedHeadlessActorSurface headless =
      actor_keyed_service()->CreateHeadlessActorSurface();
  const ActorSurfaceHandle handle = headless.actor_surface_handle();

  EXPECT_FALSE(actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle));
  ASSERT_TRUE(handle.Get());
  EXPECT_FALSE(handle.Get()->IsTab());

  EXPECT_FALSE(actor_keyed_service()->DemoteTabToHeadlessActorSurface(
      ActorSurfaceHandle::Null()));
  EXPECT_FALSE(actor_keyed_service()->DemoteTabToHeadlessActorSurface(
      ActorSurfaceHandle::CreateForTesting()));
}

// Like promotion, demotion only takes pages out of normal tabbed windows, even
// though a popup window has a tab strip too.
IN_PROC_BROWSER_TEST_F(ActorKeyedServiceBrowserTest,
                       DemoteTabToHeadlessActorSurfaceRejectsPopupWindow) {
  BrowserWindowInterface* popup =
      CreateBrowserForPopup(browser()->GetProfile());
  tabs::TabInterface* tab = popup->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);
  const ActorSurfaceHandle handle = ActorSurfaceHandle::From(tab->GetHandle());
  ASSERT_TRUE(handle.Get());
  const int tab_count = popup->GetTabStripModel()->count();

  EXPECT_FALSE(actor_keyed_service()->DemoteTabToHeadlessActorSurface(handle));

  EXPECT_EQ(popup->GetTabStripModel()->count(), tab_count);
  ASSERT_TRUE(handle.Get());
  EXPECT_TRUE(handle.Get()->IsTab());
}

}  // namespace
}  // namespace actor
