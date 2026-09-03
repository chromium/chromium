// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

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

class GlicPinnedTabManagerWithOverrides : public GlicPinnedTabManagerImpl {
 public:
  using GlicPinnedTabManagerImpl::GlicPinnedTabManagerImpl;
  MOCK_METHOD(bool,
              IsBrowserValidForSharing,
              (BrowserWindowInterface*),
              (override));
  MOCK_METHOD(bool, IsValidForSharing, (tabs::TabInterface*), (override));
  MOCK_METHOD(bool, IsGlicWindowShowing, (), (override));
};

class GlicPinnedTabManagerBrowserTest : public GlicBrowserTest {
 public:
  GlicPinnedTabManagerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&GlicPinnedTabManagerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    https_server_handle_ = https_server_.StartAndReturnHandle();
    ASSERT_TRUE(https_server_handle_);

    auto* metrics = service()->metrics();
    pinned_tab_manager_ = std::make_unique<GlicPinnedTabManagerWithOverrides>(
        GetProfile(), /*window_controller=*/nullptr, metrics);
    ON_CALL(*pinned_tab_manager_, IsBrowserValidForSharing(_))
        .WillByDefault(Return(true));
    // TODO(mcrouse): Add tests for invalid candidates once testing harness for
    // sharing manager is enabled.
    ON_CALL(*pinned_tab_manager_, IsValidForSharing(_))
        .WillByDefault(Return(true));

    ON_CALL(*pinned_tab_manager_, IsGlicWindowShowing())
        .WillByDefault(Return(false));
  }

  void TearDownOnMainThread() override {
    pinned_tab_manager_.reset();
    GlicBrowserTest::TearDownOnMainThread();
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

// TODO(b/534710453): Re-enable this test on Android. Currently flaky.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ReturnsMultipleCandidatesSortedByActivation \
  DISABLED_ReturnsMultipleCandidatesSortedByActivation
#else
#define MAYBE_ReturnsMultipleCandidatesSortedByActivation \
  ReturnsMultipleCandidatesSortedByActivation
#endif
IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       MAYBE_ReturnsMultipleCandidatesSortedByActivation) {
  // By default, the browser starts with a single tab open to "about:blank".
  tabs::TabInterface* tab_1 =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  CreateAndActivateTab(https_server_.GetURL("/sentient-toaster-manual"));
  tabs::TabInterface* tab_3 =
      CreateAndActivateTab(https_server_.GetURL("/zombie-squirrels"));

  FakePinCandidatesObserver observer;
  auto options = mojom::GetPinCandidatesOptions::New();
  options->max_candidates = 3;
  pinned_tab_manager_->SubscribeToPinCandidates(std::move(options),
                                                observer.Bind());

  // Set up the ordering so toggling between them is predictable.
  GetTabListInterface()->ActivateTab(tab_3->GetHandle());
  GetTabListInterface()->ActivateTab(tab_1->GetHandle());

  // Toggle between the tabs a few times to make sure that it gets updated
  // for every activation event.
  for (size_t i = 0; i < 3; ++i) {
    GetTabListInterface()->ActivateTab(tab_3->GetHandle());

    // The activated tab should now be at the front of the list.
    ExpectThatEventually(
        observer,
        ElementsAre(HasTitle("The Looming Threat of the Undead Rodent"),
                    HasTitle("The Physics of Feline Fluid Dynamics"),
                    HasTitle("My Toaster Is Evil: A User's Guide")));

    GetTabListInterface()->ActivateTab(tab_1->GetHandle());

    // The activated tab should now be at the front of the list.
    ExpectThatEventually(
        observer,
        ElementsAre(HasTitle("The Physics of Feline Fluid Dynamics"),
                    HasTitle("The Looming Threat of the Undead Rodent"),
                    HasTitle("My Toaster Is Evil: A User's Guide")));
  }
}

// TODO(b/534710453): Re-enable this test on Android. Currently flaky.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SortsCandidatesByQuery DISABLED_SortsCandidatesByQuery
#else
#define MAYBE_SortsCandidatesByQuery SortsCandidatesByQuery
#endif
IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       MAYBE_SortsCandidatesByQuery) {
  // By default, the browser starts with a single tab open to "about:blank".
  CreateAndActivateTab(https_server_.GetURL("/how-to-train-your-goldfish"));
  CreateAndActivateTab(https_server_.GetURL("/the-art-of-the-nap"));
  CreateAndActivateTab(https_server_.GetURL("/advanced-sock-puppetry"));
  CreateAndActivateTab(https_server_.GetURL("/pigeon-espionage"));

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
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future;
  auto subscription = pinned_tab_manager_->AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  // Pin a tab and verify it was pinned.
  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
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
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));

  BrowserWindowInterface* new_window = CreateBrowserWindow(GetProfile());
  ASSERT_TRUE(new_window);

  GetTabListInterface()->MoveTabToWindow(tab_handle, new_window->GetSessionID(),
                                         /*destination_index=*/0);
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest, unpinTabs) {
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  // Pin a tab and verify it was pinned.
  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(1u, pinned_tab_manager_->GetNumPinnedTabs());

  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future;
  auto subscription = pinned_tab_manager_->AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  // Unpin the tab and verify it was unpinned.
  EXPECT_TRUE(
      pinned_tab_manager_->UnpinTabs({tab_handle}, GlicUnpinTrigger::kUnknown));
  EXPECT_FALSE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(0u, pinned_tab_manager_->GetNumPinnedTabs());

  // Check that the callback was called with pinned=false.
  {
    auto [result_interface, result_pinned] = pin_status_future.Get();
    EXPECT_EQ(tab_interface, result_interface);
    EXPECT_FALSE(result_pinned);
  }
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       UnpinTabOnTabDestroyed) {
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  // Pin a tab and verify it was pinned.
  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(1u, pinned_tab_manager_->GetNumPinnedTabs());

  base::test::TestFuture<tabs::TabInterface*, bool> pin_status_future;
  auto subscription = pinned_tab_manager_->AddTabPinningStatusChangedCallback(
      pin_status_future.GetRepeatingCallback());

  // Close all tabs, which should destroy them.
  for (auto* tab : GetTabListInterface()->GetAllTabs()) {
    GetTabListInterface()->CloseTab(tab->GetHandle());
  }

  // Check that the callback was called with pinned=false.
  {
    auto [result_interface, result_pinned] = pin_status_future.Get();
    EXPECT_EQ(tab_interface, result_interface);
    EXPECT_FALSE(result_pinned);
  }

  // Verify the tab was unpinned.
  EXPECT_FALSE(pinned_tab_manager_->IsTabPinned(tab_handle));
  EXPECT_EQ(0u, pinned_tab_manager_->GetNumPinnedTabs());
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       VerifyPinnedStatePersistsOnRestore) {
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));

  // Switch to another tab to ensure the pinned tab is in the background.
  CreateAndActivateTab(https_server_.GetURL("/sentient-toaster-manual"));
  EXPECT_NE(GetTabListInterface()->GetActiveTab(), tab_interface);

  // Discard the pinned tab to simulate a situation where it needs to be
  // restored.
  content::WebContents* discarded_contents =
      GetTabListInterface()->DiscardTab(tab_handle);
  ASSERT_TRUE(discarded_contents);
  EXPECT_TRUE(discarded_contents->WasDiscarded());

  // Activate the tab to trigger a restore (reload).
  GetTabListInterface()->ActivateTab(tab_handle);
  EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab_interface);
  content::WaitForLoadStop(tab_interface->GetContents());

  // Verify the tab remains pinned.
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));
}

IN_PROC_BROWSER_TEST_F(GlicPinnedTabManagerBrowserTest,
                       VerifyUnpinningOnBackgroundOriginChange) {
  tabs::TabInterface* tab_interface =
      CreateAndActivateTab(https_server_.GetURL("/why-cats-are-liquid"));
  ASSERT_TRUE(tab_interface);
  const tabs::TabHandle tab_handle = tab_interface->GetHandle();

  EXPECT_TRUE(
      pinned_tab_manager_->PinTabs({tab_handle}, GlicPinTrigger::kUnknown));
  EXPECT_TRUE(pinned_tab_manager_->IsTabPinned(tab_handle));

  // Switch to another tab to ensure the pinned tab is in the background.
  CreateAndActivateTab(https_server_.GetURL("/sentient-toaster-manual"));
  EXPECT_NE(GetTabListInterface()->GetActiveTab(), tab_interface);

  // Navigate the pinned tab to a different origin.
  GURL new_origin_url("data:text/html,<html><body>New Origin</body></html>");
  {
    content::NavigationController::LoadURLParams params(new_origin_url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    tab_interface->GetContents()->GetController().LoadURLWithParams(params);
    content::WaitForLoadStop(tab_interface->GetContents());
  }

  // Verify the tab is unpinned.
  EXPECT_FALSE(pinned_tab_manager_->IsTabPinned(tab_handle));
}

}  // namespace glic
