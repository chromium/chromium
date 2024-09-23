// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using infobars::InfoBarDelegate;

namespace {

class FakeInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  explicit FakeInfoBarDelegate(
      infobars::InfoBarDelegate::InfoBarIdentifier identifier)
      : identifier_(identifier) {}

  ~FakeInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return identifier_;
  }

  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override {
    return delegate->GetIdentifier() == GetIdentifier();
  }

  infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
};

std::unique_ptr<infobars::InfoBar> CreateInfoBar(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto infobar_delegate = std::make_unique<FakeInfoBarDelegate>(identifier);
  return std::make_unique<infobars::InfoBar>(std::move(infobar_delegate));
}

const base::circular_deque<std::string>& GetEvents() {
  return breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
}

size_t GetNumEvents() {
  return GetEvents().size();
}

}  // namespace

// Test fixture for BreadcrumbManagerTabHelper class.
class BreadcrumbManagerTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    second_web_contents_ = CreateTestWebContents();
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
    infobars::ContentInfoBarManager::CreateForWebContents(
        second_web_contents_.get());
    BreadcrumbManagerTabHelper::CreateForWebContents(web_contents());
    BreadcrumbManagerTabHelper::CreateForWebContents(
        second_web_contents_.get());
  }

  void TearDown() override {
    second_web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> second_web_contents_;

  const GURL kTestURL = GURL("https://test");
};

// Tests that the identifiers returned for different WebContents are unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueIdentifiers) {
  const int first_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebContents(web_contents())
          ->GetUniqueId();
  const int second_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebContents(second_web_contents_.get())
          ->GetUniqueId();

  EXPECT_GT(first_tab_identifier, 0);
  EXPECT_GT(second_tab_identifier, 0);
  EXPECT_NE(first_tab_identifier, second_tab_identifier);
}

// Tests that BreadcrumbManagerTabHelper events are logged to the
// associated BreadcrumbManagerKeyedService.
TEST_F(BreadcrumbManagerTabHelperTest, EventsLogged) {
  ASSERT_EQ(0u, GetNumEvents());

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  simulator->Start();

  auto events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();

  simulator->Commit();
  events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  events.pop_back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
}

// Tests that BreadcrumbManagerTabHelper events logged from separate
// WebContents are unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueEvents) {
  auto first_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  first_simulator->Start();
  auto second_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, second_web_contents_.get());
  second_simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_STRNE(events.front().c_str(), events.back().c_str());
  EXPECT_NE(std::string::npos,
            events.front().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.front();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
}

// Tests metadata for www.google.com navigation.
TEST_F(BreadcrumbManagerTabHelperTest, GoogleNavigationStart) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://www.google.com"), web_contents());
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos,
            events.front().find(breadcrumbs::kBreadcrumbGoogleNavigation))
      << events.front();
}

// Tests metadata for https://play.google.com/ navigation.
TEST_F(BreadcrumbManagerTabHelperTest, GooglePlayNavigationStart) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://play.google.com/"), web_contents());
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  // #google is useful to indicate SRP. There is no need to know URLs of other
  // visited google properties.
  EXPECT_EQ(std::string::npos,
            events.front().find(breadcrumbs::kBreadcrumbGoogleNavigation))
      << events.front();
}

// TODO(crbug.com/40740494): special handling is needed for new-tab-page tests
// on Android, as it uses a different new-tab URL.
#if !BUILDFLAG(IS_ANDROID)
// Tests metadata for chrome://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, ChromeNewTabNavigationStart) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(chrome::kChromeUINewTabURL), web_contents());
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos,
            events.front().find(base::StringPrintf(
                "%s%" PRIu64, breadcrumbs::kBreadcrumbDidStartNavigation,
                simulator->GetNavigationHandle()->GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos,
            events.front().find(breadcrumbs::kBreadcrumbNtpNavigation))
      << events.front();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests unique ID in DidStartNavigation and DidFinishNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationUniqueId) {
  ASSERT_EQ(0u, GetNumEvents());
  // DidStartNavigation
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  simulator->Start();
  auto events = GetEvents();
  ASSERT_EQ(1u, events.size());
  const int64_t navigation_id =
      simulator->GetNavigationHandle()->GetNavigationId();
  EXPECT_NE(std::string::npos,
            events.front().find(base::StringPrintf(
                "%s%" PRIu64, breadcrumbs::kBreadcrumbDidStartNavigation,
                navigation_id)))
      << events.front();
  // DidFinishNavigation
  simulator->Commit();
  events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  events.pop_back();
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%" PRIu64, breadcrumbs::kBreadcrumbDidFinishNavigation,
                navigation_id)))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByUser) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      kTestURL, web_contents()->GetPrimaryMainFrame());
  simulator->SetHasUserGesture(true);
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos, events.back().find("#link")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(
      std::string::npos,
      events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByScript) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      kTestURL, web_contents()->GetPrimaryMainFrame());
  simulator->SetHasUserGesture(false);
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos, events.back().find("#link")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_NE(
      std::string::npos,
      events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests browser initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, BrowserInitiatedByScript) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_TYPED);
  simulator->Start();
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos, events.back().find("#typed")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(
      std::string::npos,
      events.back().find(breadcrumbs::kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests PDF load.
TEST_F(BreadcrumbManagerTabHelperTest, PdfLoad) {
  ASSERT_EQ(0u, GetNumEvents());
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  simulator->SetContentsMimeType("application/pdf");
  simulator->Commit();
  const auto& events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPdfLoad))
      << events.back();
}

// Tests page load succeess.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadSuccess) {
  ASSERT_EQ(0u, GetNumEvents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kTestURL);
  const auto& events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests page load failure.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadFailure) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      kTestURL, web_contents());
  simulator->Start();
  ASSERT_EQ(1u, GetNumEvents());

  static_cast<content::TestWebContents*>(web_contents())
      ->GetPrimaryMainFrame()
      ->DidFailLoadWithError(kTestURL, net::ERR_ABORTED);
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoadFailure))
      << events.back();
}

// TODO(crbug.com/40740494): special handling is needed for new-tab-page tests
// on Android, as it uses a different new-tab URL. Tests NTP page load.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(BreadcrumbManagerTabHelperTest, NtpPageLoad) {
  ASSERT_EQ(0u, GetNumEvents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(chrome::kChromeUINewTabURL));
  const auto& events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbNtpNavigation))
      << events.back();
  // NTP navigation can't fail, so there is no success/failure metadata.
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbPageLoadFailure))
      << events.back();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests navigation error.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationError) {
  ASSERT_EQ(0u, GetNumEvents());
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), kTestURL, net::ERR_INTERNET_DISCONNECTED);
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(net::ErrorToShortString(
                                   net::ERR_INTERNET_DISCONNECTED)))
      << events.back();
}

// Tests that adding an infobar logs the expected breadcrumb.
TEST_F(BreadcrumbManagerTabHelperTest, AddInfobar) {
  ASSERT_EQ(0u, GetNumEvents());
  const auto identifier = InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier));
  const auto& events = GetEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", breadcrumbs::kBreadcrumbInfobarAdded, identifier)))
      << events.back();
}

// Tests that infobar breadcrumbs specify the infobar type.
TEST_F(BreadcrumbManagerTabHelperTest, InfobarTypes) {
  ASSERT_EQ(0u, GetNumEvents());
  // Add and remove first infobar.
  const auto first_identifier =
      InfoBarDelegate::InfoBarIdentifier::SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(first_identifier));
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->RemoveAllInfoBars(/*animate=*/false);
  // Add second infobar.
  const auto second_identifier =
      InfoBarDelegate::InfoBarIdentifier::SYNC_ERROR_INFOBAR_DELEGATE_IOS;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(second_identifier));
  const auto& events = GetEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_NE(events.front(), events.back());
  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%d", breadcrumbs::kBreadcrumbInfobarAdded,
                                   first_identifier)))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(base::StringPrintf(
                                   "%s%d", breadcrumbs::kBreadcrumbInfobarAdded,
                                   second_identifier)))
      << events.back();
}

// Tests that removing an infobar without animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarNotAnimated) {
  ASSERT_EQ(0u, GetNumEvents());
  const auto identifier = InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier));
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->RemoveAllInfoBars(/*animate=*/false);
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", breadcrumbs::kBreadcrumbInfobarRemoved, identifier)))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that removing an infobar with animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarAnimated) {
  ASSERT_EQ(0u, GetNumEvents());
  const auto identifier = InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier));
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->RemoveAllInfoBars(/*animate=*/true);
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", breadcrumbs::kBreadcrumbInfobarRemoved, identifier)))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(breadcrumbs::kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that replacing an infobar logs the expected breadcrumb event.
TEST_F(BreadcrumbManagerTabHelperTest, ReplaceInfobar) {
  ASSERT_EQ(0u, GetNumEvents());
  const auto identifier = InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier));
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier),
                   /*replace_existing=*/true);
  const auto& events = GetEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", breadcrumbs::kBreadcrumbInfobarReplaced, identifier)))
      << events.back();
}

// Tests that replacing an infobar many times only logs the replaced infobar
// breadcrumb at major increments.
TEST_F(BreadcrumbManagerTabHelperTest, SequentialInfobarReplacements) {
  ASSERT_EQ(0u, GetNumEvents());
  const auto identifier = InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  infobars::ContentInfoBarManager::FromWebContents(web_contents())
      ->AddInfoBar(CreateInfoBar(identifier));
  for (int replacements = 0; replacements < 500; replacements++) {
    infobars::ContentInfoBarManager::FromWebContents(web_contents())
        ->AddInfoBar(CreateInfoBar(identifier),
                     /*replace_existing=*/true);
  }
  const auto& events = GetEvents();
  // Replacing the infobar 500 times should only log breadcrumbs on the 1st,
  // 2nd, 5th, 20th, 100th, 200th replacement.
  ASSERT_EQ(7u, events.size());
  // The events should contain the number of times the info has been replaced.
  // Validate the last one, which occurs at the 200th replacement.
  const std::string expected_event = base::StringPrintf(
      "%s%d %d", breadcrumbs::kBreadcrumbInfobarReplaced, identifier, 200);
  EXPECT_NE(std::string::npos, events.back().find(expected_event))
      << events.back();
}
