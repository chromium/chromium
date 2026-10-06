// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_tab_helper.h"

#include <memory>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_browsertest.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace actor {
namespace {

using ActorSurfaceTabHelperBrowserTest = ActorKeyedServiceBrowserTest;

IN_PROC_BROWSER_TEST_F(ActorSurfaceTabHelperBrowserTest,
                       TabHasSurfaceOnCreationAndDestroysOnClose) {
  ActorSurfaceRegistry& registry = actor_keyed_service()->GetSurfaceRegistry();

  tabs::TabInterface* first_tab = active_tab();
  ASSERT_TRUE(first_tab);
  EXPECT_NE(ActorSurfaceTabHelper::From(first_tab), nullptr);

  ActorSurface* first_surface = registry.GetForTab(first_tab->GetHandle());
  ASSERT_TRUE(first_surface);
  EXPECT_TRUE(first_surface->IsTab());
  EXPECT_EQ(first_surface->GetTabHandle(), first_tab->GetHandle());
  EXPECT_EQ(first_surface->GetWebContents(), first_tab->GetContents());
  EXPECT_EQ(first_surface->GetActorTabData(), ActorTabData::From(first_tab));
  EXPECT_EQ(ActorSurfaceHandle(first_tab->GetHandle().raw_value()).Get(),
            first_surface);

  // Opening a second tab eagerly creates its surface.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* second_tab =
      browser()->GetTabStripModel()->GetTabAtIndex(1);
  ASSERT_TRUE(second_tab);
  const tabs::TabHandle second_tab_handle = second_tab->GetHandle();
  const ActorSurfaceHandle second_surface_handle(second_tab_handle.raw_value());

  ActorSurface* second_surface = registry.GetForTab(second_tab_handle);
  ASSERT_TRUE(second_surface);
  EXPECT_EQ(second_surface->GetHandle(), second_surface_handle);
  EXPECT_EQ(second_surface_handle.Get(), second_surface);
  EXPECT_EQ(second_surface->GetWebContents(), second_tab->GetContents());

  // Closing the second tab destroys its surface.
  browser()->GetTabStripModel()->CloseWebContentsAt(1,
                                                    TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(second_surface_handle.Get(), nullptr);
  EXPECT_EQ(registry.GetForTab(second_tab_handle), nullptr);

  // The first tab's surface remains intact.
  EXPECT_EQ(registry.GetForTab(first_tab->GetHandle()), first_surface);
}

IN_PROC_BROWSER_TEST_F(ActorSurfaceTabHelperBrowserTest,
                       TabDiscardPreservesSurfaceAndUpdatesWebContents) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  tabs::TabInterface* tab = tab_strip->GetTabAtIndex(1);
  ASSERT_TRUE(tab);
  const tabs::TabHandle tab_handle = tab->GetHandle();

  ActorSurfaceRegistry& registry = actor_keyed_service()->GetSurfaceRegistry();
  ActorSurface* surface = registry.GetForTab(tab_handle);
  ASSERT_TRUE(surface);
  const ActorSurfaceHandle surface_handle = surface->GetHandle();

  std::unique_ptr<content::WebContents> replacement_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  content::WebContents* replacement_ptr = replacement_contents.get();

  tab_strip->DiscardWebContents(tab->GetContents(),
                                std::move(replacement_contents));

  EXPECT_EQ(registry.GetForTab(tab_handle), surface);
  EXPECT_EQ(surface_handle.Get(), surface);
  EXPECT_EQ(surface->GetWebContents(), replacement_ptr);
}

IN_PROC_BROWSER_TEST_F(ActorSurfaceTabHelperBrowserTest,
                       TabMoveBetweenWindowsPreservesSurface) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetTabAtIndex(1);
  ASSERT_TRUE(tab);
  const tabs::TabHandle tab_handle = tab->GetHandle();

  ActorSurfaceRegistry& registry = actor_keyed_service()->GetSurfaceRegistry();
  ActorSurface* surface = registry.GetForTab(tab_handle);
  ASSERT_TRUE(surface);
  const ActorSurfaceHandle surface_handle = surface->GetHandle();

  BrowserWindowInterface* second_browser = CreateBrowser(GetProfile());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->GetTabStripModel()->DetachTabAtForInsertion(1);
  second_browser->GetTabStripModel()->InsertDetachedTabAt(
      1, std::move(detached_tab), AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(registry.GetForTab(tab_handle), surface);
  EXPECT_EQ(surface_handle.Get(), surface);
  EXPECT_EQ(surface->GetWebContents(), tab->GetContents());
}

IN_PROC_BROWSER_TEST_F(ActorSurfaceTabHelperBrowserTest,
                       IncognitoTabGetsNoSurface) {
  ActorSurfaceRegistry& regular_registry =
      actor_keyed_service()->GetSurfaceRegistry();
  const size_t initial_size = regular_registry.size();

  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(GetProfile());
  ASSERT_TRUE(incognito_browser);
  EXPECT_EQ(ActorKeyedService::Get(incognito_browser->GetProfile()), nullptr);

  tabs::TabInterface* incognito_tab =
      incognito_browser->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(incognito_tab);
  EXPECT_NE(ActorSurfaceTabHelper::From(incognito_tab), nullptr);
  EXPECT_EQ(ActorSurfaceHandle(incognito_tab->GetHandle().raw_value()).Get(),
            nullptr);
  EXPECT_EQ(regular_registry.GetForTab(incognito_tab->GetHandle()), nullptr);
  EXPECT_EQ(regular_registry.size(), initial_size);
}

}  // namespace
}  // namespace actor
