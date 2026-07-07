// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/shared_types.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::TestFuture;

namespace actor {

namespace {

// The Journal only keeps the last 20 entries in the its buffer. If we want to
// use the journal entries for assertions, we need to observe and store them as
// they happen.
class TestJournalObserver : public AggregatedJournal::Observer {
 public:
  explicit TestJournalObserver(AggregatedJournal* journal) : journal_(journal) {
    journal->AddObserver(this);
  }

  ~TestJournalObserver() override { journal_->RemoveObserver(this); }

  void WillAddJournalEntry(const AggregatedJournal::Entry& entry) override {
    // We copy the data from the entry, because we don't own the entry.
    std::string s = base::StrCat({"Event: ", entry.data->event, ";"});

    for (const auto& details_entry : entry.data->details) {
      base::StrAppend(&s, {details_entry->key, "=", details_entry->value, ";"});
    }
    entries_.push_back(std::move(s));
  }

  const std::vector<std::string>& Entries() const { return entries_; }

 private:
  raw_ptr<AggregatedJournal> journal_;
  std::vector<std::string> entries_;
};

// Note: There's a MockOneTimeTokenService for OneTimeTokenService (not -Impl)
// but that mock does not implement the KeyedService. So we need our own mock
// of the -Impl with the KeyedService so that we can use the KeyedService
// factory for injection.
class MockKeyedOneTimeTokenService
    : public one_time_tokens::OneTimeTokenServiceImpl {
 public:
  MockKeyedOneTimeTokenService() : OneTimeTokenServiceImpl(nullptr, nullptr) {}
  ~MockKeyedOneTimeTokenService() override = default;

  MOCK_METHOD(one_time_tokens::ExpiringSubscription,
              Subscribe,
              (one_time_tokens::OneTimeTokenSource,
               base::Time,
               one_time_tokens::OneTimeTokenService::Callback),
              (override));
};

class AttemptOtpFillingToolBrowserTest : public ActorToolsTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ActorToolsTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
                        return std::make_unique<
                            affiliations::FakeAffiliationService>();
                      }));
                }));
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();

    observer_ = std::make_unique<TestJournalObserver>(
        &actor_keyed_service().GetJournal());

    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    observer_.reset();

    ActorToolsTest::TearDownOnMainThread();
  }

  const std::vector<std::string>& JournalEntries() const {
    return observer_->Entries();
  }

  static std::unique_ptr<MockKeyedOneTimeTokenService> CreateMockOtpService(
      content::BrowserContext* context) {
    return std::make_unique<testing::NiceMock<MockKeyedOneTimeTokenService>>();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    autofill::OneTimeTokenServiceFactory::GetInstance()
        ->SetTestingSubclassFactoryAndUse<MockKeyedOneTimeTokenService>(
            context, base::BindOnce(&CreateMockOtpService));
  }

  // Waits for the background page analysis (AnnotatedPageContent) to be
  // completed and indexed for the current tab. This is required before
  // attempting to resolve PageTargets to stable identifiers like
  // FieldGlobalIds, ensuring the tool acts on a fully analyzed state.
  void WaitForTabObservation() {
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
    content::WaitForCopyableViewInWebContents(web_contents());
    TestFuture<ActorKeyedService::TabObservationResult> tab_observation_future;
    actor_keyed_service().RequestTabObservation(
        *active_tab(), actor_task().id(), std::nullopt,
        tab_observation_future.GetCallback());
    const ActorKeyedService::TabObservationResult& result =
        tab_observation_future.Get();
    std::optional<std::string> error_message =
        ActorKeyedService::ExtractErrorMessageIfFailed(result);
    ASSERT_FALSE(error_message)
        << "Waiting for tab observation failed: " << *error_message;
    ASSERT_TRUE(result.value());
  }

 protected:
  MockKeyedOneTimeTokenService& GetMockOtpService() {
    auto* mock_otp_service = static_cast<MockKeyedOneTimeTokenService*>(
        autofill::OneTimeTokenServiceFactory::GetForProfile(GetProfile()));
    CHECK(mock_otp_service);
    return *mock_otp_service;
  }

  affiliations::FakeAffiliationService* fake_affiliation_service() {
    return static_cast<affiliations::FakeAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(GetProfile()));
  }

  void SetExpectedOtp(std::optional<std::string> otp) {
    EXPECT_CALL(GetMockOtpService(),
                Subscribe(one_time_tokens::OneTimeTokenSource::kGmail,
                          testing::_, testing::_))
        .WillOnce(
            [otp](one_time_tokens::OneTimeTokenSource source,
                  base::Time expiration,
                  one_time_tokens::OneTimeTokenService::Callback callback) {
              if (otp) {
                callback.Run(one_time_tokens::OneTimeTokenSource::kGmail,
                             one_time_tokens::OneTimeToken(
                                 one_time_tokens::OneTimeTokenType::kGmail,
                                 *otp, base::TimeTicks::Now()));
              } else {
                callback.Run(
                    one_time_tokens::OneTimeTokenSource::kGmail,
                    base::unexpected(
                        one_time_tokens::OneTimeTokenRetrievalError::kUnknown));
              }
              return one_time_tokens::ExpiringSubscription();
            });
  }

  bool HasJournalEntryWithDetails(std::string_view event,
                                  std::string_view detail_key_value) {
    for (const std::string& entry : JournalEntries()) {
      if (entry.find(base::StrCat({"Event: ", event, ";"})) !=
              std::string::npos &&
          entry.find(detail_key_value) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

 private:
  std::unique_ptr<TestJournalObserver> observer_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
};

// Gets the dom node or returns nullopt when the node id or document token
// cannot be retrieved.
std::optional<DomNode> GetDomNodeOnPage(content::RenderFrameHost& rfh,
                                        std::string_view query_selector) {
  ASSIGN_OR_RETURN(int node_id, GetDOMNodeId(rfh, query_selector));
  ASSIGN_OR_RETURN(
      std::string document_identifier,
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  return DomNode{.node_id = node_id,
                 .document_identifier = std::move(document_identifier)};
}

// The tool can be created with one field and the task returns OK.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolGetsCreatedWithOneFieldAndTaskReturnsOk) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});
  SetExpectedOtp("1234");

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectOkResult(result);
  EXPECT_THAT(JournalEntries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*for_signin=true")));
  EXPECT_THAT(JournalEntries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*trigger_fields_count=1")));
  EXPECT_THAT(
      JournalEntries(),
      testing::Contains(testing::ContainsRegex(
          "AttemptOtpFillingTool::OnOtpRetrieved;.*otp_received=true")));
}

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ConsumesLoginContextOnInvoke) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  SetExpectedOtp("1234");

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectOkResult(result);
  EXPECT_FALSE(actor_task()
                   .GetExecutionEngine()
                   .GetActorOneTimeTokenFillingService()
                   .ConsumeLoginContext()
                   .has_value());
}

// The tool fails when OTP retrieval returns an error.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenOtpRetrievalReturnsError) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});
  SetExpectedOtp(std::nullopt);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kOtpRetrievalError);
  EXPECT_THAT(
      JournalEntries(),
      testing::Contains(testing::ContainsRegex(
          "AttemptOtpFillingTool::OnOtpRetrieved;.*otp_received=false")));
}

// The tool works when OTP retrieval is asynchronous.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolWorksWhenOtpRetrievalIsAsynchronous) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});

  EXPECT_CALL(GetMockOtpService(),
              Subscribe(one_time_tokens::OneTimeTokenSource::kGmail, testing::_,
                        testing::_))
      .WillOnce([](one_time_tokens::OneTimeTokenSource source,
                   base::Time expiration,
                   one_time_tokens::OneTimeTokenService::Callback callback) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(callback,
                           one_time_tokens::OneTimeTokenSource::kGmail,
                           one_time_tokens::OneTimeToken(
                               one_time_tokens::OneTimeTokenType::kGmail,
                               "1234", base::TimeTicks::Now())),
            base::Milliseconds(100));
        return one_time_tokens::ExpiringSubscription();
      });

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectOkResult(result);
  EXPECT_THAT(
      JournalEntries(),
      testing::Contains(testing::ContainsRegex(
          "AttemptOtpFillingTool::OnOtpRetrieved;.*otp_received=true")));
}

// The tool can be created with multiple fields (one per digit) and the
// task returns OK.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolGetsCreatedWithMultipleFieldsAndTaskReturnsOk) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field_1,
                       GetDomNodeOnPage(*main_frame(), "#otp_digit_1"));
  ASSERT_OK_AND_ASSIGN(DomNode otp_field_2,
                       GetDomNodeOnPage(*main_frame(), "#otp_digit_2"));
  ASSERT_OK_AND_ASSIGN(DomNode otp_field_3,
                       GetDomNodeOnPage(*main_frame(), "#otp_digit_3"));
  ASSERT_OK_AND_ASSIGN(DomNode otp_field_4,
                       GetDomNodeOnPage(*main_frame(), "#otp_digit_4"));
  std::vector<PageTarget> trigger_fields = {otp_field_1, otp_field_2,
                                            otp_field_3, otp_field_4};
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(active_tab()->GetHandle(),
                                                     trigger_fields,
                                                     /*for_signin=*/true);
  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});
  SetExpectedOtp("1234");

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectOkResult(result);
  EXPECT_THAT(JournalEntries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*trigger_fields_count=4")));
}

// The tool can be created with for_signin set to false.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenForSigninFalse) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/false);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kOtpSigninContextMismatch);
  EXPECT_THAT(JournalEntries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*for_signin=false")));
}

// The tool fails when the target tab is closed before invocation.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenTabIsNull) {
  // Add a new tab to the browser.
  int index = browser()->tab_strip_model()->count();
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                                  /*foreground=*/true);
  tabs::TabInterface* new_tab =
      browser()->tab_strip_model()->GetTabAtIndex(index);
  tabs::TabHandle target_tab_handle = new_tab->GetHandle();

  DomNode dummy_field = {.node_id = 1, .document_identifier = "dummy"};
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          target_tab_handle, std::vector<PageTarget>{dummy_field},
          /*for_signin=*/true);

  // Close the newly added tab.
  browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                   TabCloseTypes::CLOSE_NONE);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

// The tool fails when the tab was not observed, resulting in a null
// observation.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenNoLastTabObservation) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  // Skip WaitForTabObservation() so last_observation is null.

  DomNode dummy_field = {.node_id = 1, .document_identifier = "dummy"};
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{dummy_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingNoLastTabObservation);
}

// The tool fails when a trigger field is not found on the page.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenTriggerFieldNotFound) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  // Create a DomNode with a non-existent query selector/node_id.
  DomNode nonexistent_field = {.node_id = -1,
                               .document_identifier = "nonexistent"};
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{nonexistent_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kFormFillingFieldNotFound);
}

// TODO(crbug.com/502907795): Differentiate between no form on the page and
// filling insecure and add a test for it.

// `AttemptOtpFillingTool` fails when the form filling context is insecure (e.g.
// mixed content form submission).
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolFailsWhenFormFillingIsInsecure) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('single-otp-form').setAttribute('action', "
      "'http://example.com/simple.html');"));
  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectErrorResult(result,
                    mojom::ActionResultCode::kFormFillingUnknownAutofillError);
}

// Tests verifying if the OTP filling attempt is part of an ongoing actor login
// flow. If true, the confirmation UI is skipped because the user already
// consented. Verifies exact, affiliation, PSL matching, and page navigation
// limits.

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       IsActorLoginFlow_ExactMatch_SilentFilling) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url),
                                /*should_use_strong_matching=*/true, {});

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  SetExpectedOtp("1234");

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=true;"));
}

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       IsActorLoginFlow_Mismatch_RequiresConfirmation) {
  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  // Sign-in started on mismatch.com.
  GURL login_url = GURL("https://mismatch.com");

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(login_url),
                                /*should_use_strong_matching=*/false, {});

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kOtpSigninContextMismatch);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=false;"));
}

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       IsActorLoginFlow_AffiliationMatch_SilentFilling) {
  const GURL url = embedded_https_test_server().GetURL("b.example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  // Sign-in started on example.com.
  GURL login_url = embedded_https_test_server().GetURL("example.com", "/");

  std::string login_spec = url::Origin::Create(login_url).Serialize();
  std::string page_spec = url::Origin::Create(url).Serialize();

  // Seed affiliation group containing example.com and b.example.com.
  fake_affiliation_service()->AddAffiliationGroup({
      affiliations::Facet(
          affiliations::FacetURI::FromCanonicalSpec(login_spec)),
      affiliations::Facet(affiliations::FacetURI::FromCanonicalSpec(page_spec)),
  });

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(login_url),
                                /*should_use_strong_matching=*/true, {});

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  SetExpectedOtp("1234");

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=true;"));
}

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       IsActorLoginFlow_PslMatchStrong_RequiresConfirmation) {
  const GURL url = embedded_https_test_server().GetURL("sub1.example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  // Sign-in started on sub2.example.com.
  GURL login_url = GURL("https://sub2.example.com");

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(login_url),
                                /*should_use_strong_matching=*/true, {});

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kOtpSigninContextMismatch);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=false;"));
}

IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       IsActorLoginFlow_PslMatchWeak_SilentFilling) {
  const GURL url = embedded_https_test_server().GetURL("sub1.example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  // Sign-in started on sub2.example.com.
  GURL login_url = GURL("https://sub2.example.com");

  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(login_url),
                                /*should_use_strong_matching=*/false, {});

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);
  SetExpectedOtp("1234");

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=true;"));
}

IN_PROC_BROWSER_TEST_F(
    AttemptOtpFillingToolBrowserTest,
    IsActorLoginFlow_ExcessiveNavigations_RequiresConfirmation) {
  const GURL url1 = embedded_https_test_server().GetURL("example.com",
                                                        "/actor/otp_page.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  const GURL url3 = embedded_https_test_server().GetURL("example.com",
                                                        "/actor/otp_page.html");

  // 1. Initial navigation.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url1));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  // 2. Start tracking login flow (tracks main frame with 0 navigations).
  actor_task()
      .GetExecutionEngine()
      .GetActorOneTimeTokenFillingService()
      .OnPasswordFillingStarted(active_tab()->GetHandle(),
                                url::Origin::Create(url1),
                                /*should_use_strong_matching=*/true, {});

  // 3. First navigation (count becomes 1).
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url2));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  // 4. Second navigation (count becomes 2 - limit reached).
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url3));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation());

  ASSERT_OK_AND_ASSIGN(DomNode otp_field,
                       GetDomNodeOnPage(*main_frame(), "#otp"));

  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(), std::vector<PageTarget>{otp_field},
          /*for_signin=*/true);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kOtpSigninContextMismatch);

  EXPECT_TRUE(HasJournalEntryWithDetails(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      "is_actor_login=false;"));
}

}  // namespace
}  // namespace actor
