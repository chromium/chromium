// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_test_utils.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::RenderViewHost;

namespace prerender {

namespace test_utils {

namespace {

// An ExternalProtocolHandler that blocks everything and asserts it never is
// called.
class NeverRunsExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    NOTREACHED();
    // This will crash, but it shouldn't get this far with BlockState::BLOCK
    // anyway.
    return nullptr;
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    // Block everything and fail the test.
    ADD_FAILURE();
    return ExternalProtocolHandler::BLOCK;
  }

  void BlockRequest() override {}

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin) override {
    NOTREACHED();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    NOTREACHED();
  }

  void FinishedProcessingCheck() override { NOTREACHED(); }
};

}  // namespace

FakeSafeBrowsingDatabaseManager::FakeSafeBrowsingDatabaseManager() {}

bool FakeSafeBrowsingDatabaseManager::CheckBrowseUrl(
    const GURL& gurl,
    const safe_browsing::SBThreatTypeSet& threat_types,
    Client* client) {
  if (bad_urls_.find(gurl.spec()) == bad_urls_.end() ||
      bad_urls_[gurl.spec()] == safe_browsing::SB_THREAT_TYPE_SAFE) {
    return true;
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                     this, gurl, client));
  return false;
}

bool FakeSafeBrowsingDatabaseManager::IsSupported() const {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool FakeSafeBrowsingDatabaseManager::CanCheckResourceType(
    content::ResourceType /* resource_type */) const {
  return true;
}

bool FakeSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  return true;
}

FakeSafeBrowsingDatabaseManager::~FakeSafeBrowsingDatabaseManager() {}

void FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone(const GURL& gurl,
                                                           Client* client) {
  client->OnCheckBrowseUrlResult(gurl, bad_urls_[gurl.spec()],
                                 safe_browsing::ThreatMetadata());
}

TestPrerenderContents::TestPrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    Origin origin,
    FinalStatus expected_final_status,
    bool ignore_final_status)
    : PrerenderContents(prerender_manager,
                        profile,
                        url,
                        referrer,
                        initiator_origin,
                        origin),
      expected_final_status_(expected_final_status),
      observer_(this),
      new_render_view_host_(nullptr),
      was_hidden_(false),
      was_shown_(false),
      should_be_shown_(expected_final_status == FINAL_STATUS_USED),
      skip_final_checks_(ignore_final_status) {}

TestPrerenderContents::~TestPrerenderContents() {
  if (skip_final_checks_)
    return;

  EXPECT_EQ(expected_final_status_, final_status())
      << " when testing URL " << prerender_url().path()
      << " (Expected: " << NameFromFinalStatus(expected_final_status_)
      << ", Actual: " << NameFromFinalStatus(final_status()) << ")";

  // A used PrerenderContents will only be destroyed when we swap out
  // WebContents, at the end of a navigation caused by a call to
  // NavigateToURLImpl().
  if (final_status() == FINAL_STATUS_USED)
    EXPECT_TRUE(new_render_view_host_);

  EXPECT_EQ(should_be_shown_, was_shown_);
}

bool TestPrerenderContents::CheckURL(const GURL& url) {
  // Prevent FINAL_STATUS_UNSUPPORTED_SCHEME when navigating to about:crash in
  // the PrerenderRendererCrash test.
  if (url.spec() != content::kChromeUICrashURL)
    return PrerenderContents::CheckURL(url);
  return true;
}

void TestPrerenderContents::OnRenderViewHostCreated(
    RenderViewHost* new_render_view_host) {
  // Used to make sure the RenderViewHost is hidden and, if used,
  // subsequently shown.
  observer_.Add(new_render_view_host->GetWidget());

  new_render_view_host_ = new_render_view_host;

  PrerenderContents::OnRenderViewHostCreated(new_render_view_host);
}

void TestPrerenderContents::RenderWidgetHostVisibilityChanged(
    content::RenderWidgetHost* widget_host,
    bool became_visible) {
  EXPECT_EQ(new_render_view_host_->GetWidget(), widget_host);

  if (!became_visible) {
    was_hidden_ = true;
  } else {
    // A prerendered RenderViewHost should only be shown after being removed
    // from the PrerenderContents for display.
    EXPECT_FALSE(GetRenderViewHost());
    was_shown_ = true;
  }
}

void TestPrerenderContents::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  observer_.Remove(widget_host);
}

DestructionWaiter::DestructionWaiter(TestPrerenderContents* prerender_contents,
                                     FinalStatus expected_final_status)
    : expected_final_status_(expected_final_status),
      saw_correct_status_(false) {
  if (!prerender_contents) {
    // TODO(mattcary): It is not correct to assume the contents were destroyed
    // correctly, but until the prefetch renderer destruction race can be fixed
    // there's no other way to keep tests from flaking.
    saw_correct_status_ = true;
    return;
  }
  if (prerender_contents->final_status() != FINAL_STATUS_UNKNOWN) {
    // The contents was already destroyed by the time this was called.
    MarkDestruction(prerender_contents->final_status());
  } else {
    marker_ = std::make_unique<DestructionMarker>(this);
    prerender_contents->AddObserver(marker_.get());
  }
}

DestructionWaiter::~DestructionWaiter() {}

bool DestructionWaiter::WaitForDestroy() {
  if (!saw_correct_status_) {
    wait_loop_.Run();
  }
  return saw_correct_status_;
}

void DestructionWaiter::MarkDestruction(FinalStatus reason) {
  saw_correct_status_ = (reason == expected_final_status_);
  wait_loop_.Quit();
}

DestructionWaiter::DestructionMarker::DestructionMarker(
    DestructionWaiter* waiter)
    : waiter_(waiter) {}

DestructionWaiter::DestructionMarker::~DestructionMarker() {}

void DestructionWaiter::DestructionMarker::OnPrerenderStop(
    PrerenderContents* contents) {
  waiter_->MarkDestruction(contents->final_status());
}

TestPrerender::TestPrerender()
    : contents_(nullptr),
      final_status_(FINAL_STATUS_UNKNOWN),
      number_of_loads_(0),
      expected_number_of_loads_(0),
      started_(false),
      stopped_(false) {}

TestPrerender::~TestPrerender() {
  if (contents_)
    contents_->RemoveObserver(this);
}

FinalStatus TestPrerender::GetFinalStatus() const {
  if (contents_)
    return contents_->final_status();
  return final_status_;
}

void TestPrerender::WaitForCreate() {
  if (contents_)
    return;
  create_loop_.Run();
}

void TestPrerender::WaitForStart() {
  if (started_)
    return;
  start_loop_.Run();
}

void TestPrerender::WaitForStop() {
  if (stopped_)
    return;
  stop_loop_.Run();
}

void TestPrerender::WaitForLoads(int expected_number_of_loads) {
  DCHECK(!load_waiter_);
  DCHECK(!expected_number_of_loads_);
  if (number_of_loads_ < expected_number_of_loads) {
    load_waiter_.reset(new base::RunLoop);
    expected_number_of_loads_ = expected_number_of_loads;
    load_waiter_->Run();
    load_waiter_.reset();
    expected_number_of_loads_ = 0;
  }
  EXPECT_LE(expected_number_of_loads, number_of_loads_);
}

void TestPrerender::OnPrerenderCreated(TestPrerenderContents* contents) {
  DCHECK(!contents_);
  contents_ = contents;
  contents_->AddObserver(this);
  create_loop_.Quit();
}

void TestPrerender::OnPrerenderStart(PrerenderContents* contents) {
  started_ = true;
  start_loop_.Quit();
}

void TestPrerender::OnPrerenderStopLoading(PrerenderContents* contents) {
  number_of_loads_++;
  if (load_waiter_ && number_of_loads_ >= expected_number_of_loads_)
    load_waiter_->Quit();
}

void TestPrerender::OnPrerenderStop(PrerenderContents* contents) {
  DCHECK(contents_);
  contents_ = nullptr;
  final_status_ = contents->final_status();
  stopped_ = true;
  stop_loop_.Quit();
  // If there is a WaitForLoads call and it has yet to see the expected number
  // of loads, stop the loop so the test fails instead of timing out.
  if (load_waiter_)
    load_waiter_->Quit();
}

// static
FirstContentfulPaintManagerWaiter* FirstContentfulPaintManagerWaiter::Create(
    PrerenderManager* manager) {
  auto fcp_waiter = base::WrapUnique(new FirstContentfulPaintManagerWaiter());
  auto* fcp_waiter_ptr = fcp_waiter.get();
  manager->AddObserver(std::move(fcp_waiter));
  return fcp_waiter_ptr;
}

FirstContentfulPaintManagerWaiter::FirstContentfulPaintManagerWaiter()
    : saw_fcp_(false) {}

FirstContentfulPaintManagerWaiter::~FirstContentfulPaintManagerWaiter() {}

void FirstContentfulPaintManagerWaiter::OnFirstContentfulPaint() {
  saw_fcp_ = true;
  if (waiter_)
    waiter_->Quit();
}

void FirstContentfulPaintManagerWaiter::Wait() {
  if (saw_fcp_)
    return;
  waiter_ = std::make_unique<base::RunLoop>();
  waiter_->Run();
  waiter_.reset();
}

TestPrerenderContentsFactory::TestPrerenderContentsFactory() {}

TestPrerenderContentsFactory::~TestPrerenderContentsFactory() {
  EXPECT_TRUE(expected_contents_queue_.empty());
}

std::unique_ptr<TestPrerender>
TestPrerenderContentsFactory::ExpectPrerenderContents(
    FinalStatus final_status) {
  std::unique_ptr<TestPrerender> handle(new TestPrerender());
  expected_contents_queue_.push_back(
      ExpectedContents(final_status, handle->AsWeakPtr()));
  return handle;
}

void TestPrerenderContentsFactory::IgnorePrerenderContents() {
  expected_contents_queue_.push_back(ExpectedContents(true));
}

PrerenderContents* TestPrerenderContentsFactory::CreatePrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const content::Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    Origin origin) {
  ExpectedContents expected;
  if (!expected_contents_queue_.empty()) {
    expected = expected_contents_queue_.front();
    expected_contents_queue_.pop_front();
  }
  TestPrerenderContents* contents = new TestPrerenderContents(
      prerender_manager, profile, url, referrer, initiator_origin, origin,
      expected.final_status, expected.ignore);
  if (expected.handle)
    expected.handle->OnPrerenderCreated(contents);
  return contents;
}

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents() {}

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents(
    const ExpectedContents& other) = default;

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents(
    FinalStatus final_status,
    const base::WeakPtr<TestPrerender>& handle)
    : final_status(final_status), handle(handle) {}

TestPrerenderContentsFactory::ExpectedContents::ExpectedContents(bool ignore)
    : ignore(ignore) {}

TestPrerenderContentsFactory::ExpectedContents::~ExpectedContents() {}

PrerenderInProcessBrowserTest::PrerenderInProcessBrowserTest()
    : external_protocol_handler_delegate_(
          std::make_unique<NeverRunsExternalProtocolHandlerDelegate>()),
      safe_browsing_factory_(
          std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>()),
      prerender_contents_factory_(nullptr),
      explicitly_set_browser_(nullptr),
      autostart_test_server_(true) {}

PrerenderInProcessBrowserTest::~PrerenderInProcessBrowserTest() {}

void PrerenderInProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));
}

void PrerenderInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
}

content::SessionStorageNamespace*
PrerenderInProcessBrowserTest::GetSessionStorageNamespace() const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return nullptr;
  return web_contents->GetController().GetDefaultSessionStorageNamespace();
}

std::string PrerenderInProcessBrowserTest::MakeAbsolute(
    const std::string& path) {
  CHECK(!path.empty());
  if (path.front() == '/') {
    return path;
  }
  return "/" + path;
}

bool PrerenderInProcessBrowserTest::UrlIsInPrerenderManager(
    const std::string& html_file) const {
  return UrlIsInPrerenderManager(embedded_test_server()->GetURL(html_file));
}

bool PrerenderInProcessBrowserTest::UrlIsInPrerenderManager(
    const GURL& url) const {
  return GetPrerenderManager()->FindPrerenderData(
             url, GetSessionStorageNamespace()) != nullptr;
}

content::WebContents* PrerenderInProcessBrowserTest::GetActiveWebContents()
    const {
  return current_browser()->tab_strip_model()->GetActiveWebContents();
}

PrerenderManager* PrerenderInProcessBrowserTest::GetPrerenderManager() const {
  return PrerenderManagerFactory::GetForBrowserContext(
      current_browser()->profile());
}

TestPrerenderContents* PrerenderInProcessBrowserTest::GetPrerenderContentsFor(
    const GURL& url) const {
  PrerenderManager::PrerenderData* prerender_data =
      GetPrerenderManager()->FindPrerenderData(url, nullptr);
  return static_cast<TestPrerenderContents*>(
      prerender_data ? prerender_data->contents() : nullptr);
}

net::EmbeddedTestServer* PrerenderInProcessBrowserTest::src_server() {
  if (https_src_server_)
    return https_src_server_.get();
  return embedded_test_server();
}

test_utils::FakeSafeBrowsingDatabaseManager*
PrerenderInProcessBrowserTest::GetFakeSafeBrowsingDatabaseManager() {
  return static_cast<test_utils::FakeSafeBrowsingDatabaseManager*>(
      safe_browsing_factory()
          ->test_safe_browsing_service()
          ->database_manager()
          .get());
}

void PrerenderInProcessBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  safe_browsing_factory_->SetTestDatabaseManager(
      new test_utils::FakeSafeBrowsingDatabaseManager());
  safe_browsing::SafeBrowsingService::RegisterFactory(
      safe_browsing_factory_.get());
}

void PrerenderInProcessBrowserTest::SetUpOnMainThread() {
  current_browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);
  embedded_test_server()->RegisterRequestMonitor(
      base::Bind(&PrerenderInProcessBrowserTest::MonitorResourceRequest,
                 base::Unretained(this)));
  if (autostart_test_server_)
    CHECK(embedded_test_server()->Start());
  ExternalProtocolHandler::SetDelegateForTesting(
      external_protocol_handler_delegate_.get());

  // Check that PrerenderManager exists, which is necessary to make sure
  // NoStatePrefetch can be enabled and perceived FCP metrics can be recorded.
  PrerenderManager* prerender_manager = GetPrerenderManager();
  // Use CHECK to fail fast. The ASSERT_* macros in this context are not useful
  // because they only silently exit and make the tests crash later with more
  // complicated symptoms.
  CHECK(prerender_manager);

  // Increase the memory allowed in a prerendered page above normal settings.
  // Debug build bots occasionally run against the default limit, and tests
  // were failing because the prerender was canceled due to memory exhaustion.
  // http://crbug.com/93076
  prerender_manager->mutable_config().max_bytes = 2000 * 1024 * 1024;

  prerender_manager->mutable_config().rate_limit_enabled = false;
  CHECK(!prerender_contents_factory_);
  prerender_contents_factory_ = new TestPrerenderContentsFactory;
  prerender_manager->SetPrerenderContentsFactoryForTest(
      prerender_contents_factory_);
  CHECK(safe_browsing_factory_->test_safe_browsing_service());
}

void PrerenderInProcessBrowserTest::UseHttpsSrcServer() {
  if (https_src_server_)
    return;
  https_src_server_.reset(
      new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
  https_src_server_->ServeFilesFromSourceDirectory("chrome/test/data");
  https_src_server_->RegisterRequestMonitor(
      base::Bind(&PrerenderInProcessBrowserTest::MonitorResourceRequest,
                 base::Unretained(this)));
  CHECK(https_src_server_->Start());
}

base::string16 PrerenderInProcessBrowserTest::MatchTaskManagerTab(
    const char* page_title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::ASCIIToUTF16(page_title));
}

base::string16 PrerenderInProcessBrowserTest::MatchTaskManagerPrerender(
    const char* page_title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX,
                                    base::ASCIIToUTF16(page_title));
}

GURL PrerenderInProcessBrowserTest::GetURLWithReplacement(
    const std::string& url_file,
    const std::string& replacement_variable,
    const std::string& replacement_text) {
  base::StringPairs replacement_pair;
  replacement_pair.push_back(make_pair(replacement_variable, replacement_text));
  return src_server()->GetURL(
      MakeAbsolute(net::test_server::GetFilePathWithReplacements(
          url_file, replacement_pair)));
}

std::vector<std::unique_ptr<TestPrerender>>
PrerenderInProcessBrowserTest::NavigateWithPrerenders(
    const GURL& loader_url,
    const std::vector<FinalStatus>& expected_final_status_queue) {
  CHECK(!expected_final_status_queue.empty());
  std::vector<std::unique_ptr<TestPrerender>> prerenders;
  for (size_t i = 0; i < expected_final_status_queue.size(); i++) {
    prerenders.push_back(prerender_contents_factory()
        ->ExpectPrerenderContents(expected_final_status_queue[i]));
  }

  // Navigate to the loader URL and then wait for the first prerender to be
  // created.
  ui_test_utils::NavigateToURL(current_browser(), loader_url);
  prerenders[0]->WaitForCreate();

  return prerenders;
}

GURL PrerenderInProcessBrowserTest::ServeLoaderURL(
    const std::string& loader_path,
    const std::string& replacement_variable,
    const GURL& url_to_prerender,
    const std::string& loader_query) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair(replacement_variable, url_to_prerender.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      loader_path, replacement_text);
  return src_server()->GetURL(replacement_path + loader_query);
}

void PrerenderInProcessBrowserTest::MonitorResourceRequest(
    const net::test_server::HttpRequest& request) {
  base::AutoLock auto_lock(lock_);
  requests_[request.GetURL()]++;
  if (waiting_url_ == request.GetURL() &&
      requests_[request.GetURL()] == waiting_count_) {
    waiting_closure_.Run();
  }
}

uint32_t PrerenderInProcessBrowserTest::GetRequestCount(const GURL& url) {
  base::AutoLock auto_lock(lock_);
  auto i = requests_.find(url);
  if (i == requests_.end())
    return 0;
  return i->second;
}

void PrerenderInProcessBrowserTest::WaitForRequestCount(
    const GURL& url,
    uint32_t expected_count) {
  if (GetRequestCount(url) == expected_count)
    return;

  base::RunLoop run_loop;
  {
    base::AutoLock auto_lock(lock_);
    waiting_closure_ = run_loop.QuitClosure();
    waiting_url_ = url;
    waiting_count_ = expected_count;
  }
  run_loop.Run();
  {
    base::AutoLock auto_lock(lock_);
    waiting_url_ = GURL();
    waiting_count_ = 0;
    waiting_closure_.Reset();
  }
}

}  // namespace test_utils

}  // namespace prerender
