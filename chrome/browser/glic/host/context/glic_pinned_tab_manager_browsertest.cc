// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"

#include "base/containers/fixed_flat_map.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/ui/browser.h"
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

class MockGlicSharingManager : public GlicSharingManager {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              AddFocusedTabChangedCallback,
              (FocusedTabChangedCallback),
              (override));
  MOCK_METHOD(FocusedTabData, GetFocusedTabData, (), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddTabPinningStatusChangedCallback,
              (TabPinningStatusChangedCallback),
              (override));
  MOCK_METHOD(bool, PinTabs, (base::span<const tabs::TabHandle>), (override));
  MOCK_METHOD(bool, UnpinTabs, (base::span<const tabs::TabHandle>), (override));
  MOCK_METHOD(void, UnpinAllTabs, (), (override));
  MOCK_METHOD(int32_t, GetMaxPinnedTabs, (), (const, override));
  MOCK_METHOD(int32_t, GetNumPinnedTabs, (), (const, override));
  MOCK_METHOD(bool, IsTabPinned, (tabs::TabHandle), (const, override));
  MOCK_METHOD(bool,
              IsBrowserValidForSharing,
              (BrowserWindowInterface*),
              (override));
};

class GlicPinnedTabManagerBrowserTest : public InProcessBrowserTest {
 public:
  GlicPinnedTabManagerBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&GlicPinnedTabManagerBrowserTest::HandleRequest,
                            base::Unretained(this)));
    https_server_handle_ = https_server_.StartAndReturnHandle();
    ASSERT_TRUE(https_server_handle_);

    sharing_manager_ =
        std::make_unique<testing::NiceMock<MockGlicSharingManager>>();
    ON_CALL(*sharing_manager_, IsBrowserValidForSharing(_))
        .WillByDefault(Return(true));
    pinned_tab_manager_ = std::make_unique<GlicPinnedTabManager>(
        browser()->profile(), sharing_manager_.get());
  }

  void TearDownOnMainThread() override {
    pinned_tab_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
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
  std::unique_ptr<MockGlicSharingManager> sharing_manager_;
  std::unique_ptr<GlicPinnedTabManager> pinned_tab_manager_;
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

}  // namespace glic
