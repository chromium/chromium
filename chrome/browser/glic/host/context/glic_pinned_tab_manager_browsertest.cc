// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"

#include "base/containers/fixed_flat_map.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Pointee;
using testing::Property;
using testing::Return;

namespace glic {

namespace {

MATCHER_P(HasTitle, title, "") {
  if (!arg->tab_data->title.has_value()) {
    *result_listener << "has no title";
    return false;
  }
  if (arg->tab_data->title.value() != title) {
    *result_listener << "has title "
                     << testing::PrintToString(arg->tab_data->title.value());
    return false;
  }
  return true;
}

template <typename Observer, typename Matcher>
void ExpectThatEventually(Observer& observer, const Matcher& matcher) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta timeout = base::Seconds(5);

  while (base::TimeTicks::Now() - start_time < timeout) {
    auto value = observer.Take();
    if (testing::Matches(matcher)(value)) {
      SUCCEED();
      return;
    }
  }
  ADD_FAILURE() << "Timed out waiting for value to match.";
}

constexpr auto kUrlToTitleMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        {"/why-cats-are-liquid", "The Physics of Feline Fluid Dynamics"},
        {"/sentient-toaster-manual", "My Toaster Is Evil: A User's Guide"},
        {"/zombie-squirrels", "The Looming Threat of the Undead Rodent"},
        {"/how-to-train-your-goldfish", "Advanced Goldfish Obedience Training"},
        {"/the-art-of-the-nap", "Competitive Napping: A Professional's Guide"},
        {"/advanced-sock-puppetry", "Guide to Advanced Sock Puppetry"},
        {"/pigeon-espionage",
         "Pigeons Aren't Real: The Government Drone Conspiracy"},
    });

}  // namespace

class FakePinCandidatesObserver : public mojom::PinCandidatesObserver {
 public:
  FakePinCandidatesObserver() = default;
  ~FakePinCandidatesObserver() override = default;

  mojo::PendingRemote<mojom::PinCandidatesObserver> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  std::vector<mojom::PinCandidatePtr> Take() { return future_.Take(); }

  bool IsReady() { return future_.IsReady(); }

  // mojom::PinCandidatesObserver:
  void OnPinCandidatesChanged(
      std::vector<mojom::PinCandidatePtr> candidates) override {
    future_.SetValue(std::move(candidates));
  }

 private:
  base::test::TestFuture<std::vector<mojom::PinCandidatePtr>> future_;
  mojo::Receiver<mojom::PinCandidatesObserver> receiver_{this};
};

class GlicPinnedTabManagerWithOverrides : public GlicPinnedTabManager {
 public:
  using GlicPinnedTabManager::GlicPinnedTabManager;
  MOCK_METHOD(bool,
              IsBrowserValidForSharing,
              (BrowserWindowInterface*),
              (override));
  MOCK_METHOD(bool,
              IsValidForSharing,
              (content::WebContents*),
              (override));
  MOCK_METHOD(bool, IsGlicWindowShowing, (), (override));
};

class GlicPinnedTabManagerBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicPinnedTabManagerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    NonInteractiveGlicTest::SetUpOnMainThread();
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&GlicPinnedTabManagerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    https_server_handle_ = https_server_.StartAndReturnHandle();
    ASSERT_TRUE(https_server_handle_);

    auto* metrics = glic_service()->metrics();
    pinned_tab_manager_ = std::make_unique<GlicPinnedTabManagerWithOverrides>(
        browser()->profile(), /*window_controller=*/nullptr, metrics);
    ON_CALL(*pinned_tab_manager_, IsBrowserValidForSharing(_))
        .WillByDefault(Return(true));
    // TODO(mcrouse): Add tests for invalid candidates once testing harness for sharing manager is enabled.
    ON_CALL(*pinned_tab_manager_, IsValidForSharing(_))
        .WillByDefault(Return(true));

    ON_CALL(*pinned_tab_manager_, IsGlicWindowShowing())
        .WillByDefault(Return(false));
  }

  void TearDownOnMainThread() override {
    pinned_tab_manager_.reset();
    NonInteractiveGlicTest::TearDownOnMainThread();
  }

  // Helper function to create, navigate, set title, and add a new tab to the
  // current browser's tab strip.
  void CreateAndAddTab(const std::string& url_path) {
    GURL url = https_server_.GetURL(url_path);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto it = kUrlToTitleMap.find(request.relative_url);
    if (it == kUrlToTitleMap.end()) {
      return nullptr;
    }
    auto title = it->second;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content("<html><head><title>" + std::string(title) +
                          "</title></head><body></body></html>");
    return response;
  }

  net::EmbeddedTestServer https_server_;
  net::test_server::EmbeddedTestServerHandle https_server_handle_;
  std::unique_ptr<GlicPinnedTabManagerWithOverrides> pinned_tab_manager_;
};

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       ReturnsMultipleCandidatesSortedByActivation) {
  // By default, the browser starts with a single tab open to "about:blank".
  CreateAndAddTab("/why-cats-are-liquid");
  CreateAndAddTab("/sentient-toaster-manual");
  CreateAndAddTab("/zombie-squirrels");

  FakePinCandidatesObserver observer;
  auto options = mojom::GetPinCandidatesOptions::New();
  options->max_candidates = 3;
  pinned_tab_manager_->SubscribeToPinCandidates(std::move(options),
                                                observer.Bind());

  // The initial list should be sorted by creation time. Because the max number
  // of candidates is 3, the initial "about:blank" tab is not included.
  ExpectThatEventually(
      observer, ElementsAre(HasTitle("The Looming Threat of the Undead Rodent"),
                            HasTitle("My Toaster Is Evil: A User's Guide"),
                            HasTitle("The Physics of Feline Fluid Dynamics")));

  // Activate the oldest tab (index 1, since "about:blank" is at 0).
  browser()->tab_strip_model()->ActivateTabAt(1);

  // The activated tab should now be at the front of the list.
  ExpectThatEventually(
      observer, ElementsAre(HasTitle("The Physics of Feline Fluid Dynamics"),
                            HasTitle("The Looming Threat of the Undead Rodent"),
                            HasTitle("My Toaster Is Evil: A User's Guide")));
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       SortsCandidatesByQuery) {
  // By default, the browser starts with a single tab open to "about:blank".
  CreateAndAddTab("/how-to-train-your-goldfish");
  CreateAndAddTab("/the-art-of-the-nap");
  CreateAndAddTab("/advanced-sock-puppetry");
  CreateAndAddTab("/pigeon-espionage");

  FakePinCandidatesObserver observer;
  auto options = mojom::GetPinCandidatesOptions::New();
  options->max_candidates = 4;
  options->query = "Guide";
  pinned_tab_manager_->SubscribeToPinCandidates(std::move(options),
                                                observer.Bind());

  // The list should be sorted by match type. Because the max number of
  // candidates is 4, the initial "about:blank" tab is not included.
  ExpectThatEventually(
      observer,
      ElementsAre(
          HasTitle("Guide to Advanced Sock Puppetry"),
          HasTitle("Competitive Napping: A Professional's Guide"),
          HasTitle("Pigeons Aren't Real: The Government Drone Conspiracy"),
          HasTitle("Advanced Goldfish Obedience Training")));
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest, PinTabs) {
  CreateAndAddTab("/why-cats-are-liquid");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future;
  auto subscription = pinned_tab_manager_->AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  // Pin a tab and verify it was pinned.
  EXPECT_TRUE(pinned_tab_manager_->PinTabs({tab_handle}));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(1u, pinned_tab_manager_->GetNumPinnedTabs());


  // Check that the callback was called with pinned=true.
  {
    auto [result_interface, result_pinned] = pin_status_future.Get();
    EXPECT_EQ(tab_interface, result_interface);
    EXPECT_TRUE(result_pinned);
  }
}

// Ensure that a pinned tab can be dragged out to another window without
// crashing.
IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       DragPinnedTabsToNewWindow) {
  // We intentionally create a second window.
  browser_activator().SetMode(BrowserActivator::Mode::kFirst);

  CreateAndAddTab("/why-cats-are-liquid");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  EXPECT_TRUE(pinned_tab_manager_->PinTabs({tab_handle}));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));

  chrome::MoveTabsToNewWindow(browser(), {1});
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest, unpinTabs) {
  CreateAndAddTab("/why-cats-are-liquid");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::GetFromContents(tab_strip_model->GetWebContentsAt(1));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  // Pin a tab and verify it was pinned.
  EXPECT_TRUE(pinned_tab_manager_->PinTabs({tab_handle}));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(1u, pinned_tab_manager_->GetNumPinnedTabs());


  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future;
  auto subscription = pinned_tab_manager_->AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  // Unpin the tab and verify it was unpinned.
  EXPECT_TRUE(pinned_tab_manager_->UnpinTabs({tab_handle}));
  EXPECT_FALSE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(0u, pinned_tab_manager_->GetNumPinnedTabs());

  // Check that the callback was called with pinned=false.
  {
    auto [result_interface, result_pinned] = pin_status_future.Get();
    EXPECT_EQ(tab_interface, result_interface);
    EXPECT_FALSE(result_pinned);
  }
}

}  // namespace glic
