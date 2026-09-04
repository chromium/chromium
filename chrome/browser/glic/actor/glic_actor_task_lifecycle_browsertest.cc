// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/browser/glic/actor/new_glic_actor_functional_browsertest.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/performance_manager/public/features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace mojo {

base::Value ConvertToValue(const page_content_annotations::ScreenshotOptions::
                               ScreenshotCollectionOptions& in) {
  base::Value raw_out(base::Value::Type::DICT);
  base::DictValue& out = raw_out.GetDict();
  out.Set("maxWidth", static_cast<int>(in.max_width.value_or(0)));
  out.Set("maxHeight", static_cast<int>(in.max_height.value_or(0)));
  out.Set("screenshotImageFormat",
          static_cast<int>(in.screenshot_image_format.value_or(
              page_content_annotations::ScreenshotOptions::
                  ScreenshotImageFormat::kJpeg)));
  out.Set("screenshotCompressionQuality",
          static_cast<int>(in.screenshot_compression_quality.value_or(
              page_content_annotations::ScreenshotOptions::
                  ScreenshotCompressionQuality::kMedium)));
  return raw_out;
}

template <>
struct TypeConverter<base::Value, glic::mojom::TabContextOptions> {
  static base::Value Convert(const glic::mojom::TabContextOptions& in) {
    base::Value raw_out(base::Value::Type::DICT);
    base::DictValue& out = raw_out.GetDict();
    out.Set("innerText", in.inner_text);
    out.Set("innerTextBytesLimit", static_cast<int>(in.inner_text_bytes_limit));
    out.Set("viewportScreenshot", in.viewport_screenshot);
    out.Set("annotatedPageContent", in.annotated_page_content);
    out.Set("maxMetaTags", static_cast<int>(in.max_meta_tags));
    out.Set("pdfData", in.pdf_data);
    out.Set("pdfSizeLimit", static_cast<int>(in.pdf_size_limit));
    out.Set("annotatedPageContentMode",
            static_cast<int>(in.annotated_page_content_mode));
    out.Set("screenshotCollectionOptions",
            ConvertToValue(in.screenshot_collection_options));
    return raw_out;
  }
};
}  // namespace mojo

namespace glic::actor {
namespace {

using ::actor::ActorTask;
using ::actor::TaskId;
using ::base::test::TestFuture;
using optimization_guide::proto::Actions;

// Helper class to observe journal entries and wait for a specific condition.
class JournalObserver : public ::actor::AggregatedJournal::Observer {
 public:
  using Predicate =
      base::RepeatingCallback<bool(const ::actor::mojom::JournalEntry&)>;

  explicit JournalObserver(::actor::AggregatedJournal* journal)
      : journal_(journal) {
    journal_->AddObserver(this);
  }

  ~JournalObserver() override { journal_->RemoveObserver(this); }

  void WillAddJournalEntry(
      const ::actor::AggregatedJournal::Entry& entry) override {
    entries_.push_back(entry.data.Clone());
    if (wait_predicate_ && wait_predicate_.Run(*entry.data)) {
      if (run_loop_) {
        run_loop_->Quit();
      }
    }
  }

  // Waits until a journal entry matching the predicate is observed.
  void WaitUntil(Predicate predicate) {
    for (const auto& entry : entries_) {
      if (predicate.Run(*entry)) {
        return;
      }
    }
    wait_predicate_ = std::move(predicate);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    wait_predicate_.Reset();
  }

 private:
  raw_ptr<::actor::AggregatedJournal> journal_;
  std::vector<::actor::mojom::JournalEntryPtr> entries_;
  Predicate wait_predicate_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

bool JournalEntryHasError(const ::actor::mojom::JournalEntry& entry,
                          const std::string& error_message) {
  for (const auto& detail : entry.details) {
    if (detail->key == "error" && detail->value == error_message) {
      return true;
    }
  }
  return false;
}

class PrefInvarianceScope {
 public:
  explicit PrefInvarianceScope(PrefService* prefs)
      : prefs_(prefs),
        initial_enabled_(prefs->GetBoolean(
            autofill::prefs::kAutofillGmailOtpFillingEnabled)),
        initial_dismiss_timestamp_(prefs->GetTime(
            autofill::prefs::
                kAutofillGmailOtpFillingActivationDismissalTimestamp)) {
    registrar_.Init(prefs_);
    auto on_pref_changed = base::BindRepeating(
        &PrefInvarianceScope::OnPrefChanged, base::Unretained(this));
    registrar_.Add(autofill::prefs::kAutofillGmailOtpFillingEnabled,
                   on_pref_changed);
    registrar_.Add(
        autofill::prefs::kAutofillGmailOtpFillingActivationDismissalTimestamp,
        on_pref_changed);
  }

  void VerifyInvariance() {
    EXPECT_EQ(pref_change_count_, 0)
        << "Preference invariance violated: " << pref_change_count_
        << " change notifications fired.";
    EXPECT_EQ(
        prefs_->GetBoolean(autofill::prefs::kAutofillGmailOtpFillingEnabled),
        initial_enabled_)
        << "kAutofillGmailOtpFillingEnabled value unexpectedly changed.";
    EXPECT_EQ(prefs_->GetTime(
                  autofill::prefs::
                      kAutofillGmailOtpFillingActivationDismissalTimestamp),
              initial_dismiss_timestamp_)
        << "kAutofillGmailOtpFillingActivationDismissalTimestamp value "
           "unexpectedly changed.";
  }

  int change_count() const { return pref_change_count_; }

 private:
  void OnPrefChanged() { pref_change_count_++; }

  raw_ptr<PrefService> prefs_;
  const bool initial_enabled_;
  const base::Time initial_dismiss_timestamp_;
  int pref_change_count_ = 0;
  PrefChangeRegistrar registrar_;
};

class TestJournalObserver : public ::actor::AggregatedJournal::Observer {
 public:
  explicit TestJournalObserver(::actor::AggregatedJournal* journal)
      : journal_(journal) {
    journal_->AddObserver(this);
  }

  ~TestJournalObserver() override { journal_->RemoveObserver(this); }

  void WillAddJournalEntry(
      const ::actor::AggregatedJournal::Entry& entry) override {
    std::string s = base::StrCat({"Event: ", entry.data->event, ";"});
    for (const auto& details_entry : entry.data->details) {
      base::StrAppend(&s, {details_entry->key, "=", details_entry->value, ";"});
    }
    entries_.push_back(std::move(s));
  }

  const std::vector<std::string>& Entries() const { return entries_; }

 private:
  raw_ptr<::actor::AggregatedJournal> journal_;
  std::vector<std::string> entries_;
};

std::optional<::actor::DomNode> GetDomNodeOnPage(
    content::RenderFrameHost& rfh,
    std::string_view query_selector) {
  ASSIGN_OR_RETURN(int node_id, content::GetDOMNodeId(rfh, query_selector));
  ASSIGN_OR_RETURN(
      std::string document_identifier,
      optimization_guide::DocumentIdentifierUserData::GetDocumentIdentifier(
          rfh.GetGlobalFrameToken()));
  return ::actor::DomNode{
      .node_id = node_id,
      .document_identifier = std::move(document_identifier)};
}

class GlicActorTaskLifecycleFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorTaskLifecycleFunctionalBrowserTest()
      : GlicActorFunctionalBrowserTestBase(
            GlicTestJsPath("./glic_actor_task_lifecycle_browsertest.js")) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {blink::features::kAIPageContentTrackedElementsIframe, {}},
            {page_content_annotations::kGlicTabScreenshotExperiment,
             {{"screenshot_timeout_ms", "30s"}}},
            {performance_manager::features::kGlicActuationPriorityVoter, {}},
        },
        /*disabled_features=*/{});
  }
  ~GlicActorTaskLifecycleFunctionalBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    GlicActorFunctionalBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicActorTaskLifecycleGmailOtpEnabledBrowserTest
    : public GlicActorTaskLifecycleFunctionalBrowserTest {
 public:
  GlicActorTaskLifecycleGmailOtpEnabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActorAutofillOneTimePassword,
         autofill::features::kGlicActorAutofill,
         ::actor::kGlicActorSkipScreenshot},
        /*disabled_features=*/{});
  }
  ~GlicActorTaskLifecycleGmailOtpEnabledBrowserTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    GlicActorTaskLifecycleFunctionalBrowserTest::
        SetUpBrowserContextKeyedServices(context);
    AffiliationServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
    autofill::OneTimeTokenServiceFactory::GetInstance()
        ->SetTestingSubclassFactoryAndUse<
            one_time_tokens::MockOneTimeTokenService>(
            context,
            base::BindOnce(&GlicActorTaskLifecycleGmailOtpEnabledBrowserTest::
                               CreateMockOtpService));
  }

  void SetUpOnMainThread() override {
    GlicActorTaskLifecycleFunctionalBrowserTest::SetUpOnMainThread();

    autofill::prefs::SetAutofillGmailOtpFillingEnabled(GetProfile()->GetPrefs(),
                                                       true);
    SeedTestServerAffiliation("example.com");

    EXPECT_CALL(GetMockOtpService(),
                Subscribe(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(
            [](one_time_tokens::OneTimeTokenSource source,
               base::Time expiration,
               one_time_tokens::OneTimeTokenService::Callback callback,
               base::OnceClosure expiration_callback) {
              return one_time_tokens::ExpiringSubscription();
            });
    EXPECT_CALL(GetMockOtpService(), GetRecentOneTimeTokens(testing::_))
        .WillRepeatedly(
            [](one_time_tokens::OneTimeTokenService::Callback callback) {});
    EXPECT_CALL(GetMockOtpService(), GetCachedOneTimeTokens())
        .WillRepeatedly(
            []() { return std::vector<one_time_tokens::OneTimeToken>(); });
  }

  affiliations::FakeAffiliationService* fake_affiliation_service() {
    return static_cast<affiliations::FakeAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(GetProfile()));
  }

  void SeedTestServerAffiliation(std::string_view host) {
    GURL test_url = embedded_https_test_server().GetURL(host, "/");
    std::string test_spec = url::Origin::Create(test_url).Serialize();
    std::string standard_spec = base::StrCat({"https://", host});
    fake_affiliation_service()->AddAffiliationGroup({
        affiliations::Facet(
            affiliations::FacetURI::FromCanonicalSpec(test_spec)),
        affiliations::Facet(
            affiliations::FacetURI::FromCanonicalSpec(standard_spec)),
    });
  }

  static std::unique_ptr<one_time_tokens::MockOneTimeTokenService>
  CreateMockOtpService(content::BrowserContext* context) {
    return std::make_unique<
        testing::NiceMock<one_time_tokens::MockOneTimeTokenService>>();
  }

  one_time_tokens::MockOneTimeTokenService& GetMockOtpService() {
    auto* mock_otp_service =
        static_cast<one_time_tokens::MockOneTimeTokenService*>(
            autofill::OneTimeTokenServiceFactory::GetForProfile(GetProfile()));
    CHECK(mock_otp_service);
    return *mock_otp_service;
  }

  void SetMockOtpResponse(const std::string& otp) {
    EXPECT_CALL(GetMockOtpService(),
                Subscribe(one_time_tokens::OneTimeTokenSource::kGmail,
                          testing::_, testing::_, testing::_))
        .WillOnce([otp](one_time_tokens::OneTimeTokenSource source,
                        base::Time expiration,
                        one_time_tokens::OneTimeTokenService::Callback callback,
                        base::OnceClosure expiration_callback) {
          callback.Run(
              one_time_tokens::OneTimeTokenSource::kGmail,
              base::expected<one_time_tokens::OneTimeToken,
                             one_time_tokens::OneTimeTokenRetrievalError>(
                  one_time_tokens::OneTimeToken(
                      one_time_tokens::OneTimeTokenType::kGmail, otp,
                      base::TimeTicks::Now(), "sender@example.com")));
          return one_time_tokens::ExpiringSubscription();
        });
  }

  void SetGmailOtpConfirmationResponseMode(std::string_view mode) {
    GlicInstanceImpl* instance = GetInstanceImpl();
    ASSERT_TRUE(instance);
    content::WebContents* guest_contents =
        instance->host().web_client_contents();
    ASSERT_TRUE(guest_contents);
    ASSERT_TRUE(content::ExecJs(
        guest_contents,
        base::StrCat(
            {"window.setGmailOtpConfirmationResponseMode('", mode, "');"})));
  }

  void WaitForTabObservation(TaskId task_id) {
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
    content::WaitForCopyableViewInWebContents(web_contents());
    TestFuture<::actor::ActorKeyedService::TabObservationResult>
        tab_observation_future;
    actor_keyed_service()->RequestTabObservation(
        *active_tab(), task_id, std::nullopt,
        tab_observation_future.GetCallback());
    const ::actor::ActorKeyedService::TabObservationResult& result =
        tab_observation_future.Get();
    std::optional<std::string> error_message =
        ::actor::ActorKeyedService::ExtractErrorMessageIfFailed(result);
    ASSERT_FALSE(error_message)
        << "Waiting for tab observation failed: " << *error_message;
    ASSERT_TRUE(result.value());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeCreatedTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

// TODO(b/484011242): Fix flakiness and re-enable this test on Android and Mac.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_testPauseAndResumeCreatedTaskWithIframe \
  DISABLED_testPauseAndResumeCreatedTaskWithIframe
#else
#define MAYBE_testPauseAndResumeCreatedTaskWithIframe \
  testPauseAndResumeCreatedTaskWithIframe
#endif
IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       MAYBE_testPauseAndResumeCreatedTaskWithIframe) {
  ASSERT_TRUE(
      content::NavigateToURL(active_tab()->GetContents(),
                             embedded_https_test_server().GetURL(
                                 "example.com", "/actor/simple_iframe.html")));

  content::RenderFrameHost* main_frame =
      active_tab()->GetContents()->GetPrimaryMainFrame();

  // Wait for main frame layout/render.
  {
    base::test::TestFuture<bool> future;
    main_frame->GetRenderWidgetHost()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
  }

  // Wait for child frame layout/render.
  {
    content::RenderFrameHost* child_frame =
        content::ChildFrameAt(main_frame, 0);
    ASSERT_TRUE(child_frame);

    base::test::TestFuture<bool> future;
    child_frame->GetRenderWidgetHost()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait())
        << "Timeout waiting for syncing with subframe renderer";
  }

  content::WaitForCopyableViewInWebContents(active_tab()->GetContents());

  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());

  ExecuteJsTest();

  // Pausing an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "PauseActorTask" &&
               JournalEntryHasError(entry,
                                    "Task ID does not match current task");
      }));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseAndResumeInactiveTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription completion_subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  completion_subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  JournalObserver observer(&actor_keyed_service()->GetJournal());

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFinished state.";

  // Pausing an inactive task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "PauseActorTask" &&
               JournalEntryHasError(entry,
                                    "Task ID does not match current task");
      }));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testPauseActiveTask) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testStopActiveTaskWithModelError) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFailed, task_completion_state.Get())
      << "Task " << task_id << " did not reach kFailed state.";
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptInvalidTask) {
  JournalObserver observer(&actor_keyed_service()->GetJournal());
  TaskId invalid_task_id = TaskId(12345);
  ASSERT_EQ(actor_keyed_service()->GetTask(invalid_task_id), nullptr);

  ExecuteJsTest();

  // Interrupting an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "InterruptActorTask" &&
               JournalEntryHasError(entry,
                                    "Task ID does not match current task");
      }));

  ContinueJsTest();

  // Uninterrupting an invalid task should be a no-op and log an error.
  observer.WaitUntil(
      base::BindRepeating([](const ::actor::mojom::JournalEntry& entry) {
        return entry.event == "UninterruptActorTask" &&
               JournalEntryHasError(entry,
                                    "Task ID does not match current task");
      }));
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptTaskWithCompletedActions) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptAndUninterruptActiveTaskAndPerformActions) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testInterruptWithReasons) {
  TestFuture<ActorTask::State> task_completion_state;
  base::CallbackListSubscription subscription;

  ExecuteJsTest();

  TaskId task_id = ExtractTaskIdFromStepData();
  subscription =
      CreateTaskCompletionSubscription(task_id, task_completion_state);

  ContinueJsTest();

  EXPECT_EQ(ActorTask::State::kFinished, task_completion_state.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testActuatingChangedCallback) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  TestFuture<bool> actuating_true_future;
  TestFuture<bool> actuating_false_future;

  base::CallbackListSubscription subscription =
      task_manager->AddActuatingChangedCallback(
          base::BindLambdaForTesting([&](bool actuating) {
            if (actuating) {
              actuating_true_future.SetValue(true);
            } else {
              actuating_false_future.SetValue(false);
            }
          }));

  ExecuteJsTest();

  // After the task is created, verify the callback receives true.
  EXPECT_TRUE(actuating_true_future.Get());

  ContinueJsTest();

  // After the task is stopped, verify the callback receives false.
  EXPECT_FALSE(actuating_false_future.Get());
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testActivateTabWithConversationUsesActorState) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  PreventDeletionOnClose(instance, "test_conversation_id");

  // Execute JS test to create the task.
  ExecuteJsTest();

  tabs::TabInterface* first_tab = active_tab();
  ASSERT_TRUE(first_tab);

  // Open a second tab so we can test focusing.
  tabs::TabInterface* second_tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(second_tab);
  ASSERT_NE(second_tab, first_tab);
  EXPECT_EQ(active_tab(), second_tab);

  // Make the task act on the FIRST tab.
  ContinueJsTest({.instance = instance});

  // Now the first tab should be in LastActedTabs.

  // Call ActivateTabWithConversation.
  auto activate_result =
      coordinator().ActivateTabWithConversation("test_conversation_id");

  EXPECT_EQ(GlicInstanceCoordinator::ActivateTabResult::kSuccess,
            activate_result);

  // Verify that the FIRST tab is now active (since it was the last acted tab).
  EXPECT_EQ(active_tab(), first_tab);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpOptInDialog) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test, which sets up the subscriber and calls advanceToNextStep()
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  // Get the GlicActorClientSession (which implements ActorTaskDelegate)
  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpOptInResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpOptInDialog(task_id,
                                             response_future.GetCallback());

  // Continue JS test, which awaits the dialog request promise and completes it.
  ContinueJsTest();

  // Verify the callback in C++ receives the correct approved response
  ::actor::webui::mojom::GmailOtpOptInResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_response());
  EXPECT_TRUE(response->get_response()->permission_granted);

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpOptInDialogNoSubscriber) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test (does nothing and calls advanceToNextStep())
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  // Get the GlicActorClientSession
  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpOptInResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpOptInDialog(task_id,
                                             response_future.GetCallback());

  // Verify that the callback resolves with the correct error reason (no
  // subscriber)
  ::actor::webui::mojom::GmailOtpOptInResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_error_reason());
  EXPECT_EQ(
      ::actor::webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber,
      response->get_error_reason());

  // Continue JS test to finish the JS runner thread cleanly.
  ContinueJsTest();

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testGmailOtpOptInDialogFeatureDisabled) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test (asserts selectGmailOtpOptInRequestHandler is undefined)
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  // Get the GlicActorClientSession
  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpOptInResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpOptInDialog(task_id,
                                             response_future.GetCallback());

  // Verify that the callback resolves with the correct error reason (no
  // subscriber)
  ::actor::webui::mojom::GmailOtpOptInResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_error_reason());
  EXPECT_EQ(
      ::actor::webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber,
      response->get_error_reason());

  // Continue JS test to finish the JS runner thread cleanly.
  ContinueJsTest();

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationDialog) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test, which sets up the subscriber and calls
  // advanceToNextStep()
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpConfirmationResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpConfirmationDialog(
      task_id, "123456", response_future.GetCallback());

  // Continue JS test, which awaits the dialog request promise and completes
  // it.
  ContinueJsTest();

  // Verify the callback in C++ receives the correct approved response
  ::actor::webui::mojom::GmailOtpConfirmationResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_response());
  EXPECT_TRUE(response->get_response()->permission_granted);

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationDialogNoSubscriber) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test (does nothing and calls advanceToNextStep())
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpConfirmationResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpConfirmationDialog(
      task_id, "123456", response_future.GetCallback());

  // Verify that the callback resolves with the correct error reason (no
  // subscriber)
  ::actor::webui::mojom::GmailOtpConfirmationResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_error_reason());
  EXPECT_EQ(
      ::actor::webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber,
      response->get_error_reason());

  // Continue JS test to finish the JS runner thread cleanly.
  ContinueJsTest();

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testGmailOtpConfirmationDialogFeatureDisabled) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());

  // Execute JS test (asserts selectGmailOtpConfirmationRequestHandler is
  // undefined)
  ExecuteJsTest();

  GlicActorTaskManager* task_manager = instance->GetActorTaskManager();
  ASSERT_TRUE(task_manager);

  ::actor::ActorTaskDelegate* delegate =
      task_manager->GetClientSessionForTesting();
  ASSERT_TRUE(delegate);

  base::test::TestFuture<::actor::webui::mojom::GmailOtpConfirmationResultPtr>
      response_future;
  delegate->RequestToShowGmailOtpConfirmationDialog(
      task_id, "123456", response_future.GetCallback());

  // Verify that the callback resolves with the correct error reason (no
  // subscriber)
  ::actor::webui::mojom::GmailOtpConfirmationResultPtr response =
      response_future.Take();
  ASSERT_TRUE(response->is_error_reason());
  EXPECT_EQ(
      ::actor::webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber,
      response->get_error_reason());

  // Continue JS test to finish the JS runner thread cleanly.
  ContinueJsTest();

  task_manager->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationAcceptFillsOtpSuccessfully) {
  TestJournalObserver observer(&actor_keyed_service()->GetJournal());

  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  ExecuteJsTest();

  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation(task_id));

  ASSERT_OK_AND_ASSIGN(
      ::actor::DomNode otp_field,
      GetDomNodeOnPage(*web_contents()->GetPrimaryMainFrame(), "#otp"));

  PrefInvarianceScope pref_scope(GetProfile()->GetPrefs());

  SetGmailOtpConfirmationResponseMode("accept");

  const std::string kExpectedOtp = "482910";
  SetMockOtpResponse(kExpectedOtp);

  std::unique_ptr<::actor::ToolRequest> request =
      std::make_unique<::actor::AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(),
          std::vector<::actor::PageTarget>{otp_field},
          /*for_signin=*/false);

  ::actor::ActResultFuture result;
  task->Act(::actor::ToRequestList(std::move(request)), result.GetCallback());

  ::actor::ExpectOkResult(result);

  EXPECT_EQ(
      content::EvalJs(web_contents(), "document.querySelector('#otp').value"),
      kExpectedOtp);

  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*for_signin=false")));
  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::OnGmailOtpConfirmationResponse;.*"
                  "permission_granted=true")));
  EXPECT_THAT(
      observer.Entries(),
      testing::Contains(testing::ContainsRegex(
          "AttemptOtpFillingTool::OnOtpRetrieved;.*otp_received=true")));

  pref_scope.VerifyInvariance();

  ContinueJsTest();

  instance->GetActorTaskManager()->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationDeclineAbortsFilling) {
  TestJournalObserver observer(&actor_keyed_service()->GetJournal());

  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  ExecuteJsTest();

  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation(task_id));

  ASSERT_OK_AND_ASSIGN(
      ::actor::DomNode otp_field,
      GetDomNodeOnPage(*web_contents()->GetPrimaryMainFrame(), "#otp"));

  PrefInvarianceScope pref_scope(GetProfile()->GetPrefs());

  SetGmailOtpConfirmationResponseMode("decline");
  SetMockOtpResponse("123456");

  std::unique_ptr<::actor::ToolRequest> request =
      std::make_unique<::actor::AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(),
          std::vector<::actor::PageTarget>{otp_field},
          /*for_signin=*/false);

  ::actor::ActResultFuture result;
  task->Act(::actor::ToRequestList(std::move(request)), result.GetCallback());

  ::actor::ExpectErrorResult(
      result,
      ::actor::mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling);

  EXPECT_EQ(
      content::EvalJs(web_contents(), "document.querySelector('#otp').value"),
      "");

  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*for_signin=false")));
  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::OnGmailOtpConfirmationResponse;.*"
                  "permission_granted=false")));

  pref_scope.VerifyInvariance();

  ContinueJsTest();

  instance->GetActorTaskManager()->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationErrorHandling) {
  TestJournalObserver observer(&actor_keyed_service()->GetJournal());

  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
  EXPECT_NE(task_id, TaskId());
  ActorTask* task = actor_keyed_service()->GetTask(task_id);
  ASSERT_TRUE(task);

  ExecuteJsTest();

  const GURL url = embedded_https_test_server().GetURL("example.com",
                                                       "/actor/otp_page.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_NO_FATAL_FAILURE(WaitForTabObservation(task_id));

  ASSERT_OK_AND_ASSIGN(
      ::actor::DomNode otp_field,
      GetDomNodeOnPage(*web_contents()->GetPrimaryMainFrame(), "#otp"));

  PrefInvarianceScope pref_scope(GetProfile()->GetPrefs());

  SetGmailOtpConfirmationResponseMode("error");
  SetMockOtpResponse("123456");

  std::unique_ptr<::actor::ToolRequest> request =
      std::make_unique<::actor::AttemptOtpFillingToolRequest>(
          active_tab()->GetHandle(),
          std::vector<::actor::PageTarget>{otp_field},
          /*for_signin=*/false);

  ::actor::ActResultFuture result;
  task->Act(::actor::ToRequestList(std::move(request)), result.GetCallback());

  ::actor::ExpectErrorResult(
      result, ::actor::mojom::ActionResultCode::kOtpUnableToFill);

  EXPECT_EQ(
      content::EvalJs(web_contents(), "document.querySelector('#otp').value"),
      "");

  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::Invoke;.*for_signin=false")));
  EXPECT_THAT(observer.Entries(),
              testing::Contains(testing::ContainsRegex(
                  "AttemptOtpFillingTool::OnGmailOtpConfirmationResponse;.*"
                  "error_reason=")));

  pref_scope.VerifyInvariance();

  ContinueJsTest();

  instance->GetActorTaskManager()->GetClientSessionForTesting()->StopActorTask(
      task_id.value(), glic::mojom::ActorTaskStopReason::kTaskComplete);
}

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleGmailOtpEnabledBrowserTest,
                       testGmailOtpConfirmationAssertNoPreferenceMutations) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);

  ExecuteJsTest();

  struct TestCase {
    std::string name;
    bool initial_opt_in_enabled;
    base::Time initial_dismiss_timestamp;
    std::string response_mode;
    std::string mock_otp;
    std::string expected_dom_value;
  };

  const std::vector<TestCase> test_cases = {
      {
          .name = "PreOptedIn_Accept",
          .initial_opt_in_enabled = true,
          .initial_dismiss_timestamp = base::Time(),
          .response_mode = "accept",
          .mock_otp = "445566",
          .expected_dom_value = "445566",
      },
      {
          .name = "PreOptedIn_Decline",
          .initial_opt_in_enabled = true,
          .initial_dismiss_timestamp = base::Time(),
          .response_mode = "decline",
          .mock_otp = "123456",
          .expected_dom_value = "",
      },
      {
          .name = "PreOptedIn_WithDismissalTimestamp_Accept",
          .initial_opt_in_enabled = true,
          .initial_dismiss_timestamp = base::Time::Now() - base::Days(10),
          .response_mode = "accept",
          .mock_otp = "778899",
          .expected_dom_value = "778899",
      },
      {
          .name = "PreOptedIn_WithDismissalTimestamp_Decline",
          .initial_opt_in_enabled = true,
          .initial_dismiss_timestamp = base::Time::Now() - base::Days(10),
          .response_mode = "decline",
          .mock_otp = "123456",
          .expected_dom_value = "",
      },
  };

  PrefService* prefs = GetProfile()->GetPrefs();

  for (const auto& tc : test_cases) {
    SCOPED_TRACE(tc.name);

    ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateActorTask(instance));
    EXPECT_NE(task_id, TaskId());
    ActorTask* task = actor_keyed_service()->GetTask(task_id);
    ASSERT_TRUE(task);

    const GURL url = embedded_https_test_server().GetURL(
        "example.com", "/actor/otp_page.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
    ASSERT_NO_FATAL_FAILURE(WaitForTabObservation(task_id));

    ASSERT_OK_AND_ASSIGN(
        ::actor::DomNode otp_field,
        GetDomNodeOnPage(*web_contents()->GetPrimaryMainFrame(), "#otp"));

    ASSERT_TRUE(content::ExecJs(web_contents(),
                                "document.querySelector('#otp').value = '';"));

    prefs->SetBoolean(autofill::prefs::kAutofillGmailOtpFillingEnabled,
                      tc.initial_opt_in_enabled);
    prefs->SetTime(
        autofill::prefs::kAutofillGmailOtpFillingActivationDismissalTimestamp,
        tc.initial_dismiss_timestamp);

    PrefInvarianceScope pref_scope(prefs);

    SetGmailOtpConfirmationResponseMode(tc.response_mode);
    SetMockOtpResponse(tc.mock_otp);

    std::unique_ptr<::actor::ToolRequest> request =
        std::make_unique<::actor::AttemptOtpFillingToolRequest>(
            active_tab()->GetHandle(),
            std::vector<::actor::PageTarget>{otp_field},
            /*for_signin=*/false);

    ::actor::ActResultFuture result;
    task->Act(::actor::ToRequestList(std::move(request)), result.GetCallback());

    if (tc.response_mode == "accept") {
      ::actor::ExpectOkResult(result);
    } else {
      ::actor::ExpectErrorResult(
          result,
          ::actor::mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling);
    }

    EXPECT_EQ(
        content::EvalJs(web_contents(), "document.querySelector('#otp').value"),
        tc.expected_dom_value);

    pref_scope.VerifyInvariance();

    instance->GetActorTaskManager()
        ->GetClientSessionForTesting()
        ->StopActorTask(task_id.value(),
                        glic::mojom::ActorTaskStopReason::kTaskComplete);
  }

  ContinueJsTest();
}

#if !BUILDFLAG(IS_ANDROID)
[[nodiscard]] TestResult<void> RunUntilPriorityIs(
    content::RenderProcessHost* rph,
    base::Process::Priority priority) {
  return RunUntilEqual([&]() { return rph->GetPriority(); }, priority);
}
#else
[[nodiscard]] TestResult<void> RunUntilImportanceIs(
    content::RenderProcessHost* rph,
    content::ChildProcessImportance importance) {
  return RunUntilEqual([&]() { return rph->GetEffectiveImportance(); },
                       importance);
}

bool IsProtectRecentlyVisibleTabEnabled() {
  return base::android::device_info::is_desktop() ||
         base::FeatureList::IsEnabled(
             chrome::android::kProtectRecentlyVisibleTab);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicActorTaskLifecycleFunctionalBrowserTest,
                       testActuatingPriorityChange) {
  GlicInstanceImpl* instance = GetInstanceImpl();
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));
  GlicSidePanelCoordinator* coordinator =
      GlicSidePanelCoordinator::GetForTab(active_tab());
  bool supports_peek = coordinator && coordinator->SupportsPeek();

  content::WebContents* webui_contents = instance->host().webui_contents();
  ASSERT_TRUE(webui_contents);
  content::RenderProcessHost* webui_rph =
      webui_contents->GetPrimaryMainFrame()->GetProcess();
  content::WebContents* guest_contents = instance->host().web_client_contents();
  EXPECT_TRUE(guest_contents);
  content::RenderProcessHost* guest_rph =
      guest_contents->GetPrimaryMainFrame()->GetProcess();

  // Close glic to ensure the priority is reduced.
  PreventDeletionOnClose(instance);
  ToggleGlicForActiveTab();
  EXPECT_OK(WaitForGlicClose());
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::HIDDEN));
  EXPECT_EQ(guest_contents->GetVisibility(), content::Visibility::HIDDEN);

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_OK(
      RunUntilPriorityIs(webui_rph, base::Process::Priority::kBestEffort));
  EXPECT_OK(
      RunUntilPriorityIs(guest_rph, base::Process::Priority::kBestEffort));
#else

  // When the feature is enabled, recently visible pages are protected.
  content::ChildProcessImportance expected_importance_when_hidden =
      IsProtectRecentlyVisibleTabEnabled()
          ? content::ChildProcessImportance::NOT_PERCEPTIBLE
          : content::ChildProcessImportance::NORMAL;

  EXPECT_OK(RunUntilImportanceIs(webui_rph, expected_importance_when_hidden));
  // TODO(crbug.com/525435394): Ensure the guest process is not protected.
  EXPECT_OK(RunUntilImportanceIs(
      guest_rph, content::ChildProcessImportance::NOT_PERCEPTIBLE));
#endif
  ExecuteJsTest();

  // Task is now created and actuating. The process priority should be boosted.
  EXPECT_TRUE(instance->IsActuating());

  // On platforms that do not support peek mode, the side panel re-opens when
  // a tab is added to the actuation. Close it again to test priority boosting
  // when hidden.
  if (!supports_peek) {
    ToggleGlicForActiveTab();
    EXPECT_OK(WaitForGlicClose());
    EXPECT_OK(
        WaitForWebUiContentsVisibility(instance, content::Visibility::HIDDEN));
    EXPECT_EQ(guest_contents->GetVisibility(), content::Visibility::HIDDEN);
  } else {
    EXPECT_EQ(webui_contents->GetVisibility(), content::Visibility::HIDDEN);
    EXPECT_EQ(guest_contents->GetVisibility(), content::Visibility::HIDDEN);
  }

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_OK(
      RunUntilPriorityIs(webui_rph, base::Process::Priority::kUserBlocking));
  EXPECT_OK(
      RunUntilPriorityIs(guest_rph, base::Process::Priority::kUserBlocking));
#else
  EXPECT_OK(RunUntilImportanceIs(webui_rph,
                                 content::ChildProcessImportance::IMPORTANT));
  EXPECT_OK(RunUntilImportanceIs(guest_rph,
                                 content::ChildProcessImportance::IMPORTANT));
#endif

  // Open a second tab to make the initial tab hidden. The priority of glic
  // renderers should lower, but not drop to best effort.
  auto* first_tab = active_tab();
  auto* second_tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(second_tab);
  ASSERT_NE(second_tab, first_tab);
  EXPECT_EQ(active_tab(), second_tab);

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_OK(
      RunUntilPriorityIs(webui_rph, base::Process::Priority::kUserVisible));
  EXPECT_OK(
      RunUntilPriorityIs(guest_rph, base::Process::Priority::kUserVisible));
#else
  EXPECT_OK(RunUntilImportanceIs(webui_rph,
                                 content::ChildProcessImportance::MODERATE));
  EXPECT_OK(RunUntilImportanceIs(guest_rph,
                                 content::ChildProcessImportance::MODERATE));
#endif

  // Finish/stop the task.
  ContinueJsTest({.instance = instance});
  EXPECT_FALSE(instance->IsActuating());

  // Now Glic is not actuating, so the priority should drop.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_OK(
      RunUntilPriorityIs(webui_rph, base::Process::Priority::kBestEffort));
  EXPECT_OK(
      RunUntilPriorityIs(guest_rph, base::Process::Priority::kBestEffort));
#else
  EXPECT_OK(RunUntilImportanceIs(webui_rph, expected_importance_when_hidden));
  // TODO(crbug.com/525435394): Ensure the guest process is not protected.
  EXPECT_OK(RunUntilImportanceIs(
      guest_rph, content::ChildProcessImportance::NOT_PERCEPTIBLE));
#endif
}

}  // namespace
}  // namespace glic::actor
