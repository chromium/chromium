// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/desk_template_client_lacros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/range/range.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "url/gurl.h"

namespace {

constexpr char kTestName[] = "chrome version";
constexpr char kChromeVersionUrl[] = "chrome://version/";
constexpr char kGoogleURL[] = "www.google.com";
constexpr char kAboutBlankURL[] = "about:blank";

// Asserts that tab groups are correct based on `expected_state`.
void AssertTabGroupsCorrect(
    TabStripModel* browser_tab_model,
    const crosapi::mojom::DeskTemplateStatePtr& expected_state) {
  TabGroupModel* group_model = browser_tab_model->group_model();
  std::vector<tab_groups::TabGroupId> tab_group_id_list =
      group_model->ListTabGroups();

  EXPECT_TRUE(expected_state->groups.has_value());
  std::vector<tab_groups::TabGroupInfo>& expected_groups =
      expected_state->groups.value();

  for (const auto& group_id : tab_group_id_list) {
    TabGroup* current_group = group_model->GetTabGroup(group_id);

    tab_groups::TabGroupInfo current_group_info = tab_groups::TabGroupInfo(
        current_group->ListTabs(),
        tab_groups::TabGroupVisualData(*current_group->visual_data()));
    auto it = std::find(expected_groups.begin(), expected_groups.end(),
                        current_group_info);

    EXPECT_FALSE(it == expected_groups.end());
  }
}

// Assertst that `browser` was created correctly based on `expected_state` and
// `initial_bounds`.
void AssertBrowserCreatedCorrectly(
    Browser* browser,
    const crosapi::mojom::DeskTemplateStatePtr& expected_state,
    const gfx::Rect& initial_bounds) {
  EXPECT_TRUE(browser);

  EXPECT_EQ(initial_bounds, browser->create_params().initial_bounds);

  if (expected_state->browser_app_name.has_value()) {
    EXPECT_EQ(browser->create_params().type, Browser::Type::TYPE_APP);
  } else {
    EXPECT_EQ(browser->create_params().type, Browser::Type::TYPE_NORMAL);
  }

  EXPECT_EQ(browser->creation_source(), Browser::CreationSource::kDeskTemplate);

  TabStripModel* browser_tab_model = browser->tab_strip_model();
  EXPECT_TRUE(browser_tab_model);

  EXPECT_EQ(expected_state->urls.size(),
            static_cast<uint32_t>(browser_tab_model->count()));

  // We expect order of the browser's tabs to be equivalent to the tabs given.
  int tab_index = 0;
  for (const auto& expected_url : expected_state->urls) {
    EXPECT_EQ(expected_url,
              browser_tab_model->GetWebContentsAt(tab_index)->GetURL());
    ++tab_index;
  }

  if (browser_tab_model->SupportsTabGroups() &&
      expected_state->groups.has_value()) {
    AssertTabGroupsCorrect(browser_tab_model, expected_state);
  }

  EXPECT_EQ(
      expected_state->first_non_pinned_index,
      static_cast<uint32_t>(browser_tab_model->IndexOfFirstNonPinnedTab()));
}

}  // namespace

class DeskTemplateClientLacrosBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the correct GURL object for "www.google.com/test" which is used
  // as a testing URL.
  GURL GetGoogleTestURL() {
    return embedded_test_server()->GetURL(kGoogleURL, "/test");
  }

  // Returns a maximal `DeskTemplateStatePtr` mojom for testing without pinned
  // tabs and tab groups.
  crosapi::mojom::DeskTemplateStatePtr
  MakeTestStateWithoutPinnedTabsOrTabGroup() {
    crosapi::mojom::DeskTemplateStatePtr state =
        crosapi::mojom::DeskTemplateState::New();

    state->urls = std::vector<GURL>(
        {GURL(kChromeVersionUrl), GURL(kAboutBlankURL), GetGoogleTestURL(),
         GURL(kChromeVersionUrl), GURL(kChromeVersionUrl)});
    state->active_index = 0;

    return state;
  }

  // Returns a DeskTemplateStatePtr where all tabs are pinned.  This tests for
  // regressions wherein all tabs become unpinned on launch.
  crosapi::mojom::DeskTemplateStatePtr MakeTestMojomWithOnlyPinnedTabs() {
    crosapi::mojom::DeskTemplateStatePtr state =
        MakeTestStateWithoutPinnedTabsOrTabGroup();

    state->first_non_pinned_index = 5;

    return state;
  }

  // Returns a DeskTemplateStatePtr where some tabs are pinned and the rest are
  // normal. This tests for regressions wherein all tabs become unpinned on
  // launch.
  crosapi::mojom::DeskTemplateStatePtr MakeTestMojomWithSomePinnedTabs() {
    crosapi::mojom::DeskTemplateStatePtr state =
        MakeTestStateWithoutPinnedTabsOrTabGroup();

    state->first_non_pinned_index = 3;

    return state;
  }

  // Returns a maximal `DeskTemplateStatePtr` mojom for testing.
  crosapi::mojom::DeskTemplateStatePtr MakeTestMojom() {
    crosapi::mojom::DeskTemplateStatePtr state =
        MakeTestStateWithoutPinnedTabsOrTabGroup();

    // Add in tab groups and pinned tabs.
    state->first_non_pinned_index = 1;
    state->groups =
        std::vector<tab_groups::TabGroupInfo>({tab_groups::TabGroupInfo(
            gfx::Range(2, 4),
            tab_groups::TabGroupVisualData(
                u"tab group one", tab_groups::TabGroupColorId::kGreen))});

    return state;
  }

  // Returns a maximal `DeskTemplateStatePtr` mojom for testing where the tab
  // group in question reaches to the end of the tab strip.  This ensures that
  // the browser launches with the entirety of the tab group intact.
  crosapi::mojom::DeskTemplateStatePtr MakeTestMojomWithTabGroupAtEndOfStrip() {
    crosapi::mojom::DeskTemplateStatePtr state =
        MakeTestStateWithoutPinnedTabsOrTabGroup();

    // Add in tab groups and pinned tabs.
    state->first_non_pinned_index = 1;
    state->groups =
        std::vector<tab_groups::TabGroupInfo>({tab_groups::TabGroupInfo(
            gfx::Range(2, 5),
            tab_groups::TabGroupVisualData(
                u"tab group one", tab_groups::TabGroupColorId::kGreen))});

    return state;
  }

  // Returns a PWA `DeskTemplateStatePtr` mojom for testing app launches work in
  // lacros.
  crosapi::mojom::DeskTemplateStatePtr MakeAppTestMojom() {
    crosapi::mojom::DeskTemplateStatePtr state =
        crosapi::mojom::DeskTemplateState::New();

    state->urls = std::vector<GURL>({GURL(kChromeVersionUrl)});
    state->active_index = 0;
    state->browser_app_name = kTestName;

    return state;
  }

  // Returns the result of MakeTestMojom with additional invalid tab groups.
  // The result of launching a browser with this should pass
  // AssertBrowserCreated correctly with the standard MakeTestMojom object
  // passed as the mojom to assert against.
  crosapi::mojom::DeskTemplateStatePtr MakeInvalidTabGroupTestMojom() {
    crosapi::mojom::DeskTemplateStatePtr state = MakeTestMojom();

    EXPECT_TRUE(state->groups.has_value());

    // First group violates the upper bound of the tab range.
    state->groups->emplace_back(tab_groups::TabGroupInfo(
        gfx::Range(4, 6),
        tab_groups::TabGroupVisualData(u"tab_group_two",
                                       tab_groups::TabGroupColorId::kBlue)));

    return state;
  }

  // Returns the result of MakeTestStateWithoutPinnedTabsOrTabGroup with an
  // invalid first non pinned index The result of launching a browser with this
  // should pass AssertBrowserCreated correctley with the standard
  // MakeTestStateWithoutPinnedTabsOrTabGroup object passed as the mojom to
  // assert against.
  crosapi::mojom::DeskTemplateStatePtr MakeInvalidPinnedTabTestMojom() {
    crosapi::mojom::DeskTemplateStatePtr state =
        MakeTestStateWithoutPinnedTabsOrTabGroup();

    // first non pinned index in this case will be larger than the count of
    // tabs./
    state->first_non_pinned_index = 6;

    return state;
  }

  // Request handler method, just returns a simple plaintext response when
  // invoked.  We're not testing the underlying behavior of the browser, rather
  // simply that normal URLs invoked on template launch are handled correctly.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // We're not interested in the contents of the response for this test
    // any response should be valid.
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
    return http_response;
  }

  // SetUp for test base class, initializes the embedded test server.
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  // Sets Up embedded test server to listen on IO thread.
  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kGoogleURL, "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&DeskTemplateClientLacrosBrowserTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  // Helper function that modifies the test's browser() object to match the
  // representation produced in the `MakeTestMojom` function.
  void MakeTestBrowser() {
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 0, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 2, GetGoogleTestURL(),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 3, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 4, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));

    TabStripModel* strip_model = browser()->tab_strip_model();

    // May not be necessary but guarantees that the correct tab is active.
    strip_model->ActivateTabAt(0);

    // Assert we haven't moved the pinned tab.
    EXPECT_EQ(0, strip_model->SetTabPinned(0, /*pinned=*/true));
    tab_groups::TabGroupId group_id_one = strip_model->AddToNewGroup({2, 3});

    TabGroupModel* group_model = strip_model->group_model();
    TabGroup* group_one = group_model->GetTabGroup(group_id_one);
    group_one->SetVisualData(tab_groups::TabGroupVisualData(
        u"tab group one", tab_groups::TabGroupColorId::kGreen));
  }

  // Helper function that modifies the test's browser() object to match the
  // representation produced in the `MakeTestMojomWithTabGroupAtEndOfStrip`
  // function. This helps to ensure that the entirety of Tab Groups are captured
  // properly.
  void MakeTestBrowserWithTabGroupAtEndOfStrip() {
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 0, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 2, GetGoogleTestURL(),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 3, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 4, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));

    TabStripModel* strip_model = browser()->tab_strip_model();

    // May not be necessary but guarantees that the correct tab is active.
    strip_model->ActivateTabAt(0);

    // Assert we haven't moved the pinned tab.
    EXPECT_EQ(0, strip_model->SetTabPinned(0, /*pinned=*/true));
    tab_groups::TabGroupId group_id_one = strip_model->AddToNewGroup({2, 3, 4});

    TabGroupModel* group_model = strip_model->group_model();
    TabGroup* group_one = group_model->GetTabGroup(group_id_one);
    group_one->SetVisualData(tab_groups::TabGroupVisualData(
        u"tab group one", tab_groups::TabGroupColorId::kGreen));
  }

  void MakeTestAppBrowser() {
    Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();

    Browser::CreateParams create_params = Browser::CreateParams::CreateForApp(
        kTestName, /*trusted_source=*/true, {0, 0, 256, 256}, profile,
        /*user_gesture=*/false);
    Browser::Create(create_params);

    // Close default test browser, we will set browser to the browser created
    // by this method.
    CloseBrowserSynchronously(browser());
    SelectFirstBrowser();

    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 0, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
  }

  // Instantiates a browser that when captured should assert equivalence with
  // MakeTestStateWithoutPinnedTabsOrTabGroups.
  void MakeBrowserWithoutTabgroupsOrPinnedTabs() {
    Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();

    Browser::CreateParams create_params(profile, /*user_gesture=*/false);
    create_params.initial_bounds = gfx::Rect(0, 0, 256, 256);
    create_params.are_tab_groups_enabled = false;
    Browser::Create(create_params);

    // Close default test browser, we will set browser to the browser created
    // by this method.
    CloseBrowserSynchronously(browser());
    SelectFirstBrowser();

    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 0, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 1, GURL(kAboutBlankURL),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 2, GetGoogleTestURL(),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 3, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
    EXPECT_TRUE(
        AddTabAtIndexToBrowser(browser(), 4, GURL(kChromeVersionUrl),
                               ui::PageTransition::PAGE_TRANSITION_LINK));

    TabStripModel* strip_model = browser()->tab_strip_model();

    // May not be necessary but guarantees that the correct tab is active.
    strip_model->ActivateTabAt(0);
  }
};

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesBrowserCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state = MakeTestMojom();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters = MakeTestMojom();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesBrowserWithOnlyPinnedTabsCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state =
      MakeTestMojomWithOnlyPinnedTabs();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeTestMojomWithOnlyPinnedTabs();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesBrowserWithSomePinnedTabsCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state =
      MakeTestMojomWithSomePinnedTabs();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeTestMojomWithSomePinnedTabs();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesBrowserWithTabGroupAtEndOfStripCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state =
      MakeTestMojomWithTabGroupAtEndOfStrip();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeTestMojomWithTabGroupAtEndOfStrip();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesBrowserAppCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state = MakeAppTestMojom();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters = MakeAppTestMojom();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       IgnoresInvalidTabGroups) {
  // When launched the invalid tab groups should be removed before being
  // attached to the browser, so we assert against the regular test mojom.
  crosapi::mojom::DeskTemplateStatePtr expected_state = MakeTestMojom();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeInvalidTabGroupTestMojom();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       LaunchesOldBrowserCorrectly) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state =
      MakeTestStateWithoutPinnedTabsOrTabGroup();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeTestStateWithoutPinnedTabsOrTabGroup();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       IgnoresInvalidPinnedTab) {
  // State pointers don't supply a Clone operation.  Therefore we will create
  // two semantically identical states to test against.
  crosapi::mojom::DeskTemplateStatePtr expected_state =
      MakeTestStateWithoutPinnedTabsOrTabGroup();
  crosapi::mojom::DeskTemplateStatePtr launch_parameters =
      MakeInvalidPinnedTabTestMojom();
  gfx::Rect expected_bounds(0, 0, 256, 256);

  DeskTemplateClientLacros client;

  client.CreateBrowserWithRestoredData(expected_bounds, ui::SHOW_STATE_DEFAULT,
                                       std::move(launch_parameters));

  // Close default test browser, we will set browser to the browser created
  // by the method under test.
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();

  AssertBrowserCreatedCorrectly(browser(), expected_state, expected_bounds);
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       CapturesBrowserCorrectly) {
  MakeTestBrowser();
  DeskTemplateClientLacros client;
  std::string window_id = views::DesktopWindowTreeHostLacros::From(
                              browser()->window()->GetNativeWindow()->GetHost())
                              ->platform_window()
                              ->GetWindowUniqueId();

  base::test::TestFuture<uint32_t, const std::string&,
                         crosapi::mojom::DeskTemplateStatePtr>
      future;
  client.GetBrowserInformation(/*serial=*/0, window_id, future.GetCallback());
  auto [serial, out_window_id, out_state] = future.Take();

  crosapi::mojom::DeskTemplateStatePtr test_mojom = MakeTestMojom();
  EXPECT_EQ(out_state->urls, test_mojom->urls);
  EXPECT_EQ(out_state->active_index, test_mojom->active_index);
  EXPECT_EQ(out_state->first_non_pinned_index,
            test_mojom->first_non_pinned_index);
  EXPECT_TRUE(test_mojom->groups.has_value());
  EXPECT_TRUE(out_state->groups.has_value());

  // We don't care about the order of tab groups.
  EXPECT_THAT(out_state->groups.value(),
              testing::UnorderedElementsAreArray(test_mojom->groups.value()));
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       CapturesBrowserWithTabGroupAtEndOfStripCorrectly) {
  MakeTestBrowserWithTabGroupAtEndOfStrip();
  DeskTemplateClientLacros client;
  std::string window_id = views::DesktopWindowTreeHostLacros::From(
                              browser()->window()->GetNativeWindow()->GetHost())
                              ->platform_window()
                              ->GetWindowUniqueId();

  base::test::TestFuture<uint32_t, const std::string&,
                         crosapi::mojom::DeskTemplateStatePtr>
      future;
  client.GetBrowserInformation(/*serial=*/0, window_id, future.GetCallback());
  auto [serial, out_window_id, out_state] = future.Take();

  crosapi::mojom::DeskTemplateStatePtr test_mojom =
      MakeTestMojomWithTabGroupAtEndOfStrip();
  EXPECT_EQ(out_state->urls, test_mojom->urls);
  EXPECT_EQ(out_state->active_index, test_mojom->active_index);
  EXPECT_EQ(out_state->first_non_pinned_index,
            test_mojom->first_non_pinned_index);
  ASSERT_TRUE(test_mojom->groups.has_value());
  EXPECT_TRUE(out_state->groups.has_value());

  // We don't care about the order of tab groups.
  EXPECT_THAT(out_state->groups.value(),
              testing::UnorderedElementsAreArray(test_mojom->groups.value()));
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       CapturesBrowserWithoutPinnedTabsOrTabGroupsCorrectly) {
  MakeBrowserWithoutTabgroupsOrPinnedTabs();
  DeskTemplateClientLacros client;
  std::string window_id = views::DesktopWindowTreeHostLacros::From(
                              browser()->window()->GetNativeWindow()->GetHost())
                              ->platform_window()
                              ->GetWindowUniqueId();

  base::test::TestFuture<uint32_t, const std::string&,
                         crosapi::mojom::DeskTemplateStatePtr>
      future;
  client.GetBrowserInformation(/*serial=*/0, window_id, future.GetCallback());
  auto [serial, out_window_id, out_state] = future.Take();

  crosapi::mojom::DeskTemplateStatePtr test_mojom =
      MakeTestStateWithoutPinnedTabsOrTabGroup();
  EXPECT_EQ(out_state->urls, test_mojom->urls);
  EXPECT_EQ(out_state->active_index, test_mojom->active_index);

  // first_non_pinned_index defaults to zero so it should be zero when unset.
  // This value would be ignored if launched as is asserted in tests above.
  EXPECT_EQ(out_state->first_non_pinned_index, 0u);

  // This state should not contain tab groups.
  EXPECT_FALSE(out_state->groups.has_value());
}

IN_PROC_BROWSER_TEST_F(DeskTemplateClientLacrosBrowserTest,
                       CapturesBrowserAppCorrectly) {
  MakeTestAppBrowser();
  DeskTemplateClientLacros client;
  std::string window_id = views::DesktopWindowTreeHostLacros::From(
                              browser()->window()->GetNativeWindow()->GetHost())
                              ->platform_window()
                              ->GetWindowUniqueId();

  base::test::TestFuture<uint32_t, const std::string&,
                         crosapi::mojom::DeskTemplateStatePtr>
      future;
  client.GetBrowserInformation(/*serial=*/0, window_id, future.GetCallback());
  auto [serial, out_window_id, out_state] = future.Take();

  crosapi::mojom::DeskTemplateStatePtr test_mojom = MakeAppTestMojom();
  EXPECT_EQ(out_state->urls, test_mojom->urls);
  EXPECT_EQ(out_state->browser_app_name, test_mojom->browser_app_name);
}
