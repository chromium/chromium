// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_web_app_data.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/history/content/browser/download_conversions.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_state/core/security_state.h"
#include "components/services/quarantine/test_support.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_download_http_response.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/scoped_mutually_exclusive_feature_list.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "pdf/buildflags.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point_conversions.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/download/download_display.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "chrome/browser/ui/pdf/chrome_pdf_document_helper_client.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/pdf_features.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::URLLoaderInterceptor;
using content::WebContents;
using download::DownloadItem;
using download::DownloadUrlParameters;
using extensions::Extension;
using net::URLRequestMockHTTPJob;
using net::test_server::EmbeddedTestServer;

namespace {

class InnerWebContentsAttachedWaiter : public content::WebContentsObserver {
 public:
  // Observes navigation for the specified |web_contents|.
  explicit InnerWebContentsAttachedWaiter(WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void InnerWebContentsAttached(
      WebContents* inner_web_contents,
      content::RenderFrameHost* render_frame_host) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void VerifyNewDownloadId(uint32_t expected_download_id, uint32_t download_id) {
  ASSERT_EQ(expected_download_id, download_id);
}

class DownloadTestContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit DownloadTestContentBrowserClient(bool must_download)
      : must_download_(must_download) {}

  bool ShouldForceDownloadResource(content::BrowserContext* browser_context,
                                   const GURL& url,
                                   const std::string& mime_type) override {
    return must_download_;
  }

 private:
  const bool must_download_;
};

class CreatedObserver : public content::DownloadManager::Observer {
 public:
  explicit CreatedObserver(content::DownloadManager* manager)
      : manager_(manager) {
    manager->AddObserver(this);
  }

  CreatedObserver(const CreatedObserver&) = delete;
  CreatedObserver& operator=(const CreatedObserver&) = delete;

  ~CreatedObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void Wait() {
    std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
    manager_->GetAllDownloads(&downloads);
    if (!downloads.empty())
      return;
    waiting_ = true;
    run_loop_.Run();
    waiting_ = false;
  }

 private:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    DCHECK_EQ(manager_, manager);
    if (waiting_)
      run_loop_.QuitWhenIdle();
  }

  raw_ptr<content::DownloadManager> manager_;
  bool waiting_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

class OnCanDownloadDecidedObserver {
 public:
  OnCanDownloadDecidedObserver() = default;

  OnCanDownloadDecidedObserver(const OnCanDownloadDecidedObserver&) = delete;
  OnCanDownloadDecidedObserver& operator=(const OnCanDownloadDecidedObserver&) =
      delete;

  void WaitForNumberOfDecisions(size_t expected_num_of_decisions) {
    if (expected_num_of_decisions <= decisions_.size())
      return;

    expected_num_of_decisions_ = expected_num_of_decisions;
    base::RunLoop run_loop;
    completion_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnCanDownloadDecided(bool allow) {
    decisions_.push_back(allow);
    if (decisions_.size() == expected_num_of_decisions_) {
      DCHECK(!completion_closure_.is_null());
      std::move(completion_closure_).Run();
    }
  }

  const std::vector<bool>& GetDecisions() { return decisions_; }

  void Reset() {
    expected_num_of_decisions_ = 0;
    decisions_.clear();
    completion_closure_.Reset();
  }

 private:
  std::vector<bool> decisions_;
  size_t expected_num_of_decisions_ = 0;
  base::OnceClosure completion_closure_;
};

class PercentWaiter : public download::DownloadItem::Observer {
 public:
  explicit PercentWaiter(DownloadItem* item) : item_(item) {
    item_->AddObserver(this);
  }

  PercentWaiter(const PercentWaiter&) = delete;
  PercentWaiter& operator=(const PercentWaiter&) = delete;

  ~PercentWaiter() override {
    if (item_)
      item_->RemoveObserver(this);
  }

  bool WaitForFinished() {
    if (item_->GetState() == DownloadItem::COMPLETE) {
      return item_->PercentComplete() == 100;
    }
    waiting_ = true;
    run_loop_.Run();
    waiting_ = false;
    return !error_;
  }

 private:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    DCHECK_EQ(item_, item);
    if (!error_ &&
        ((prev_percent_ > item_->PercentComplete()) ||
         (item_->GetState() == DownloadItem::COMPLETE &&
          (item_->PercentComplete() != 100)))) {
      error_ = true;
      if (waiting_)
        run_loop_.QuitWhenIdle();
    }
    if (item_->GetState() == DownloadItem::COMPLETE && waiting_)
      run_loop_.QuitWhenIdle();
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    DCHECK_EQ(item_, item);
    item_->RemoveObserver(this);
    item_ = nullptr;
  }

  raw_ptr<download::DownloadItem> item_;
  bool waiting_ = false;
  bool error_ = false;
  int prev_percent_ = -1;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

// IDs and paths of CRX files used in tests.
const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoodCrxPath[] = "extensions/good.crx";

const char kLargeThemeCrxId[] = "ibcijncamhmjjdodjamgiipcgnnaeagd";
const char kLargeThemePath[] = "extensions/theme2.crx";

// User script file used in tests.
const char kUserScriptPath[] = "extensions/user_script_basic.user.js";

// Get History Information.
class DownloadsHistoryDataCollector {
 public:
  explicit DownloadsHistoryDataCollector(Profile* profile)
      : profile_(profile) {}

  DownloadsHistoryDataCollector(const DownloadsHistoryDataCollector&) = delete;
  DownloadsHistoryDataCollector& operator=(
      const DownloadsHistoryDataCollector&) = delete;

  std::vector<history::DownloadRow> WaitForDownloadInfo() {
    std::vector<history::DownloadRow> results;
    HistoryServiceFactory::GetForProfile(profile_,
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->QueryDownloads(base::BindLambdaForTesting(
            [&](std::vector<history::DownloadRow> rows) {
              results = std::move(rows);
              run_loop_.QuitWhenIdle();
            }));

    run_loop_.Run();
    return results;
  }

 private:
  raw_ptr<Profile> profile_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

bool WasAutoOpened(DownloadItem* item) {
  return item->GetAutoOpened();
}

bool IsDownloadExternallyRemoved(DownloadItem* item) {
  return item->GetFileExternallyRemoved();
}

#if !BUILDFLAG(IS_CHROMEOS)
// Called when a download starts. Marks the download as hidden.
void SetHiddenDownloadCallback(DownloadItem* item,
                               download::DownloadInterruptReason reason) {
  DownloadItemModel(item).SetShouldShowInShelf(false);
}
#endif

class SimpleDownloadManagerCoordinatorWaiter
    : public download::SimpleDownloadManagerCoordinator::Observer {
 public:
  explicit SimpleDownloadManagerCoordinatorWaiter(
      download::SimpleDownloadManagerCoordinator* coordinator)
      : coordinator_(coordinator) {
    coordinator_->AddObserver(this);
  }

  ~SimpleDownloadManagerCoordinatorWaiter() override {
    if (coordinator_)
      coordinator_->RemoveObserver(this);
  }

  void WaitForInitialization() {
    if (coordinator_ && coordinator_->initialized())
      return;
    base::RunLoop run_loop;
    completion_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Wait for a particular number of download to be created.
  void WaitForDownloadCreation(int num_download_created) {
    if (num_download_created_ >= num_download_created)
      return;
    num_download_to_wait_ = num_download_created;
    base::RunLoop run_loop;
    download_creation_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  int num_download_created() const { return num_download_created_; }

  void reset_num_download_created() { num_download_created_ = 0; }

 private:
  void OnDownloadsInitialized(bool active_downloads_only) override {
    if (completion_closure_)
      std::move(completion_closure_).Run();
  }

  void OnDownloadCreated(download::DownloadItem* item) override {
    num_download_created_++;
    if (download_creation_closure_ &&
        num_download_created_ >= num_download_to_wait_) {
      std::move(download_creation_closure_).Run();
    }
  }

  void OnManagerGoingDown(
      download::SimpleDownloadManagerCoordinator* coordinator) override {
    DCHECK_EQ(coordinator_, coordinator);
    coordinator_->RemoveObserver(this);
    coordinator_ = nullptr;
  }

  raw_ptr<download::SimpleDownloadManagerCoordinator> coordinator_;
  base::OnceClosure completion_closure_;
  base::OnceClosure download_creation_closure_;
  int num_download_created_ = 0;
  int num_download_to_wait_ = 0;
};

void CreateCompletedDownload(content::DownloadManager* download_manager,
                             const std::string& guid,
                             const base::FilePath target_path,
                             std::vector<GURL> url_chain,
                             int64_t file_size) {
  base::Time current_time = base::Time::Now();
  download_manager->CreateDownloadItem(
      guid, 1 /* id */, target_path, target_path, url_chain,
      GURL() /* referrer_url */,
      content::StoragePartitionConfig() /* storage_partition_config */,
      GURL() /* tab_url */, GURL() /* tab_referrer_url */,
      url::Origin() /* request_initiator */, "" /* mime_type */,
      "" /* original_mime_type */, current_time, current_time, "" /* etag */,
      "" /* last_modified */, file_size, file_size, "" /* hash */,
      download::DownloadItem::COMPLETE,
      download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
      download::DOWNLOAD_INTERRUPT_REASON_NONE, false /* opened */,
      current_time, false /* transient */,
      std::vector<download::DownloadItem::ReceivedSlice>());
}

#if !BUILDFLAG(IS_CHROMEOS)
// Whether download UI is visible at all (download toolbar button for download
// bubble, or download shelf).
bool IsDownloadUiVisible(BrowserWindow* window) {
  return window->GetDownloadBubbleUIController()
      ->GetDownloadDisplayController()
      ->download_display_for_testing()
      ->IsShowing();
}

// Whether download details are visible in the UI (partial view for download
// bubble, or download shelf).
bool IsDownloadDetailedUiVisible(BrowserWindow* window) {
  return window->GetDownloadBubbleUIController()
      ->GetDownloadDisplayController()
      ->download_display_for_testing()
      ->IsShowingDetails();
}
#endif

}  // namespace

class HistoryObserver : public DownloadHistory::Observer {
 public:
  explicit HistoryObserver(Profile* profile) : profile_(profile) {
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->GetDownloadHistory()
        ->AddObserver(this);
  }

  HistoryObserver(const HistoryObserver&) = delete;
  HistoryObserver& operator=(const HistoryObserver&) = delete;

  ~HistoryObserver() override {
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile_);
    if (service && service->GetDownloadHistory())
      service->GetDownloadHistory()->RemoveObserver(this);
  }

  void OnDownloadStored(download::DownloadItem* item,
                        const history::DownloadRow& info) override {
    seen_stored_ = true;
    if (waiting_)
      run_loop_.QuitWhenIdle();
  }

  void OnDownloadHistoryDestroyed() override {
    DownloadCoreServiceFactory::GetForBrowserContext(profile_)
        ->GetDownloadHistory()
        ->RemoveObserver(this);
  }

  void WaitForStored() {
    if (seen_stored_)
      return;
    waiting_ = true;
    run_loop_.Run();
    waiting_ = false;
  }

 private:
  raw_ptr<Profile> profile_;
  bool waiting_ = false;
  bool seen_stored_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

class DownloadReferrerPolicyTest
    : public DownloadTestBase,
      public ::testing::WithParamInterface<network::mojom::ReferrerPolicy> {
 public:
  DownloadReferrerPolicyTest() {
    // Link Preview hides alt+click. Disables it not to do so.
    feature_list_.InitAndDisableFeature(blink::features::kLinkPreview);
  }

  void SetUpOnMainThread() override {
    referrer_policy_ = GetParam();
    DownloadTestBase::SetUpOnMainThread();
  }

 protected:
  const network::mojom::ReferrerPolicy& referrer_policy() const {
    return referrer_policy_;
  }

 private:
  network::mojom::ReferrerPolicy referrer_policy_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DownloadReferrerPolicyTest,
    ::testing::Values(
        network::mojom::ReferrerPolicy::kAlways,
        network::mojom::ReferrerPolicy::kDefault,
        network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade,
        network::mojom::ReferrerPolicy::kNever,
        network::mojom::ReferrerPolicy::kOrigin,
        network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
        network::mojom::ReferrerPolicy::kSameOrigin,
        network::mojom::ReferrerPolicy::kStrictOrigin));

class MPArchDownloadTest : public DownloadTestBase {
 public:
  MPArchDownloadTest() = default;
  ~MPArchDownloadTest() override = default;

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class PrerenderDownloadTest : public MPArchDownloadTest {
 public:
  PrerenderDownloadTest()
      : prerender_helper_(
            base::BindRepeating(&PrerenderDownloadTest::GetWebContents,
                                base::Unretained(this))) {}
  ~PrerenderDownloadTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    MPArchDownloadTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

namespace {

class FakeDownloadProtectionService
    : public safe_browsing::DownloadProtectionService {
 public:
  FakeDownloadProtectionService()
      : safe_browsing::DownloadProtectionService(nullptr) {}

  void CheckClientDownload(
      DownloadItem* download_item,
      safe_browsing::CheckDownloadRepeatingCallback callback,
      base::optional_ref<const std::string> password) override {
    safe_browsing::ClientDownloadResponse::Verdict verdict =
        fake_verdict_.value_or(safe_browsing::ClientDownloadResponse::UNCOMMON);
    DownloadProtectionService::SetDownloadProtectionData(
        download_item, "token", verdict,
        safe_browsing::ClientDownloadResponse::TailoredVerdict());

    safe_browsing::DownloadCheckResult result =
        fake_result_.value_or(safe_browsing::DownloadCheckResult::UNCOMMON);
    std::move(callback).Run(result);
  }

  void SetFakeResponse(safe_browsing::DownloadCheckResult result,
                       safe_browsing::ClientDownloadResponse::Verdict verdict) {
    fake_result_ = result;
    fake_verdict_ = verdict;
  }

 private:
  std::optional<safe_browsing::DownloadCheckResult> fake_result_;
  std::optional<safe_browsing::ClientDownloadResponse::Verdict> fake_verdict_;
};

class FakeSafeBrowsingService : public safe_browsing::TestSafeBrowsingService {
 public:
  FakeSafeBrowsingService() : TestSafeBrowsingService() {}

  FakeSafeBrowsingService(const FakeSafeBrowsingService&) = delete;
  FakeSafeBrowsingService& operator=(const FakeSafeBrowsingService&) = delete;

 protected:
  ~FakeSafeBrowsingService() override {}

  // ServicesDelegate::ServicesCreator:
  bool CanCreateDownloadProtectionService() override { return true; }
  safe_browsing::DownloadProtectionService* CreateDownloadProtectionService()
      override {
    return new FakeDownloadProtectionService();
  }
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory
    : public safe_browsing::SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() : fake_safe_browsing_service_(nullptr) {}
  ~TestSafeBrowsingServiceFactory() override {}

  safe_browsing::SafeBrowsingServiceInterface* CreateSafeBrowsingService()
      override {
    DCHECK(!fake_safe_browsing_service_);
    fake_safe_browsing_service_ = new FakeSafeBrowsingService();
    return fake_safe_browsing_service_.get();
  }

  scoped_refptr<FakeSafeBrowsingService> fake_safe_browsing_service() {
    return fake_safe_browsing_service_;
  }

 private:
  scoped_refptr<FakeSafeBrowsingService> fake_safe_browsing_service_;
};

class DownloadTestWithFakeSafeBrowsing : public DownloadTestBase {
 public:
  DownloadTestWithFakeSafeBrowsing()
      : test_safe_browsing_factory_(new TestSafeBrowsingServiceFactory()) {}

  void SetUp() override {
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        test_safe_browsing_factory_.get());
    DownloadTestBase::SetUp();
  }

  void TearDown() override {
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
    DownloadTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestSafeBrowsingServiceFactory> test_safe_browsing_factory_;
};

class DownloadWakeLockTest : public DownloadTestBase {
 public:
  DownloadWakeLockTest() = default;

  DownloadWakeLockTest(const DownloadWakeLockTest&) = delete;
  DownloadWakeLockTest& operator=(const DownloadWakeLockTest&) = delete;

  void Initialize() {
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(device::mojom::WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

 protected:
  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;
};

}  // namespace

using DownloadTest = DownloadTestBase;

// NOTES:
//
// Files for these tests are found in DIR_TEST_DATA (currently
// "chrome\test\data\", see chrome_paths.cc).
// Mock responses have extension .mock-http-headers appended to the file name.

// Download a file due to the associated MIME type.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadMimeType) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url);

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

#if BUILDFLAG(IS_WIN)
// Download a file and confirm that the file is correctly quarantined.
//
// TODO(asanka): We should enable the test on Mac as well, but currently
// |browser_tests| aren't run from a process that has LSFileQuarantineEnabled
// bit set.
IN_PROC_BROWSER_TEST_F(DownloadTest, Quarantine_DependsOnLocalConfig) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url);

  // Check state.  Special file state must be checked before CheckDownload,
  // as CheckDownload will delete the output file.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  base::FilePath downloaded_file(DestinationFile(browser(), file));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(quarantine::IsFileQuarantined(downloaded_file, url, GURL()));
  CheckDownload(browser(), file, file);
}

// A couple of Windows specific tests to make sure we respect OS specific
// restrictions on Mark-Of-The-Web can be applied. While Chrome doesn't directly
// apply these policies, Chrome still needs to make sure the correct APIs are
// invoked during the download process that result in the expected MOTW
// behavior.

// Downloading a file from the local host shouldn't cause the application of a
// zone identifier.
IN_PROC_BROWSER_TEST_F(DownloadTest, CheckLocalhostZone_DependsOnLocalConfig) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Assumes that localhost maps to 127.0.0.1. Otherwise the test will fail
  // since EmbeddedTestServer is listening on that address.
  GURL url =
      embedded_test_server()->GetURL("localhost", "/downloads/a_zip_file.zip");
  DownloadAndWait(browser(), url);
  base::FilePath file(FILE_PATH_LITERAL("a_zip_file.zip"));
  base::FilePath downloaded_file(DestinationFile(browser(), file));
  EXPECT_FALSE(quarantine::IsFileQuarantined(downloaded_file, GURL(), GURL()));
}

// Same as the test above, but uses a file:// URL to a local file.
IN_PROC_BROWSER_TEST_F(DownloadTest, CheckLocalFileZone_DependsOnLocalConfig) {
  base::FilePath source_file = GetTestDataDirectory()
                                   .AppendASCII("downloads")
                                   .AppendASCII("a_zip_file.zip");

  GURL url = net::FilePathToFileURL(source_file);
  DownloadAndWait(browser(), url);
  base::FilePath file(FILE_PATH_LITERAL("a_zip_file.zip"));
  base::FilePath downloaded_file(DestinationFile(browser(), file));
  EXPECT_FALSE(quarantine::IsFileQuarantined(downloaded_file, GURL(), GURL()));
}
#endif

// Put up a Select File dialog when the file is downloaded, due to
// downloads preferences settings.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadMimeTypeSelect) {
  // Re-enable prompting.
  SetPromptForDownload(browser(), true);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  EnableFileChooser(true);

  // Download the file and wait.  We expect the Select File dialog to appear
  // due to the MIME type, but we still wait until the download completes.
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_TRUE(DidShowFileChooser());

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
IN_PROC_BROWSER_TEST_F(DownloadTest, NoDownload) {
  base::FilePath file(FILE_PATH_LITERAL("download-test2.html"));

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-test2.html");
  base::FilePath file_path(DestinationFile(browser(), file));

  // Open a web page and wait.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that we did not download the web page.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(VerifyNoDownloads());
}

// EmbeddedTestServer::HandleRequestCallback function that returns the relative
// URL as the MIME type.
// E.g.:
//   C -> S: GET /foo/bar =>
//   S -> C: HTTP/1.1 200 OK
//           Content-Type: foo/bar
//           ...
static std::unique_ptr<net::test_server::HttpResponse>
RespondWithContentTypeHandler(const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type(request.relative_url.substr(1));
  response->set_code(net::HTTP_OK);
  response->set_content("ooogaboogaboogabooga");
  return std::move(response);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, MimeTypesToShowNotDownload) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&RespondWithContentTypeHandler));
  ASSERT_TRUE(embedded_test_server()->Start());

  // These files should all be displayed in the browser.
  const char* mime_types[] = {
    // It is unclear whether to display text/css or download it.
    //   Firefox 3: Display
    //   Internet Explorer 7: Download
    //   Safari 3.2: Download
    // We choose to match Firefox due to the lot of complains
    // from the users if css files are downloaded:
    // http://code.google.com/p/chromium/issues/detail?id=7192
    "text/css",
    "text/javascript",
    "text/plain",
    "application/x-javascript",
    "text/html",
    "text/xml",
    "text/xsl",
    "application/xhtml+xml",
    "image/png",
    "image/gif",
    "image/jpeg",
    "image/bmp",
  };
  for (size_t i = 0; i < std::size(mime_types); ++i) {
    const char* mime_type = mime_types[i];
    GURL url(
        embedded_test_server()->GetURL(std::string("/").append(mime_type)));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Check state.
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    EXPECT_TRUE(VerifyNoDownloads());
  }
}

// Verify that when the DownloadResourceThrottle cancels a download, the
// download never makes it to the downloads system.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadResourceThrottleCancels) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to a page with the same domain as the file to download.  We can't
  // navigate directly to the file we don't want to download because cross-site
  // navigations reset the TabDownloadState.
  GURL same_site_url = embedded_test_server()->GetURL("/download_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_site_url));

  // Make sure the initial navigation didn't trigger a download.
  EXPECT_TRUE(VerifyNoDownloads());

  // Disable downloads for the tab.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          web_contents, true);
  ASSERT_TRUE(tab_download_state);
  tab_download_state->set_download_seen();
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(same_site_url),
      DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);

  // Try to start the download via Javascript and wait for the corresponding
  // load stop event.
  content::TestNavigationObserver observer(web_contents);
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "startDownload();"));
  observer.Wait();

  // Check that we did not download the file.
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  base::FilePath file_path(DestinationFile(browser(), file));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Verify that there's no pending download.  The resource throttle
  // should have deleted it before it created a download item, so it
  // shouldn't be available as a cancelled download either.
  EXPECT_TRUE(VerifyNoDownloads());
}

// Test to make sure 'download' attribute in anchor tag doesn't trigger a
// download if DownloadRequestLimiter disallows it.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       DownloadRequestLimiterDisallowsAnchorDownloadTag) {
  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/download-anchor-script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Make sure the initial navigation didn't trigger a download.
  EXPECT_TRUE(VerifyNoDownloads());

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          web_contents, true);
  ASSERT_TRUE(tab_download_state);
  // Let the first download to fail.
  tab_download_state->set_download_seen();
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(url), DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "startDownload1();"));
  can_download_observer.WaitForNumberOfDecisions(1);
  EXPECT_FALSE(can_download_observer.GetDecisions().front());
  can_download_observer.Reset();

  // Let the 2nd download to succeed.
  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser(), 1));
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(url), DownloadRequestLimiter::ALLOW_ALL_DOWNLOADS);
  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "startDownload2();"));
  can_download_observer.WaitForNumberOfDecisions(1);
  EXPECT_TRUE(can_download_observer.GetDecisions().front());

  // Waits for the 2nd download to complete.
  observer->WaitForFinished();

  // Check that only the 2nd file is downloaded.
  base::FilePath file1(FILE_PATH_LITERAL("red_dot1.png"));
  base::FilePath file_path1(DestinationFile(browser(), file1));
  base::FilePath file2(FILE_PATH_LITERAL("red_dot2.png"));
  base::FilePath file_path2(DestinationFile(browser(), file2));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path1));
  EXPECT_TRUE(base::PathExists(file_path2));
}

// Verify that non-active main frame downloads (e.g. prerendering) don't affect
// the DownloadRequestLimiter state of the WebContents.
IN_PROC_BROWSER_TEST_F(PrerenderDownloadTest,
                       DownloadRequestLimiterIsUnaffectedByPrerendering) {
  const GURL kInitialUrl =
      embedded_test_server()->GetURL("/download_script.html");
  const GURL kPrerenderingUrl = embedded_test_server()->GetURL("/empty.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Set the initial DownloadRequestLimiter state to prompt for downloads and
  // deny all requests. This allows to check whether a prerender resets the
  // state, since PROMPT_BEFORE_DOWNLOAD is reset by any navigation, while
  // DOWNLOADS_NOT_ALLOWED require a cross-site navigation to be reset and
  // those cannot be done in prerendering.
  auto* web_contents = GetWebContents();
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          web_contents, true);
  ASSERT_TRUE(tab_download_state);
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(kInitialUrl),
      DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD);
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);

  // Launch a prerendering page.
  const content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  content::test::PrerenderHostObserver host_observer(*web_contents, host_id);

  // Check that the tab download state wasn't reset by the initial prerender
  // navigation (a primary main frame navigation would have reset it as seen in
  // the test DownloadRequestLimiterTest.ResetOnNavigation).
  ASSERT_EQ(tab_download_state,
            g_browser_process->download_request_limiter()->GetDownloadState(
                web_contents, false));
  ASSERT_EQ(tab_download_state->download_status(),
            DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD);

  // Attempt a download.
  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));
  ASSERT_EQ(true, content::EvalJs(web_contents, "startDownload();"));
  can_download_observer.WaitForNumberOfDecisions(1);
  EXPECT_FALSE(can_download_observer.GetDecisions().front());

  // Check that the download didn't succeed.
  const base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  const base::FilePath file_path(DestinationFile(browser(), file));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));

  EXPECT_TRUE(VerifyNoDownloads());
}

class FencedFrameDownloadTest : public MPArchDownloadTest {
 public:
  FencedFrameDownloadTest() = default;
  ~FencedFrameDownloadTest() override = default;
  FencedFrameDownloadTest(const FencedFrameDownloadTest&) = delete;

  FencedFrameDownloadTest& operator=(const FencedFrameDownloadTest&) = delete;

  void SetUpOnMainThread() override {
    MPArchDownloadTest::SetUpOnMainThread();
    https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Verify that fenced frame downloads don't affect the DownloadRequestLimiter
// state of the WebContents.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       DownloadRequestLimiterIsUnaffectedByFencedFrame) {
  const GURL kInitialUrl =
      embedded_test_server()->GetURL("/download_script.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kInitialUrl));

  // Set the initial DownloadRequestLimiter state to prompt for downloads and
  // deny all requests. This allows to check whether a fenced frame resets the
  // state, since PROMPT_BEFORE_DOWNLOAD is reset by any navigation, while
  // DOWNLOADS_NOT_ALLOWED require a cross-site navigation to be reset and
  // those cannot be done in a fenced frame.
  auto* web_contents = GetWebContents();
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          web_contents, true);
  ASSERT_TRUE(tab_download_state);
  tab_download_state->SetDownloadStatusAndNotify(
      url::Origin::Create(kInitialUrl),
      DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD);
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);

  // Create a fenced frame and load a URL.
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  EXPECT_NE(nullptr, fenced_frame_host);

  // Check that the tab download state wasn't reset by the  navigation on the
  // fenced frame (a primary main frame navigation would have reset it as seen
  // in the test DownloadRequestLimiterTest.ResetOnNavigation).
  ASSERT_EQ(tab_download_state,
            g_browser_process->download_request_limiter()->GetDownloadState(
                web_contents, false));
  ASSERT_EQ(tab_download_state->download_status(),
            DownloadRequestLimiter::PROMPT_BEFORE_DOWNLOAD);

  // Attempt a download.
  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));
  ASSERT_EQ(true, content::EvalJs(web_contents, "startDownload();"));
  can_download_observer.WaitForNumberOfDecisions(1);
  EXPECT_FALSE(can_download_observer.GetDecisions().front());

  // Check that the download didn't succeed.
  const base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  const base::FilePath file_path(DestinationFile(browser(), file));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(file_path));

  EXPECT_TRUE(VerifyNoDownloads());
}

// Fenced frame forces download sandbox flag, which should prevent downloads
// from fenced frames.
IN_PROC_BROWSER_TEST_F(FencedFrameDownloadTest,
                       FencedFrameSandboxFlagBlockDownload) {
  ASSERT_TRUE(https_test_server()->Start());
  const GURL main_url = https_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  const GURL fenced_frame_url =
      https_test_server()->GetURL("a.test", "/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);

  content::WebContentsConsoleObserver console_observer(GetWebContents());
  console_observer.SetPattern("*Download is disallowed*");

  // Attempt a download by clicking the anchor element with download attribute
  // within the fenced frame.
  constexpr char kADownloadScript[] = R"(
      var a = document.createElement('a');
      a.setAttribute('href', 'foo.zip');
      a.download = '';
      document.body.appendChild(a);
      a.click();
    )";

  EXPECT_TRUE(ExecJs(fenced_frame_rfh, kADownloadScript));
  EXPECT_TRUE(console_observer.Wait());
  ASSERT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(console_observer.GetMessageAt(0),
            "Download is disallowed. The frame initiating or instantiating the "
            "download is sandboxed, but the flag ‘allow-downloads’ is not set. "
            "See https://www.chromestatus.com/feature/5706745674465280 for "
            "more details.");
  EXPECT_TRUE(VerifyNoDownloads());
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
IN_PROC_BROWSER_TEST_F(DownloadTest, ContentDisposition) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-test3.gif");

  base::FilePath download_file(
      FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url);

  base::FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

// UnknownSize and KnownSize are tests which depend on
// SlowDownloadHttpResponse to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish".  At that time, the download will finish.
// These tests don't currently test much due to holes in |RunSizeTest()|.  See
// comments in that routine for details.
IN_PROC_BROWSER_TEST_F(DownloadTest, UnknownSize) {
  ASSERT_TRUE(RunSizeTest(browser(), SIZE_TEST_TYPE_UNKNOWN,
                          "32.0 KB - ", "100% - "));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, KnownSize) {
  ASSERT_TRUE(RunSizeTest(browser(), SIZE_TEST_TYPE_KNOWN,
                          "71% - ", "100% - "));
}

// Test that when downloading an item in Incognito mode, we don't crash when
// closing the last Incognito window (http://crbug.com/13983).
IN_PROC_BROWSER_TEST_F(DownloadTest, IncognitoDownload) {
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito);
  int window_count = chrome::GetTotalBrowserCount();
  EXPECT_EQ(2, window_count);

  // Download a file in the Incognito window and wait.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Since |incognito| is a separate browser, we have to set it up explicitly.
  incognito->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  DownloadAndWait(incognito, url);

  // We should still have 2 windows.
  ExpectWindowCountAfterDownload(2);

  // Close the Incognito window and don't crash.
  chrome::CloseWindow(incognito);

  ui_test_utils::WaitForBrowserToClose(incognito);
  ExpectWindowCountAfterDownload(1);

  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

// Download one file on-record, then download the same file off-record, and test
// that the filename is deduplicated.  The previous test tests for a specific
// bug; this next test tests that filename deduplication happens independently
// of DownloadManager/CDMD.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_IncognitoRegular) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  // Read the origin file now so that we can compare the downloaded files to it
  // later.
  base::FilePath origin(OriginFile(base::FilePath(FILE_PATH_LITERAL(
      "downloads/a_zip_file.zip"))));
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(origin));
  int64_t origin_file_size = 0;
  EXPECT_TRUE(base::GetFileSize(origin, &origin_file_size));
  std::string original_contents;
  EXPECT_TRUE(base::ReadFileToString(origin, &original_contents));

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Download a file in the on-record browser and check that it was downloaded
  // correctly.
  DownloadAndWaitWithDisposition(browser(), url,
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_NO_WAIT);
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1UL, download_items.size());
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("a_zip_file.zip")),
            download_items[0]->GetTargetFilePath().BaseName());
  ASSERT_TRUE(base::PathExists(download_items[0]->GetTargetFilePath()));
  EXPECT_TRUE(VerifyFile(download_items[0]->GetTargetFilePath(),
                         original_contents, origin_file_size));
  uint32_t download_id = download_items[0]->GetId();
  // Verify that manager will increment the download ID when a new download is
  // requested.
  DownloadManagerForBrowser(browser())->GetNextId(
      base::BindOnce(&VerifyNewDownloadId, download_id + 1));

  // Setup an incognito window.
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito);
  int window_count = BrowserList::GetInstance()->size();
  EXPECT_EQ(2, window_count);

  download_items.clear();
  GetDownloads(incognito, &download_items);
  ASSERT_TRUE(download_items.empty());

  // Download a file in the incognito browser and check that it was downloaded
  // correctly.
  DownloadAndWaitWithDisposition(incognito, url,
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_NO_WAIT);
  GetDownloads(incognito, &download_items);
  ASSERT_EQ(1UL, download_items.size());
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("a_zip_file (1).zip")),
            download_items[0]->GetTargetFilePath().BaseName());
  ASSERT_TRUE(base::PathExists(download_items[0]->GetTargetFilePath()));
  EXPECT_TRUE(VerifyFile(download_items[0]->GetTargetFilePath(),
                         original_contents, origin_file_size));
  // The incognito download should increment the download ID again.
  ASSERT_EQ(download_id + 2, download_items[0]->GetId());
}

// Navigate to a new background page, but don't download.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab1) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-test2.html");

  // Open a web page and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // We should have two tabs now.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(VerifyNoDownloads());
}

// Download a file in a background tab. Verify that the tab is closed
// automatically.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab1) {
  // Download a file in a new background tab and wait.  The tab is automatically
  // closed when the download begins.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  DownloadAndWaitWithDisposition(browser(), url,
                                 WindowOpenDisposition::NEW_BACKGROUND_TAB, 0);

  // When the download finishes, we should still have one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file in another tab via
// a Javascript call.
// Verify that we have 2 tabs.
//
// The download_page1.html page contains an openNew() function that opens a
// tab and then downloads download-test1.lib.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab2) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download_page1.html");

  // Open a web page and wait.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Download a file in a new tab and wait (via Javascript).
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(), GURL("javascript:openNew()"),
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should have two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, open another tab via a Javascript call,
// then download a file in the new tab.
// Verify that we have 2 tabs.
//
// The download_page2.html page contains an openNew() function that opens a
// tab.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab3) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/download_page2.html");

  // Open a web page and wait.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  // Open a new tab and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("javascript:openNew()"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Download a file and wait.
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  DownloadAndWaitWithDisposition(browser(), url,
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_NO_WAIT);

  // When the download finishes, we should have two tabs.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file via Javascript,
// which will do so in a temporary tab. Verify that we have 1 tab.
//
// The download_page3.html page contains an openNew() function that opens a
// tab with download-test1.lib in the URL.  When the URL is determined to be
// a download, the tab is closed automatically.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab2) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download_page3.html");

  // Open a web page and wait.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Download a file and wait.
  // The file to download is "download-test1.lib".
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(), GURL("javascript:openNew()"),
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then call Javascript via a button to
// download a file in a new tab, which is closed automatically when the
// download begins.
// Verify that we have 1 tab.
//
// The download_page4.html page contains a form with download-test1.lib as the
// action.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab3) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download_page4.html");

  // Open a web page and wait.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Download a file in a new tab and wait.  The tab will automatically close
  // when the download begins.
  // The file to download is "download-test1.lib".
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(
      browser(), GURL("javascript:document.getElementById('form').submit()"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  CheckDownload(browser(), file, file);
}

// Open a second tab, then download a file in that tab. However, have the
// download be canceled by having the file picker act like the user canceled
// the download. The 2nd tab should be closed automatically.
// TODO(xingliu): Figure out why this is working for network service.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab4) {
  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser(), 1));
  DownloadManager* manager = DownloadManagerForBrowser(browser());
  EXPECT_EQ(0, manager->InProgressCount());
  EnableFileChooser(false);

  // Get the download URL
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL slow_download_url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kUnknownSizeUrl);

  // Open a new tab for the download
  content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::WebContents* raw_new_tab = new_tab.get();
  ASSERT_TRUE(raw_new_tab);
  ASSERT_TRUE(raw_new_tab->GetController().IsInitialNavigation());
  browser()->tab_strip_model()->AppendWebContents(std::move(new_tab), true);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Download a file in that new tab, having it open a file picker
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          raw_new_tab, slow_download_url, TRAFFIC_ANNOTATION_FOR_TESTS));
  params->set_prompt(true);
  manager->DownloadUrl(std::move(params));
  observer->WaitForFinished();

  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  ASSERT_NE(0u, items.size());
  DownloadItem* item = items[0];
  ASSERT_TRUE(item);

  // When the download is canceled, the second tab should close.
  EXPECT_EQ(item->GetState(), DownloadItem::CANCELLED);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

// EmbeddedTestServer::HandleRequestCallback function that responds with a
// redirect to the URL specified via a query string.
// E.g.:
//   C -> S: GET /redirect?http://example.com
//   S -> C: HTTP/1.1 301 Moved Permanently
//           Location: http://example.com
//           ...
static std::unique_ptr<net::test_server::HttpResponse>
ServerRedirectRequestHandler(const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/redirect",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  size_t query_position = request.relative_url.find('?');

  if (query_position == std::string::npos) {
    response->set_code(net::HTTP_PERMANENT_REDIRECT);
    response->AddCustomHeader("Location",
                              "https://request-had-no-query-string");
    response->set_content_type("text/plain");
    response->set_content("Error");
    return std::move(response);
  }

  response->set_code(net::HTTP_PERMANENT_REDIRECT);
  response->AddCustomHeader("Location",
                            request.relative_url.substr(query_position + 1));
  response->set_content_type("text/plain");
  response->set_content("It's gone!");
  return std::move(response);
}

#if BUILDFLAG(IS_WIN)
// https://crbug.com/788160
#define MAYBE_DownloadHistoryCheck DISABLED_DownloadHistoryCheck
#else
#define MAYBE_DownloadHistoryCheck DownloadHistoryCheck
#endif
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_DownloadHistoryCheck) {
  // Rediret to the actual download URL.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&ServerRedirectRequestHandler));
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL download_url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl);
  GURL redirect_url =
      embedded_test_server()->GetURL("/redirect?" + download_url.spec());

  // Inject an error.
  using TestFileErrorInjector = content::TestFileErrorInjector;
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForBrowser(browser())));
  TestFileErrorInjector::FileErrorInfo error_info = {
      TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT};
  error_info.stream_offset = 0;
  error_info.stream_bytes_written = 1024;
  injector->InjectError(error_info);

  base::FilePath file(net::GenerateFileName(download_url,
                                            std::string(),
                                            std::string(),
                                            std::string(),
                                            std::string(),
                                            std::string()));

  // Download the url and wait until the object has been stored.
  base::Time start(base::Time::Now());
  HistoryObserver observer(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url));

  // Finish the download.  We're ok relying on the history to be flushed
  // at this point as our queries will be behind the history updates
  // invoked by completion.
  std::unique_ptr<content::DownloadTestObserver> download_observer(
      new content::DownloadTestObserverInterrupted(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Finsih the download.
  GURL finish_url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kFinishSlowResponseUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), finish_url));

  download_observer->WaitForFinished();
  EXPECT_EQ(1u, download_observer->NumDownloadsSeenInState(
      DownloadItem::INTERRUPTED));
  base::Time end(base::Time::Now());

  // Get what was stored in the history.
  observer.WaitForStored();
  // Get the details on what was stored into the history.
  std::vector<history::DownloadRow> downloads_in_database =
      DownloadsHistoryDataCollector(browser()->profile()).WaitForDownloadInfo();
  ASSERT_EQ(1u, downloads_in_database.size());

  // Confirm history storage is what you expect for an interrupted slow download
  // job. The download isn't continuable, so there's no intermediate file.
  history::DownloadRow& row1(downloads_in_database[0]);
  EXPECT_EQ(DestinationFile(browser(), file), row1.target_path);
  EXPECT_TRUE(row1.current_path.empty());
  ASSERT_EQ(2u, row1.url_chain.size());
  EXPECT_EQ(redirect_url.spec(), row1.url_chain[0].spec());
  EXPECT_EQ(download_url.spec(), row1.url_chain[1].spec());
  EXPECT_EQ(history::DownloadDangerType::MAYBE_DANGEROUS_CONTENT,
            row1.danger_type);
  EXPECT_LE(start, row1.start_time);
  EXPECT_GE(end, row1.end_time);
  EXPECT_EQ(0, row1.received_bytes);  // There's no ETag. So the intermediate
                                      // state is discarded.
  EXPECT_EQ(content::SlowDownloadHttpResponse::kFirstResponsePartSize +
                content::SlowDownloadHttpResponse::kSecondResponsePartSize,
            row1.total_bytes);
  EXPECT_EQ(history::DownloadState::INTERRUPTED, row1.state);
  EXPECT_EQ(history::ToHistoryDownloadInterruptReason(
                download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT),
            row1.interrupt_reason);
  EXPECT_FALSE(row1.opened);
}

// Make sure a dangerous file shows up properly in the history.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadHistoryDangerCheck) {
  // Disable SafeBrowsing so that danger will be determined by downloads system.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);

  // .swf file so that it's dangerous on all platforms (including CrOS).
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/downloads/dangerous/dangerous.swf");

  // Download the url and wait until the object has been stored.
  auto completion_observer =
      std::make_unique<content::DownloadTestObserverTerminal>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);
  auto dangerous_observer =
      std::make_unique<content::DownloadTestObserverTerminal>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
  base::Time start(base::Time::Now());
  HistoryObserver observer(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), download_url));

  // Validate the download and wait for it to finish.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  dangerous_observer->WaitForFinished();
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  downloads[0]->ValidateDangerousDownload();
  completion_observer->WaitForFinished();
  EXPECT_EQ(1u, completion_observer->NumDangerousDownloadsSeen());

  // Get history details and confirm it's what you expect.
  observer.WaitForStored();
  std::vector<history::DownloadRow> downloads_in_database =
      DownloadsHistoryDataCollector(browser()->profile()).WaitForDownloadInfo();
  ASSERT_EQ(1u, downloads_in_database.size());
  history::DownloadRow& row1(downloads_in_database[0]);
  base::FilePath file(FILE_PATH_LITERAL("downloads/dangerous/dangerous.swf"));
  EXPECT_EQ(DestinationFile(browser(), file), row1.target_path);
  EXPECT_EQ(DestinationFile(browser(), file), row1.current_path);
  EXPECT_EQ(history::DownloadDangerType::USER_VALIDATED, row1.danger_type);
  EXPECT_LE(start, row1.start_time);
  EXPECT_EQ(history::DownloadState::COMPLETE, row1.state);
  EXPECT_FALSE(row1.opened);
  // Not checking file size--not relevant to the point of the test, and
  // the file size is actually different on Windows and other platforms,
  // because for source control simplicity it's actually a text file, and
  // there are CRLF transformations for those files.
}

// Test for crbug.com/14505. This tests that chrome:// urls are still functional
// after download of a file while viewing another chrome://.
IN_PROC_BROWSER_TEST_F(DownloadTest, ChromeURLAfterDownload) {
  GURL flags_url(chrome::kChromeUIFlagsURL);
  GURL extensions_url(chrome::kChromeUIExtensionsURL);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), flags_url));
  DownloadAndWait(browser(), download_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extensions_url));
  WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  EXPECT_EQ(true, content::EvalJs(contents,
                                  R"(
        new Promise(resolve => {
          chrome.developerPrivate.getExtensionsInfo(function(info) {
            resolve(!!info && !chrome.runtime.lastError);
          });
        });
        )"));
}

// Test for crbug.com/12745. This tests that if a download is initiated from
// a chrome:// page that has registered and onunload handler, the browser
// will be able to close.
IN_PROC_BROWSER_TEST_F(DownloadTest, BrowserCloseAfterDownload) {
  GURL downloads_url(chrome::kChromeUIFlagsURL);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), downloads_url));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  EXPECT_EQ(true, content::EvalJs(
                      contents,
                      "window.onunload = function() { var do_nothing = 0; }; "
                      "true;"));

  DownloadAndWait(browser(), download_url);

  CloseBrowserSynchronously(browser());
}

// Test to make sure the 'download' attribute in anchor tag is respected.
IN_PROC_BROWSER_TEST_F(DownloadTest, AnchorDownloadTag) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-anchor-attrib.html");

  // Create a download, wait until it's complete, and confirm
  // we're in the expected state.
  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser(), 1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Confirm the downloaded data exists.
  base::FilePath downloaded_file = GetDownloadDirectory(browser());
  downloaded_file = downloaded_file.Append(FILE_PATH_LITERAL("a_red_dot.png"));
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(downloaded_file));
}

// Test that navigating to a user script URL will result in a download.
IN_PROC_BROWSER_TEST_F(DownloadTest, UserScriptDownload) {
  DownloadTestContentBrowserClient new_client(true);
  content::ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&new_client);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/" + std::string(kUserScriptPath));

  // Navigate to the user script URL and wait for the download to complete.
  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  SetBrowserClientForTesting(old_client);
}

// Test to make sure auto-open works.
// High flake rate; https://crbug.com/1247392.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_AutoOpenByUser) {
  base::FilePath file(FILE_PATH_LITERAL("download-autoopen.txt"));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-autoopen.txt");

  ASSERT_TRUE(
      GetDownloadPrefs(browser())->EnableAutoOpenByUserBasedOnExtension(file));

  DownloadAndWait(browser(), url);

  // Find the download and confirm it was opened.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(DownloadItem::COMPLETE, downloads[0]->GetState());

  // Unfortunately, this will block forever, causing a timeout, if
  // the download is never opened.
  content::DownloadUpdatedObserver(downloads[0],
                                   base::BindRepeating(&WasAutoOpened))
      .WaitForEvent();
  EXPECT_TRUE(downloads[0]->GetOpened());  // Confirm it anyway.

  // As long as we're here, confirmed everything else is good.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  CheckDownload(browser(), file, file);
}

// Download an extension. Expect a dangerous download warning.
// Deny the download.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxDenyInstall) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/" + std::string(kGoodCrxPath));

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY));

  NavigateParams params(browser(), extension_url, ui::PAGE_TRANSITION_TYPED);
  params.user_gesture = false;
  ui_test_utils::NavigateToURL(&params);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::CANCELLED));
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());
  EXPECT_TRUE(VerifyNoDownloads());

  // Check that the CRX is not installed.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_FALSE(extension_registry->enabled_extensions().Contains(kGoodCrxId));
}

// Download an extension.  Expect a dangerous download warning.
// Allow the download, deny the install.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInstallDenysPermissions) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);
  extensions::ScopedTestDialogAutoConfirm auto_confirm_install_prompt(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/" + std::string(kGoodCrxPath));

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  NavigateParams params(browser(), extension_url, ui::PAGE_TRANSITION_TYPED);
  params.user_gesture = false;
  ui_test_utils::NavigateToURL(&params);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  content::DownloadManager::DownloadVector downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  content::DownloadUpdatedObserver(downloads[0],
                                   base::BindRepeating(&WasAutoOpened))
      .WaitForEvent();

  // Check that the extension was not installed.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_FALSE(extension_registry->enabled_extensions().Contains(kGoodCrxId));
}

// Download an extension.  Expect a dangerous download warning.
// Allow the download, and the install.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInstallAcceptPermissions) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/" + std::string(kGoodCrxPath));

  // Simulate the user allowing permission to finish the install.
  extensions::ScopedTestDialogAutoConfirm auto_confirm_install_prompt(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  NavigateParams params(browser(), extension_url, ui::PAGE_TRANSITION_TYPED);
  params.user_gesture = false;
  ui_test_utils::NavigateToURL(&params);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close from auto-open.
  content::DownloadManager::DownloadVector downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  content::DownloadUpdatedObserver(downloads[0],
                                   base::BindRepeating(&WasAutoOpened))
      .WaitForEvent();

  // Check that the extension was installed.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_TRUE(extension_registry->enabled_extensions().Contains(kGoodCrxId));
}

// Test installing a CRX that fails integrity checks.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInvalid) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/extensions/bad_signature.crx");

  // Simulate the user allowing permission to finish the install.
  extensions::ScopedTestDialogAutoConfirm auto_confirm_install_prompt(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Check that the extension was not installed.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_FALSE(extension_registry->enabled_extensions().Contains(kGoodCrxId));
}

// Install a large (100kb) theme.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxLargeTheme) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/" + std::string(kLargeThemePath));

  // Simulate the user allowing permission to finish the install.
  extensions::ScopedTestDialogAutoConfirm auto_confirm_install_prompt(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  NavigateParams params(browser(), extension_url, ui::PAGE_TRANSITION_TYPED);
  params.user_gesture = false;
  ui_test_utils::NavigateToURL(&params);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close from auto-open.
  content::DownloadManager::DownloadVector downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  content::DownloadUpdatedObserver(downloads[0],
                                   base::BindRepeating(&WasAutoOpened))
      .WaitForEvent();

  // Check that the extension was installed.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  ASSERT_TRUE(
      extension_registry->enabled_extensions().Contains(kLargeThemeCrxId));
}

// Tests for download initiation functions.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadUrl) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // DownloadUrl always prompts; return acceptance of whatever it prompts.
  EnableFileChooser(true);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::DownloadTestObserver* observer(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, TRAFFIC_ANNOTATION_FOR_TESTS));
  params->set_prompt(true);
  DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_TRUE(DidShowFileChooser());

  // Check state.
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(CheckDownload(browser(), file, file));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadUrlToPath) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  base::ScopedTempDir other_directory;
  ASSERT_TRUE(other_directory.CreateUniqueTempDir());
  base::FilePath target_file_full_path =
      other_directory.GetPath().Append(file.BaseName());
  content::DownloadTestObserver* observer(CreateWaiter(browser(), 1));
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, TRAFFIC_ANNOTATION_FOR_TESTS));
  params->set_file_path(target_file_full_path);
  DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(CheckDownloadFullPaths(browser(),
                                     target_file_full_path,
                                     OriginFile(file)));

  // Temporary are treated as auto-opened, and after that open won't be
  // visible; wait for auto-open and confirm not visible.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  content::DownloadUpdatedObserver(downloads[0],
                                   base::BindRepeating(&WasAutoOpened))
      .WaitForEvent();
}

IN_PROC_BROWSER_TEST_F(DownloadTest, TransientDownload) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  base::ScopedTempDir other_directory;
  ASSERT_TRUE(other_directory.CreateUniqueTempDir());
  base::FilePath target_file_full_path =
      other_directory.GetPath().Append(file.BaseName());
  content::DownloadTestObserver* observer(CreateWaiter(browser(), 1));
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, TRAFFIC_ANNOTATION_FOR_TESTS));
  params->set_file_path(target_file_full_path);
  params->set_transient(true);
  DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(CheckDownloadFullPaths(browser(), target_file_full_path,
                                     OriginFile(file)));

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_TRUE(downloads[0]->IsTransient());
  ASSERT_FALSE(downloads[0]->IsTemporary());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, NullInitiator) {
  GURL extensions_url("chrome-extension://fakeextension/resources");

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath target_file_full_path =
      temp_dir.GetPath().Append(file.BaseName());
  content::DownloadTestObserver* observer(CreateWaiter(browser(), 1));
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, extensions_url, TRAFFIC_ANNOTATION_FOR_TESTS));

  params->set_file_path(target_file_full_path);
  params->set_transient(true);
  DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));
  observer->WaitForFinished();
  EXPECT_EQ(0u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
}

class DownloadTestSplitCacheEnabledBase : public DownloadTest {
 public:
  DownloadTestSplitCacheEnabledBase() {
    feature_list_.InitAndEnableFeature(
        net::features::kSplitCacheByNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

enum class SplitCacheTestCase {
  kEnabledTripleKeyed,
  kEnabledTriplePlusCrossSiteMainFrameNavBool,
  kEnabledTriplePlusMainFrameNavInitiator,
  kEnabledTriplePlusNavInitiator
};

const struct {
  const SplitCacheTestCase test_case;
  base::test::FeatureRef feature;
} kTestCaseToFeatureMapping[] = {
    {SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
     net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean},
    {SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
     net::features::kSplitCacheByMainFrameNavigationInitiator},
    {SplitCacheTestCase::kEnabledTriplePlusNavInitiator,
     net::features::kSplitCacheByNavigationInitiator}};

std::string GetSplitCacheTestName(SplitCacheTestCase test_case) {
  switch (test_case) {
    case (SplitCacheTestCase::kEnabledTripleKeyed):
      return "TripleKeyed";
    case (SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool):
      return "TriplePlusCrossSiteMainFrameNavigationBool";
    case (SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator):
      return "TriplePlusMainFrameNavigationInitiator";
    case (SplitCacheTestCase::kEnabledTriplePlusNavInitiator):
      return "TriplePlusNavigationInitiator";
  }
}

class DownloadTestSplitCacheEnabled
    : public DownloadTestSplitCacheEnabledBase,
      public testing::WithParamInterface<SplitCacheTestCase> {
 public:
  DownloadTestSplitCacheEnabled()
      : split_cache_experiment_feature_list_(GetParam(),
                                             kTestCaseToFeatureMapping) {}

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DownloadTestSplitCacheEnabled,
    testing::ValuesIn(
        {SplitCacheTestCase::kEnabledTripleKeyed,
         SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
         SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
         SplitCacheTestCase::kEnabledTriplePlusNavInitiator}),
    [](const testing::TestParamInfo<SplitCacheTestCase>& info) {
      return GetSplitCacheTestName(info.param);
    });

#if BUILDFLAG(ENABLE_PDF)
class PdfDownloadTestSplitCacheEnabled
    : public DownloadTestSplitCacheEnabledBase,
      public testing::WithParamInterface<std::tuple<bool, SplitCacheTestCase>> {
 public:
  PdfDownloadTestSplitCacheEnabled()
      : split_cache_experiment_feature_list_(GetSplitCacheTestCase(),
                                             kTestCaseToFeatureMapping) {
    if (UseOopif()) {
      oopif_feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
    } else {
      oopif_feature_list_.InitAndDisableFeature(
          chrome_pdf::features::kPdfOopif);
    }
  }

  bool UseOopif() const { return std::get<0>(GetParam()); }

  SplitCacheTestCase GetSplitCacheTestCase() const {
    return std::get<1>(GetParam());
  }

  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager() {
    return factory_.GetTestPdfViewerStreamManager(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void TestSaveMainFramePdfFromTargetFrameContextMenu(
      content::RenderFrameHost* target_host,
      const GURL& url,
      std::optional<url::Origin> expected_initiator = std::nullopt) {
    auto origin =
        url::Origin::Create(https_test_server()->GetURL("a.test", "/"));
    net::SiteForCookies expected_site_for_cookies =
        net::SiteForCookies::FromOrigin(origin);

    net::IsolationInfo expected_isolation_info =
        net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                   origin, origin, expected_site_for_cookies);

    // Stop the server. This makes sure we really are pulling from the cache for
    // the download request.
    ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());

    std::optional<network::ResourceRequest::TrustedParams> trusted_params;
    net::SiteForCookies site_for_cookies;
    std::optional<url::Origin> request_initiator;

    base::RunLoop request_waiter;
    URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
        [&](URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url == url) {
            trusted_params = params->url_request.trusted_params;
            site_for_cookies = params->url_request.site_for_cookies;
            request_initiator = params->url_request.request_initiator;
            request_waiter.Quit();
          }
          return false;
        }));

    std::unique_ptr<content::DownloadTestObserver> download_waiter(
        CreateWaiter(browser(), 1));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Simulate saving the PDF from the context menu "Save As...".
    content::ContextMenuParams context_menu_params;
    context_menu_params.media_type =
        blink::mojom::ContextMenuDataMediaType::kPlugin;
    context_menu_params.src_url = url;
    context_menu_params.page_url = web_contents->GetLastCommittedURL();
    TestRenderViewContextMenu menu(*target_host, context_menu_params);
    menu.Init();
    menu.ExecuteCommand(IDC_SAVE_PAGE, 0);

    request_waiter.Run();

    ASSERT_TRUE(trusted_params.has_value());
    EXPECT_TRUE(trusted_params->isolation_info.IsEqualForTesting(
        expected_isolation_info));
    EXPECT_TRUE(site_for_cookies.IsEquivalent(expected_site_for_cookies));
    EXPECT_EQ(request_initiator, expected_initiator);

    download_waiter->WaitForFinished();

    EXPECT_EQ(1u,
              download_waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
    CheckDownloadStates(1, DownloadItem::COMPLETE);
  }

 private:
  net::test::ScopedMutuallyExclusiveFeatureList
      split_cache_experiment_feature_list_;
  base::test::ScopedFeatureList oopif_feature_list_;
  pdf::TestPdfViewerStreamManagerFactory factory_;
};

// Test that the PDF can be saved from the primary frame's context menu.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromPrimaryFrameContextMenuBrowserInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  TestSaveMainFramePdfFromTargetFrameContextMenu(
      web_contents->GetPrimaryMainFrame(), url);
}

// Same as above but using a renderer-initiated navigation from a cross-origin
// document.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromPrimaryFrameContextMenuRendererInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  GURL initial_url = https_test_server()->GetURL("b.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  TestSaveMainFramePdfFromTargetFrameContextMenu(
      web_contents->GetPrimaryMainFrame(), url,
      url::Origin::Create(initial_url));
}

// Test that the PDF can be saved from the PDf extension frame's context menu.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromExtensionFrameContextMenuBrowserInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
  ASSERT_TRUE(extension_host);

  TestSaveMainFramePdfFromTargetFrameContextMenu(extension_host, url);
}
// Same as above but using a renderer-initiated navigation from a cross-origin
// document.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromExtensionFrameContextMenuRendererInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  GURL initial_url = https_test_server()->GetURL("b.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  content::RenderFrameHost* extension_host =
      pdf_extension_test_util::GetOnlyPdfExtensionHost(web_contents);
  ASSERT_TRUE(extension_host);

  TestSaveMainFramePdfFromTargetFrameContextMenu(
      extension_host, url, url::Origin::Create(initial_url));
}

// Test that the PDF can be saved from the PDF content frame's context menu.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromContentFrameContextMenuBrowserInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  content::RenderFrameHost* content_host =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
  ASSERT_TRUE(content_host);

  TestSaveMainFramePdfFromTargetFrameContextMenu(content_host, url);
}

// Same as above but using a renderer-initiated navigation from a cross-origin
// document.
IN_PROC_BROWSER_TEST_P(
    PdfDownloadTestSplitCacheEnabled,
    SaveMainFramePdfFromContentFrameContextMenuRendererInitiatedNavigation) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  GURL initial_url = https_test_server()->GetURL("b.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a PDF page.
  GURL url = https_test_server()->GetURL("a.test", "/pdf/test.pdf");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, url));
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  content::RenderFrameHost* content_host =
      pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
  ASSERT_TRUE(content_host);

  TestSaveMainFramePdfFromTargetFrameContextMenu(
      content_host, url, url::Origin::Create(initial_url));
}

IN_PROC_BROWSER_TEST_P(PdfDownloadTestSplitCacheEnabled,
                       SaveSubframePdfFromPdfUIIsolationInfo) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  auto top_frame_origin =
      url::Origin::Create(https_test_server()->GetURL("a.test", "/"));
  net::SiteForCookies expected_site_for_cookies =
      net::SiteForCookies::FromOrigin(top_frame_origin);

  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kSubFrame, top_frame_origin,
      url::Origin::Create(https_test_server()->GetURL("b.test", "/")),
      expected_site_for_cookies);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a page with a cross-origin iframe hosting a PDF.
  GURL url = https_test_server()->GetURL("a.test", "/iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/pdf/test.pdf"));

  // Navigate the subframe and get the `RenderFrameHost` needed for
  // `pdf::PDFDocumentHelper`.
  content::RenderFrameHost* document_frame;
  if (UseOopif()) {
    content::NavigateIframeToURL(web_contents,
                                 /*iframe_id=*/"test", subframe_url);
    ASSERT_TRUE(
        GetTestPdfViewerStreamManager()->WaitUntilPdfLoadedInFirstChild());

    document_frame =
        pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
    ASSERT_TRUE(document_frame);
  } else {
    InnerWebContentsAttachedWaiter waiter(web_contents);

    content::BeginNavigateIframeToURL(web_contents,
                                      /*iframe_id=*/"test", subframe_url);
    waiter.Wait();

    std::vector<content::WebContents*> inner_web_contents_vector =
        web_contents->GetInnerWebContents();
    ASSERT_EQ(1u, inner_web_contents_vector.size());
    content::WebContents* inner_web_contents =
        inner_web_contents_vector.front();

    // Wait for the page to finish loading.
    if (inner_web_contents->IsLoading()) {
      content::TestNavigationObserver inner_navigation_waiter(
          inner_web_contents);
      inner_navigation_waiter.Wait();
      ASSERT_TRUE(!inner_web_contents->IsLoading());
    }

    document_frame = inner_web_contents->GetPrimaryMainFrame();
  }

  // Stop the server. This makes sure we really are pulling from the cache for
  // the download request.
  ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());

  std::optional<network::ResourceRequest::TrustedParams> trusted_params;
  net::SiteForCookies site_for_cookies;
  std::optional<url::Origin> request_initiator;

  base::RunLoop request_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == subframe_url) {
          trusted_params = params->url_request.trusted_params;
          site_for_cookies = params->url_request.site_for_cookies;
          request_initiator = params->url_request.request_initiator;
          request_waiter.Quit();
        }
        return false;
      }));

  std::unique_ptr<content::DownloadTestObserver> download_waiter(
      CreateWaiter(browser(), 1));

  // Simulate saving the PDF from the UI.
  pdf::PDFDocumentHelper::CreateForCurrentDocument(
      document_frame, std::make_unique<ChromePDFDocumentHelperClient>());
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(document_frame);
  pdf_helper->SaveUrlAs(
      subframe_url,
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);

  request_waiter.Run();

  ASSERT_TRUE(trusted_params.has_value());
  EXPECT_TRUE(trusted_params->isolation_info.IsEqualForTesting(
      expected_isolation_info));
  EXPECT_TRUE(site_for_cookies.IsEquivalent(expected_site_for_cookies));
  ASSERT_TRUE(request_initiator.has_value());
  EXPECT_EQ(*request_initiator, top_frame_origin);

  download_waiter->WaitForFinished();
  EXPECT_EQ(1u,
            download_waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
}
#endif  // BUILDFLAG(ENABLE_PDF)

IN_PROC_BROWSER_TEST_P(DownloadTestSplitCacheEnabled,
                       SaveSubframeImageFromContextMenuIsolationInfo) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  auto top_frame_origin =
      url::Origin::Create(https_test_server()->GetURL("a.test", "/"));
  net::SiteForCookies expected_site_for_cookies =
      net::SiteForCookies::FromOrigin(top_frame_origin);

  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kSubFrame, top_frame_origin,
      url::Origin::Create(https_test_server()->GetURL("b.test", "/")),
      expected_site_for_cookies);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a page with a cross-origin iframe hosting a PDF.
  GURL url = https_test_server()->GetURL("a.test", "/iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL subframe_url(
      https_test_server()->GetURL("b.test", "/downloads/image.jpg"));
  content::NavigateIframeToURL(web_contents,
                               /*iframe_id=*/"test", subframe_url);

  // Stop the server. This makes sure we really are pulling from the cache for
  // the download request.
  ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());

  std::optional<network::ResourceRequest::TrustedParams> trusted_params;
  net::SiteForCookies site_for_cookies;
  std::optional<url::Origin> request_initiator;

  base::RunLoop request_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == subframe_url) {
          trusted_params = params->url_request.trusted_params;
          site_for_cookies = params->url_request.site_for_cookies;
          request_initiator = params->url_request.request_initiator;
          request_waiter.Quit();
        }
        return false;
      }));

  std::unique_ptr<content::DownloadTestObserver> download_waiter(
      CreateWaiter(browser(), 1));

  // Simulate saving the image from the context menu "Save As..."
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  context_menu_params.src_url = subframe_url;
  context_menu_params.page_url =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)
          ->GetLastCommittedURL();
  content::RenderFrameHost* frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(frame);
  TestRenderViewContextMenu menu(*frame, context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);

  request_waiter.Run();

  ASSERT_TRUE(trusted_params.has_value());
  EXPECT_TRUE(trusted_params->isolation_info.IsEqualForTesting(
      expected_isolation_info));
  EXPECT_TRUE(site_for_cookies.IsEquivalent(expected_site_for_cookies));
  ASSERT_TRUE(request_initiator.has_value());
  EXPECT_EQ(*request_initiator, top_frame_origin);

  download_waiter->WaitForFinished();

  EXPECT_EQ(1u,
            download_waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
}

#if BUILDFLAG(ENABLE_PDF)
IN_PROC_BROWSER_TEST_P(PdfDownloadTestSplitCacheEnabled,
                       SaveSubframePdfFromContextMenuIsolationInfo) {
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);

  auto top_frame_origin =
      url::Origin::Create(https_test_server()->GetURL("a.test", "/"));
  net::SiteForCookies expected_site_for_cookies =
      net::SiteForCookies::FromOrigin(top_frame_origin);

  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kSubFrame, top_frame_origin,
      url::Origin::Create(https_test_server()->GetURL("b.test", "/")),
      expected_site_for_cookies);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up a page with a cross-origin iframe hosting a PDF.
  GURL url = https_test_server()->GetURL("a.test", "/iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/pdf/test.pdf"));

  // Get the `RenderFrameHost` intended to handle the save. For OOPIF PDF
  // viewer, this will be the PDF content `RenderFrameHost`. For GuestView PDF
  // viewer, this will be the PDF extension `RenderFrameHost`.
  content::RenderFrameHost* target_frame;
  if (UseOopif()) {
    content::NavigateIframeToURL(web_contents,
                                 /*iframe_id=*/"test", subframe_url);
    ASSERT_TRUE(
        GetTestPdfViewerStreamManager()->WaitUntilPdfLoadedInFirstChild());

    target_frame = pdf_extension_test_util::GetOnlyPdfPluginFrame(web_contents);
    ASSERT_TRUE(target_frame);
  } else {
    InnerWebContentsAttachedWaiter waiter(web_contents);

    content::BeginNavigateIframeToURL(web_contents,
                                      /*iframe_id=*/"test", subframe_url);
    waiter.Wait();

    std::vector<content::WebContents*> inner_web_contents_vector =
        web_contents->GetInnerWebContents();
    ASSERT_EQ(1u, inner_web_contents_vector.size());
    content::WebContents* inner_web_contents =
        inner_web_contents_vector.front();

    // Wait for the page to finish loading.
    if (inner_web_contents->IsLoading()) {
      content::TestNavigationObserver inner_navigation_waiter(
          inner_web_contents);
      inner_navigation_waiter.Wait();
      ASSERT_TRUE(!inner_web_contents->IsLoading());
    }

    target_frame = inner_web_contents->GetPrimaryMainFrame();
  }

  // Stop the server. This makes sure we really are pulling from the cache for
  // the download request.
  ASSERT_TRUE(https_test_server()->ShutdownAndWaitUntilComplete());

  std::optional<network::ResourceRequest::TrustedParams> trusted_params;
  net::SiteForCookies site_for_cookies;
  std::optional<url::Origin> request_initiator;

  base::RunLoop request_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == subframe_url) {
          trusted_params = params->url_request.trusted_params;
          site_for_cookies = params->url_request.site_for_cookies;
          request_initiator = params->url_request.request_initiator;
          request_waiter.Quit();
        }
        return false;
      }));

  std::unique_ptr<content::DownloadTestObserver> download_waiter(
      CreateWaiter(browser(), 1));

  // Simulate saving the PDF from the context menu "Save As..."
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kPlugin;
  const GURL kExtensionUrl(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html");
  context_menu_params.src_url = kExtensionUrl;
  context_menu_params.page_url = web_contents->GetLastCommittedURL();
  TestRenderViewContextMenu menu(*target_frame, context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEPLUGINAS, 0);

  request_waiter.Run();

  ASSERT_TRUE(trusted_params.has_value());
  EXPECT_TRUE(trusted_params->isolation_info.IsEqualForTesting(
      expected_isolation_info));
  EXPECT_TRUE(site_for_cookies.IsEquivalent(expected_site_for_cookies));
  ASSERT_TRUE(request_initiator.has_value());
  EXPECT_EQ(*request_initiator, top_frame_origin);

  download_waiter->WaitForFinished();

  EXPECT_EQ(1u,
            download_waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_TEST_SUITE_P(
    All,
    PdfDownloadTestSplitCacheEnabled,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(
            {SplitCacheTestCase::kEnabledTripleKeyed,
             SplitCacheTestCase::kEnabledTriplePlusCrossSiteMainFrameNavBool,
             SplitCacheTestCase::kEnabledTriplePlusMainFrameNavInitiator,
             SplitCacheTestCase::kEnabledTriplePlusNavInitiator})),
    [](const testing::TestParamInfo<std::tuple<bool, SplitCacheTestCase>>&
           info) {
      std::string test_prefix =
          std::get<0>(info.param) ? "PdfOopifEnabled" : "PdfOopifDisabled";
      return base::StrCat(
          {test_prefix, "_", GetSplitCacheTestName(std::get<1>(info.param))});
    });
#endif  // BUILDFLAG(ENABLE_PDF)

class DownloadTestWithHistogramTester : public DownloadTest {
 public:
  void SetUp() override {
    // Drop the request for https://accounts.google.com/ListAccounts.... Whether
    // this request exist can be platform-specific, so drop it for consistency
    // in a histogram recording result.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) {
              return params->url_request.url.spec().find(
                         "accounts.google.com") != std::string::npos;
            }));
    DownloadTest::SetUp();
  }

  void ResetURLLoaderInterceptor() { url_loader_interceptor_.reset(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_F(DownloadTestWithHistogramTester,
                       DISABLED_SavePageNonHTMLViaGet) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a non-HTML resource. The resource also has
  // Cache-Control: no-cache set, which normally requires revalidation
  // each time.
  GURL url = embedded_test_server()->GetURL("/downloads/image.jpg");
  ASSERT_TRUE(url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Stop the test server, and then try to save the page. If cache validation
  // is not bypassed then this will fail since the server is no longer
  // reachable.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  chrome::SavePage(browser());
  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(url, download_items[0]->GetOriginalUrl());

  // Try to download it via a context menu.
  std::unique_ptr<content::DownloadTestObserver> waiter_context_menu(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  context_menu_params.src_url = url;
  context_menu_params.page_url = url;
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(2, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(2u, download_items.size());
  ASSERT_EQ(url, download_items[0]->GetOriginalUrl());
  ASSERT_EQ(url, download_items[1]->GetOriginalUrl());

  ResetURLLoaderInterceptor();
}

// Times out often on debug ChromeOS because test is slow.
#if BUILDFLAG(IS_CHROMEOS_ASH) && \
    (!defined(NDEBUG) || defined(MEMORY_SANITIZER))
#define MAYBE_SaveLargeImage DISABLED_SaveLargeImage
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Flaking on Windows, macOS, Linux, ChromeOS. https://crbug.com/1141263
#define MAYBE_SaveLargeImage DISABLED_SaveLargeImage
#else
#define MAYBE_SaveLargeImage SaveLargeImage
#endif
// Tests saving an image from a data URL that's bigger than url::kMaxURLChars.
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_SaveLargeImage) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath data_file = ui_test_utils::GetTestFilePath(
      base::FilePath().AppendASCII("downloads"),
      base::FilePath().AppendASCII("large_image.png"));
  std::string png_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::ReadFileToString(data_file, &png_data));
  }

  std::string data_url = base::Base64Encode(png_data);
  data_url.insert(0, "data:image/png;base64,");

  ASSERT_GE(data_url.size(), url::kMaxURLChars);

  // Try to download a large image via a context menu.
  std::unique_ptr<content::DownloadTestObserver> waiter_context_menu(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  context_menu_params.src_url = GURL(data_url);
  context_menu_params.page_url = url;
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(1u, download_items.size());

  std::string downloaded_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::ReadFileToString(download_items[0]->GetFullPath(),
                                 &downloaded_data));
  }
  ASSERT_EQ(downloaded_data, png_data);
}

// A EmbeddedTestServer::HandleRequestCallback function that checks for requests
// with query string ?allow-post-only, and returns a 404 response if the method
// is not POST. Similar for ?allow-get-only.
static std::unique_ptr<net::test_server::HttpResponse>
FilterMethodSpecificURLsHandler(const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if ((request.relative_url.find("?allow-post-only") != std::string::npos &&
       request.method != net::test_server::METHOD_POST) ||
      (request.relative_url.find("?allow-get-only") != std::string::npos &&
       request.method != net::test_server::METHOD_GET)) {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_NOT_FOUND);
  }
  return std::move(response);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, SavePageNonHTMLViaPost) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&FilterMethodSpecificURLsHandler));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a form page.
  GURL form_url =
      embedded_test_server()->GetURL("/downloads/form_page_to_post.html");
  ASSERT_TRUE(form_url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_url));

  // Submit the form. This will send a POST reqeuest, and the response is a
  // JPEG image. The resource also has Cache-Control: no-cache set,
  // which normally requires revalidation each time.
  GURL jpeg_url =
      embedded_test_server()->GetURL("/downloads/image.jpg?allow-post-only");
  ASSERT_TRUE(jpeg_url.is_valid());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecJs(render_frame_host, "SubmitForm()"));
  navigation_observer.Wait();
  EXPECT_EQ(jpeg_url, web_contents->GetURL());

  // Stop the test server, and then try to save the page. If cache validation
  // is not bypassed then this will fail since the server is no longer
  // reachable. This will also fail if it tries to be retrieved via "GET"
  // rather than "POST".
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  chrome::SavePage(browser());
  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(jpeg_url, download_items[0]->GetOriginalUrl());

  // Try to download it via a context menu.
  std::unique_ptr<content::DownloadTestObserver> waiter_context_menu(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  context_menu_params.src_url = jpeg_url;
  context_menu_params.page_url = jpeg_url;
  context_menu_params.is_image_media_plugin_document = true;
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(2, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(2u, download_items.size());
  ASSERT_EQ(jpeg_url, download_items[0]->GetOriginalUrl());
  ASSERT_EQ(jpeg_url, download_items[1]->GetOriginalUrl());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, SaveImageInPostPage) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&FilterMethodSpecificURLsHandler));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a form page.
  GURL form_url =
      embedded_test_server()->GetURL("/downloads/page_with_image.html");
  GURL jpeg_url =
      embedded_test_server()->GetURL("/downloads/image.jpg?allow-get-only");
  ASSERT_TRUE(form_url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_url));

  // Submit the form.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecJs(web_contents, "document.forms[0].submit()"));
  navigation_observer.Wait();
  EXPECT_EQ(form_url, web_contents->GetURL());

  // Try to download the image via a context menu.
  // The context menu is actually opened to check that it computes the right
  // params, since the renderer is responsible for part of this check.
  content::DownloadTestObserverTerminal waiter(
      DownloadManagerForBrowser(browser()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  ContextMenuWaiter context_menu_waiter(IDC_CONTENT_CONTEXT_SAVEIMAGEAS);
  gfx::Point right_click_point = gfx::ToFlooredPoint(
      content::GetCenterCoordinatesOfElementWithId(web_contents, "image"));
  content::SimulateMouseClickAt(
      web_contents, 0, blink::WebMouseEvent::Button::kRight, right_click_point);
  context_menu_waiter.WaitForMenuOpenAndClose();
  waiter.WaitForFinished();
  EXPECT_EQ(1u, waiter.NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(jpeg_url, download_items[0]->GetOriginalUrl());
}

// TODO(crbug.com/40840482): Flaky on lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_DownloadErrorsServer DISABLED_DownloadErrorsServer
#else
#define MAYBE_DownloadErrorsServer DownloadErrorsServer
#endif
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_DownloadErrorsServer) {
  DownloadInfo download_info[] = {
      {// Normal navigated download.
       "a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
       download::DOWNLOAD_INTERRUPT_REASON_NONE, true, false},
      {// Normal direct download.
       "a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
       download::DOWNLOAD_INTERRUPT_REASON_NONE, true, false},
      {// Direct download with 404 error.
       "there_IS_no_spoon.zip", "there_IS_no_spoon.zip", DOWNLOAD_DIRECT,
       download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, true, false},
      {// Navigated download with 404 error.
       "there_IS_no_spoon.zip", "there_IS_no_spoon.zip", DOWNLOAD_NAVIGATE,
       download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, false, false},
      {// Direct download with 400 error.
       "zip_file_not_found.zip", "zip_file_not_found.zip", DOWNLOAD_DIRECT,
       download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, true, false},
      {// Navigated download with 400 error.
       "zip_file_not_found.zip", "", DOWNLOAD_NAVIGATE,
       download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, false, false},
      {// Simulates clicking on <a href="http://..." download="">. The name does
       // not resolve. But since this is an explicit download, the download
       // should appear on the shelf and the error should be indicated.
       "download-anchor-attrib-name-not-resolved.html",
       "http://doesnotexist/shouldnotberesolved", DOWNLOAD_NAVIGATE,
       download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false, false},
      {// Similar to the above, but the resulting response contains a status
       // code of 400.
       "download-anchor-attrib-400.html", "zip_file_not_found.zip",
       DOWNLOAD_NAVIGATE, download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
       true, false},
      {// Direct download of a URL where the hostname doesn't resolve.
       "http://doesnotexist/shouldnotdownloadsuccessfully",
       "http://doesnotexist/shouldnotdownloadsuccessfully", DOWNLOAD_DIRECT,
       download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, true, false}};

  DownloadFilesCheckErrors(std::size(download_info), download_info);
}

// TODO(crbug.com/40197726): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadErrorsServerNavigate404) {
  DownloadInfo download_info[] = {
      {// Simulates clicking on <a href="http://..." download=""> where the URL
       // leads to a 404 response. This is different from the previous test case
       // in that the ResourceLoader issues a OnResponseStarted() callback since
       // the headers are successfully received.
       "download-anchor-attrib-404.html", "there_IS_no_spoon.zip",
       DOWNLOAD_NAVIGATE,
       download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, true, false}};

  DownloadFilesCheckErrors(std::size(download_info), download_info);
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/739766
#define MAYBE_DownloadErrorsFile DISABLED_DownloadErrorsFile
#else
#define MAYBE_DownloadErrorsFile DownloadErrorsFile
#endif

IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_DownloadErrorsFile) {
  FileErrorInjectInfo error_info[] = {
      {// Navigated download with injected "Disk full" error in Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
       }},
      {// Direct download with injected "Disk full" error in Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
       }},
      {// Navigated download with injected "Disk full" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
       }},
      {// Direct download with injected "Disk full" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
       }},
      {// Navigated download with injected "Failed" error in Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Direct download with injected "Failed" error in Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Navigated download with injected "Failed" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Direct download with injected "Failed" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Navigated download with injected "Name too long" error in
       // Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       }},
      {// Direct download with injected "Name too long" error in Initialize().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       }},
      {// Navigated download with injected "Name too long" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Direct download with injected "Name too long" error in Write().
       {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           0,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       }},
      {// Direct download with injected "Disk full" error in 2nd Write().
       {"large_image.png", "large_image.png", DOWNLOAD_DIRECT,
        download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, true, false},
       {
           content::TestFileErrorInjector::FILE_OPERATION_WRITE,
           1,
           download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
       }}};

  DownloadInsertFilesErrorCheckErrors(std::size(error_info), error_info);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadErrorReadonlyFolder) {
  DownloadInfo download_info[] = {
      {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_DIRECT,
       // This passes because we switch to the My Documents folder.
       download::DOWNLOAD_INTERRUPT_REASON_NONE, true, true},
      {"a_zip_file.zip", "a_zip_file.zip", DOWNLOAD_NAVIGATE,
       // This passes because we switch to the My Documents folder.
       download::DOWNLOAD_INTERRUPT_REASON_NONE, true, true}};

  DownloadFilesToReadonlyFolder(std::size(download_info), download_info);
}

// Test that we show a dangerous downloads warning for a dangerous file
// downloaded through a blob: URL.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadDangerousBlobData) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous =
      safe_browsing::ScopedMarkAllFilesDangerousForTesting();

  // If SafeBrowsing is enabled, certain file types (.exe, .cab,
  // .msi) will be handled by the DownloadProtectionService. However, if the URL
  // is non-standard (e.g. blob:) then those files won't be handled by the
  // DPS. We should be showing the dangerous download warning for any file
  // considered dangerous and isn't handled by the DPS.
  std::string path("downloads/download-dangerous-blob.html?filename=foo.evil");

  // Need to use http urls because the blob js doesn't work on file urls for
  // security reasons.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/" + path);

  content::DownloadTestObserver* observer(DangerousDownloadWaiter(
      browser(), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer->WaitForFinished();

  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());
}

// A EmbeddedTestServer::HandleRequestCallback function that echoes the Referrer
// header as its contents. Only responds to the relative URL /echoreferrer
// E.g.:
//    C -> S: GET /foo
//            Referer: http://example.com/foo
//    S -> C: HTTP/1.1 200 OK
//            Content-Type: text/plain
//
//            http://example.com/foo
static std::unique_ptr<net::test_server::HttpResponse>
EchoReferrerRequestHandler(const net::test_server::HttpRequest& request) {
  const std::string kReferrerHeader = "Referer";  // SIC

  if (!base::StartsWith(request.relative_url, "/echoreferrer",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/plain");
  response->AddCustomHeader("Content-Disposition", "attachment");
  auto referrer_header = request.headers.find(kReferrerHeader);
  if (referrer_header != request.headers.end())
    response->set_content(referrer_header->second);
  return std::move(response);
}

IN_PROC_BROWSER_TEST_P(DownloadReferrerPolicyTest,
                       AltClickDownloadReferrerPolicy) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&EchoReferrerRequestHandler));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a page with a referrer policy and a link on it. The link points
  // to /echoreferrer.
  GURL url = embedded_test_server()->GetURL(
      base::StringPrintf(
          "/referrer_policy/referrer-policy-start.html?policy=%s",
          content::ReferrerPolicyToString(referrer_policy()).c_str()) +
      "&redirect=" + embedded_test_server()->GetURL("/echoreferrer").spec() +
      "&link=true&target=");
  ASSERT_TRUE(url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Click on the link with the alt key pressed. This will download the link
  // target.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown, blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(embedded_test_server()->GetURL("/echoreferrer"),
            download_items[0]->GetOriginalUrl());

  // Check that the file contains the expected referrer.
  base::FilePath file(download_items[0]->GetTargetFilePath());
  GURL origin = url::Origin::Create(url).GetURL();
  switch (referrer_policy()) {
    case network::mojom::ReferrerPolicy::kAlways:
    case network::mojom::ReferrerPolicy::kDefault:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kSameOrigin:
      EXPECT_TRUE(VerifyFile(file, url.spec(), url.spec().length()));
      break;
    case network::mojom::ReferrerPolicy::kNever:
      EXPECT_TRUE(VerifyFile(file, "", 0));
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      EXPECT_TRUE(VerifyFile(file, origin.spec(), origin.spec().length()));
      break;
  }
}

// This test ensures that the Referer header is properly sanitized when
// Save Link As is chosen from the context menu from a page with all possible
// referrer policies.
IN_PROC_BROWSER_TEST_P(DownloadReferrerPolicyTest, SaveLinkAsReferrerPolicy) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&EchoReferrerRequestHandler));
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to the initial page, where Save Link As will be executed.
  GURL url = embedded_test_server()->GetURL(
      base::StringPrintf(
          "/referrer_policy/referrer-policy-start.html?policy=%s",
          content::ReferrerPolicyToString(referrer_policy()).c_str()) +
      "&redirect=" + embedded_test_server()->GetURL("/echoreferrer").spec() +
      "&link=true&target=");
  ASSERT_TRUE(url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Right-click on the link and choose Save Link As. This will download the
  // link target.
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_SAVELINKAS);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  EXPECT_EQ(1u, download_items.size());
  EXPECT_EQ(embedded_test_server()->GetURL("/echoreferrer"),
            download_items[0]->GetOriginalUrl());

  // Check that the file contains the expected referrer.
  base::FilePath file(download_items[0]->GetTargetFilePath());
  GURL origin = url::Origin::Create(url).GetURL();
  switch (referrer_policy()) {
    case network::mojom::ReferrerPolicy::kAlways:
    case network::mojom::ReferrerPolicy::kDefault:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kSameOrigin:
      EXPECT_TRUE(VerifyFile(file, url.spec(), url.spec().length()));
      break;
    case network::mojom::ReferrerPolicy::kNever:
      EXPECT_TRUE(VerifyFile(file, "", 0));
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      EXPECT_TRUE(VerifyFile(file, origin.spec(), origin.spec().length()));
      break;
  }
}

// TODO(crbug.com/40804227): Flaky on Lacros
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SaveLinkAsVsCrossOriginResourcePolicy \
  DISABLED_SaveLinkAsVsCrossOriginResourcePolicy
#else
#define MAYBE_SaveLinkAsVsCrossOriginResourcePolicy \
  SaveLinkAsVsCrossOriginResourcePolicy
#endif
// This test ensures that Cross-Origin-Resource-Policy response header doesn't
// apply to download requests initiated via Save Link As context menu (such
// requests are considered browser-initiated).  See also
// https://crbug.com/952834.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       MAYBE_SaveLinkAsVsCrossOriginResourcePolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);

  // Test's sanity check that initially there are no download items.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Read the origin file now so that we can compare the downloaded files to it
  // later.
  base::FilePath origin(OriginFile(base::FilePath(FILE_PATH_LITERAL(
      "downloads/cross-origin-resource-policy-resource.txt"))));
  int64_t origin_file_size = 0;
  std::string original_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(origin));
    EXPECT_TRUE(base::GetFileSize(origin, &origin_file_size));
    EXPECT_TRUE(base::ReadFileToString(origin, &original_contents));
  }

  // Navigate to the test page.
  GURL url = embedded_test_server()->GetURL(
      "a.test", "/downloads/cross-origin-resource-policy-test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Right-click on the link and choose Save Link As. This will download the
  // link target.
  std::unique_ptr<content::DownloadTestObserver> download_waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_SAVELINKAS);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  download_waiter->WaitForFinished();
  EXPECT_EQ(1u,
            download_waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1u, download_items.size());
  GURL expected_original_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/downloads/"
      "cross-origin-resource-policy-resource.txt");
  EXPECT_EQ(expected_original_url, download_items[0]->GetOriginalUrl());
  EXPECT_TRUE(VerifyFile(download_items[0]->GetTargetFilePath(),
                         original_contents, origin_file_size));
}

// This test ensures that the Referer header is properly sanitized when
// Save Image As is chosen from the context menu.
IN_PROC_BROWSER_TEST_P(DownloadReferrerPolicyTest,
                       DISABLED_SaveImageAsReferrerPolicy) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&EchoReferrerRequestHandler));
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to site using secure HTTPS schema, which serves as referrer URL
  // of the next request.
  EmbeddedTestServer https_server(EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL(
      base::StringPrintf(
          "/referrer_policy/referrer-policy-start.html?policy=%s",
          content::ReferrerPolicyToString(referrer_policy()).c_str()) +
      "&redirect=" + embedded_test_server()->GetURL("/echoreferrer").spec() +
      "&link=true&target="); /* HTTPS */
  ASSERT_TRUE(url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Try to download an image via a context menu from the secure HTTPS site.
  // The download request uses insecure HTTP. The referrer URL is downgraded,
  // resulting in the referrer URL being sanitized from the download request.
  GURL img_url = embedded_test_server()->GetURL("/echoreferrer"); /* HTTP */

  std::unique_ptr<content::DownloadTestObserver> waiter_context_menu(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kImage;
  context_menu_params.page_url = url;
  context_menu_params.src_url = img_url;
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS, 0);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(DidShowFileChooser());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(img_url, download_items[0]->GetOriginalUrl());
  base::FilePath file = download_items[0]->GetTargetFilePath();
  // The contents of the file is the value of the Referer header if there was
  // one. Since the URL is downgraded from HTTPS to HTTP, the referrer is
  // removed.
  GURL origin = url::Origin::Create(url).GetURL();
  switch (referrer_policy()) {
    case network::mojom::ReferrerPolicy::kAlways:
      EXPECT_TRUE(VerifyFile(file, url.spec(), url.spec().length()));
      break;
    case network::mojom::ReferrerPolicy::kDefault:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
    case network::mojom::ReferrerPolicy::kSameOrigin:
    case network::mojom::ReferrerPolicy::kNever:
      EXPECT_TRUE(VerifyFile(file, "", 0));
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      EXPECT_TRUE(VerifyFile(file, origin.spec(), origin.spec().length()));
      break;
  }
}

// This test ensures that a cross-domain download correctly sets the referrer
// according to the referrer policy.
IN_PROC_BROWSER_TEST_P(DownloadReferrerPolicyTest,
                       DownloadCrossDomainReferrerPolicy) {
  https_test_server()->RegisterRequestHandler(
      base::BindRepeating(&ServerRedirectRequestHandler));
  https_test_server()->RegisterRequestHandler(
      base::BindRepeating(&EchoReferrerRequestHandler));
  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  EnableFileChooser(true);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a page with a referrer policy and a link on it. The link points
  // to /echoreferrer.
  GURL url = https_test_server()->GetURL(
      "www.b.test",
      base::StringPrintf(
          "/downloads/download_cross_referrer_policy.html?policy=%s",
          content::ReferrerPolicyToString(referrer_policy()).c_str()));
  ASSERT_TRUE(url.is_valid());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Click on the link with the alt key pressed. This will download the link
  // target.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown, blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(https_test_server()->GetURL("www.a.test", "/echoreferrer"),
            download_items[0]->GetURL());

  // Check that the file contains the expected referrer. The referrer is
  // expected to be sent for policies kAlways, kDefault, and
  // kNoReferrerWhenDowngrade. The referrer should not be sent for policies
  // kNever, kSameOrigin, and kStrictOriginWhenCrossOrigin.
  base::FilePath file(download_items[0]->GetTargetFilePath());
  GURL origin = url::Origin::Create(url).GetURL();

  // Since the default referrer policy can change based on configuration,
  // resolve referrer_policy() into a concrete policy.
  auto policy_for_comparison = referrer_policy();
  if (policy_for_comparison == network::mojom::ReferrerPolicy::kDefault) {
    policy_for_comparison = blink::ReferrerUtils::NetToMojoReferrerPolicy(
        blink::ReferrerUtils::GetDefaultNetReferrerPolicy());
  }

  switch (policy_for_comparison) {
    case network::mojom::ReferrerPolicy::kAlways:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      EXPECT_TRUE(VerifyFile(file, url.spec(), url.spec().length()));
      break;
    case network::mojom::ReferrerPolicy::kSameOrigin:
    case network::mojom::ReferrerPolicy::kNever:
      EXPECT_TRUE(VerifyFile(file, "", 0));
      break;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      EXPECT_TRUE(VerifyFile(file, origin.spec(), origin.spec().length()));
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected policy.";
  }
}

IN_PROC_BROWSER_TEST_F(DownloadTest, TestMultipleDownloadsRequests) {
  // Create a downloads observer.
  std::unique_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 2));

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/downloads/download-a_zip_file.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);

  // Waits for the download to complete.
  downloads_observer->WaitForFinished();
  EXPECT_EQ(2u, downloads_observer->NumDownloadsSeenInState(
      DownloadItem::COMPLETE));

  browser()->tab_strip_model()->GetActiveWebContents()->Close();
}

// Test the scenario for 3 consecutive downloads, where each is triggered by
// creating an iframe with srcdoc to another iframe with src to a downloadable
// file. Only the 1st download is expected to happen.
IN_PROC_BROWSER_TEST_F(DownloadTest, MultipleDownloadsFromIframeSrcdoc) {
  std::unique_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 1u));

  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/downloads/multiple_download_from_iframe_srcdoc.html");

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);

  // Only the 1st download should succeed. The following should fail.
  can_download_observer.WaitForNumberOfDecisions(3);
  std::vector<bool> expected_decisions{true, false, false};
  EXPECT_EQ(can_download_observer.GetDecisions(), expected_decisions);

  downloads_observer->WaitForFinished();

  EXPECT_EQ(
      1u, downloads_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
}

// Test <a download> download that triggers a x-origin redirect to another
// download. The download should succeed.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       CrossOriginRedirectDownloadFromAnchorDownload) {
  std::unique_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 1u));
  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));

  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  GURL url = https_test_server()->GetURL(
      "www.b.test",
      "/downloads/multiple_a_download_x_origin_redirect_to_download.html");

  base::StringPairs port_replacement;
  port_replacement.emplace_back(
      "{{PORT}}", base::NumberToString(https_test_server()->port()));
  std::string download_url = net::test_server::GetFilePathWithReplacements(
      "redirect_x_origin_download.html", port_replacement);

  url = GURL(url.spec() + "?download_url=" + download_url + "&num_downloads=1");

  // Navigate to a page that triggers a <a download> download attempt that
  // triggers a x-origin redirect to another download.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);

  // The <a download> attempt and well as the redirected download should both
  // pass the download limiter check.
  can_download_observer.WaitForNumberOfDecisions(2);
  std::vector<bool> expected_decisions{true, true};
  EXPECT_EQ(can_download_observer.GetDecisions(), expected_decisions);

  // Wait for the redirected download resulted from the download attempt to
  // finish.
  downloads_observer->WaitForFinished();
}

// Test the scenario for 3 consecutive <a download> download attempts that all
// trigger a x-origin redirect to another download. Only the redirected download
// resulted from the 1st <a download> attempt should succeed.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       MultipleCrossOriginRedirectDownloadsFromAnchorDownload) {
  std::unique_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 1u));

  OnCanDownloadDecidedObserver can_download_observer;
  g_browser_process->download_request_limiter()
      ->SetOnCanDownloadDecidedCallbackForTesting(base::BindRepeating(
          &OnCanDownloadDecidedObserver::OnCanDownloadDecided,
          base::Unretained(&can_download_observer)));

  https_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(https_test_server()->Start());
  GURL url = https_test_server()->GetURL(
      "www.b.test",
      "/downloads/multiple_a_download_x_origin_redirect_to_download.html");

  base::StringPairs port_replacement;
  port_replacement.emplace_back(
      "{{PORT}}", base::NumberToString(https_test_server()->port()));
  std::string download_url = net::test_server::GetFilePathWithReplacements(
      "redirect_x_origin_download.html", port_replacement);

  url = GURL(url.spec() + "?download_url=" + download_url + "&num_downloads=3");

  // Navigate to a page that triggers 3 consecutive <a download> download
  // attempts that all trigger a x-origin redirect to another download.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 1);

  // The 1st <a download> attempt should pass the download limiter check,
  // and prevent subsequent 2nd/3rd download attempts from passing the check.
  // The download resulted from the x-origin redirect from the 1st download
  // attempt will still pass the check, which could happen at any point
  // before/between/after the 2nd and 3rd <a download> attempts.
  can_download_observer.WaitForNumberOfDecisions(4);
  const std::vector<bool>& decisions = can_download_observer.GetDecisions();
  EXPECT_EQ(decisions.size(), 4u);
  EXPECT_TRUE(decisions.front());
  EXPECT_EQ(1, std::count(decisions.begin() + 1, decisions.end(), true));

  // Wait for the redirected download resulted from the 1st download attempt to
  // finish.
  downloads_observer->WaitForFinished();
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_Renaming) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  content::DownloadManager* manager = DownloadManagerForBrowser(browser());
  base::FilePath origin_file(OriginFile(base::FilePath(FILE_PATH_LITERAL(
      "downloads/a_zip_file.zip"))));
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(origin_file));
  std::string origin_contents;
  ASSERT_TRUE(base::ReadFileToString(origin_file, &origin_contents));

  // Download the same url several times and expect that all downloaded files
  // after the zero-th contain a deduplication counter.
  for (int index = 0; index < 5; ++index) {
    DownloadAndWait(browser(), url);
    download::DownloadItem* item =
        manager->GetDownload(download::DownloadItem::kInvalidId + 1 + index);
    ASSERT_TRUE(item);
    ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
    base::FilePath target_path(item->GetTargetFilePath());
    EXPECT_EQ(std::string("a_zip_file") +
        (index == 0 ? std::string(".zip") :
                      base::StringPrintf(" (%d).zip", index)),
              target_path.BaseName().AsUTF8Unsafe());
    ASSERT_TRUE(base::PathExists(target_path));
    ASSERT_TRUE(VerifyFile(target_path, origin_contents,
                           origin_contents.size()));
  }
}

// Test that the entire download pipeline handles unicode correctly.
// Disabled on Windows due to flaky timeouts: crbug.com/446695
#if BUILDFLAG(IS_WIN)
#define MAYBE_DownloadTest_CrazyFilenames DISABLED_DownloadTest_CrazyFilenames
#else
#define MAYBE_DownloadTest_CrazyFilenames DownloadTest_CrazyFilenames
#endif
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_DownloadTest_CrazyFilenames) {
  static constexpr const wchar_t* kCrazyFilenames[] = {
      L"a_file_name.zip",
      L"\u89c6\u9891\u76f4\u64ad\u56fe\u7247.zip",  // chinese chars
      (L"\u0412\u043e "
       L"\u0424\u043b\u043e\u0440\u0438\u0434\u0435\u043e\u0431\u044a"
       L"\u044f\u0432\u043b\u0435\u043d\u0440\u0435\u0436\u0438\u043c \u0427"
       L"\u041f \u0438\u0437-\u0437\u0430 \u0443\u0442\u0435\u0447\u043a\u0438 "
       L"\u043d\u0435\u0444\u0442\u0438.zip"),  // russian
      L"Desocupa\xe7\xe3o est\xe1vel.zip",
      // arabic:
      (L"\u0638\u2026\u0638\u02c6\u0637\xa7\u0638\u201a\u0637\xb9 \u0638\u201e"
       L"\u0638\u201e\u0637\xb2\u0638\u0679\u0637\xa7\u0637\xb1\u0637\xa9.zip"),
      L"\u05d4\u05e2\u05d3\u05e4\u05d5\u05ea.zip",  // hebrew
      L"\u092d\u093e\u0930\u0924.zip",              // hindi
      L"d\xe9stabilis\xe9.zip",                     // french
      // korean
      L"\u97d3-\u4e2d \uc815\uc0c1, \ucc9c\uc548\ud568 \uc758\uacac.zip",
      L"jiho....tiho...miho.zip",
      L"jiho!@#$tiho$%^&-()_+=miho copy.zip",  // special chars
      L"Wohoo-to hoo+I.zip",
      L"Picture 1.zip",
      L"This is a very very long english sentence with spaces and , and +.zip",
  };

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath origin_directory =
      GetDownloadDirectory(browser()).Append(FILE_PATH_LITERAL("origin"));
  ASSERT_TRUE(base::CreateDirectory(origin_directory));

  for (size_t index = 0; index < std::size(kCrazyFilenames); ++index) {
    SCOPED_TRACE(testing::Message() << "Index " << index);
    std::string crazy8;
    const wchar_t* const crazy_w = kCrazyFilenames[index];
    ASSERT_TRUE(base::WideToUTF8(crazy_w, wcslen(crazy_w), &crazy8));
    base::FilePath file_path(origin_directory.Append(
#if BUILDFLAG(IS_WIN)
        crazy_w
#elif BUILDFLAG(IS_POSIX)
        crazy8
#endif
        ));

    // Create the file.
    EXPECT_TRUE(base::WriteFile(file_path, crazy8));
    GURL file_url(net::FilePathToFileURL(file_path));

    // Download the file and check that the filename is correct.
    DownloadAndWait(browser(), file_url);
    GetDownloads(browser(), &download_items);
    ASSERT_EQ(1UL, download_items.size());
    base::FilePath downloaded(download_items[0]->GetTargetFilePath());
    download_items[0]->Remove();
    download_items.clear();
    ASSERT_TRUE(CheckDownloadFullPaths(
        browser(),
        downloaded,
        file_path));
  }
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_Remove) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Download a file.
  DownloadAndWaitWithDisposition(browser(), url,
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui_test_utils::BROWSER_TEST_NO_WAIT);
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1UL, download_items.size());
  base::FilePath downloaded(download_items[0]->GetTargetFilePath());

  // Remove the DownloadItem but not the file, then check that the file still
  // exists.
  download_items[0]->Remove();
  download_items.clear();
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(0UL, download_items.size());
  ASSERT_TRUE(CheckDownloadFullPaths(
      browser(), downloaded, OriginFile(base::FilePath(
          FILE_PATH_LITERAL("downloads/a_zip_file.zip")))));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_PauseResumeCancel) {
  DownloadItem* download_item = CreateSlowTestDownload();
  ASSERT_TRUE(download_item);
  ASSERT_FALSE(download_item->GetTargetFilePath().empty());
  EXPECT_FALSE(download_item->IsPaused());
  EXPECT_NE(DownloadItem::CANCELLED, download_item->GetState());
  download_item->Pause();
  EXPECT_TRUE(download_item->IsPaused());
  download_item->Resume(false);
  EXPECT_FALSE(download_item->IsPaused());
  EXPECT_NE(DownloadItem::CANCELLED, download_item->GetState());
  download_item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, download_item->GetState());
}

// The Mac downloaded files quarantine feature is implemented by the
// Contents/Info.plist file in cocoa apps. browser_tests cannot test
// quarantining files on Mac because it is not a cocoa app.
// TODO(benjhayden) test the equivalents on other platforms.

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux: http://crbug.com/238459
#define MAYBE_DownloadTest_PercentComplete DISABLED_DownloadTest_PercentComplete
#elif BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
// Stack overflow on Win/ASan: http://crbug.com/367746304
#define MAYBE_DownloadTest_PercentComplete DISABLED_DownloadTest_PercentComplete
#else
#define MAYBE_DownloadTest_PercentComplete DownloadTest_PercentComplete
#endif
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_DownloadTest_PercentComplete) {
  // Write a huge file. Make sure the test harness can supply "Content-Length"
  // header to indicate the file size, or the download will not have valid
  // percentage progression.
  test_response_handler()->RegisterToTestServer(embedded_test_server());
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/large_file");

  content::TestDownloadHttpResponse::Parameters parameters;
  parameters.size = 1024 * 1024 * 32; /* 32MB file. */
  content::TestDownloadHttpResponse::StartServing(parameters, url);

  // Ensure that we have enough disk space to download the large file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t free_space =
        base::SysInfo::AmountOfFreeDiskSpace(GetDownloadDirectory(browser()));
    ASSERT_LE(parameters.size, free_space)
        << "Not enough disk space to download. Got " << free_space;
  }

  std::unique_ptr<content::DownloadTestObserver> progress_waiter(
      CreateInProgressWaiter(browser(), 1));

  // Start downloading a file, wait for it to be created.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  progress_waiter->WaitForFinished();
  EXPECT_EQ(1u, progress_waiter->NumDownloadsSeenInState(
      DownloadItem::IN_PROGRESS));
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(1u, download_items.size());

  // Wait for the download to complete, checking along the way that the
  // PercentComplete() never regresses.
  PercentWaiter waiter(download_items[0]);
  EXPECT_TRUE(waiter.WaitForFinished());
  EXPECT_EQ(DownloadItem::COMPLETE, download_items[0]->GetState());
  ASSERT_EQ(100, download_items[0]->PercentComplete());

  // Check that the file downloaded correctly.
  ASSERT_EQ(parameters.size, download_items[0]->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download_items[0]->GetTotalBytes());

  // Delete the file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::DieFileDie(download_items[0]->GetTargetFilePath(), false));
}

// A download that is interrupted due to a file error should be able to be
// resumed.
IN_PROC_BROWSER_TEST_F(DownloadTest, Resumption_NoPrompt) {
  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));
  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      CreateWaiter(browser(), 1));
  EnableFileChooser(true);

  DownloadItem* download = StartMockDownloadAndInjectError(
      error_injector.get(), download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(download);

  download->Resume(false);
  completion_observer->WaitForFinished();

  EXPECT_EQ(
      1u, completion_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  EXPECT_FALSE(DidShowFileChooser());
}

// A download that's interrupted due to a reason that indicates that the target
// path is invalid or unusable should cause a prompt to be displayed on
// resumption.
IN_PROC_BROWSER_TEST_F(DownloadTest, Resumption_WithPrompt) {
  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));
  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      CreateWaiter(browser(), 1));
  EnableFileChooser(true);

  DownloadItem* download = StartMockDownloadAndInjectError(
      error_injector.get(), download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE);
  ASSERT_TRUE(download);

  download->Resume(true);
  completion_observer->WaitForFinished();

  EXPECT_EQ(
      1u, completion_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  EXPECT_TRUE(DidShowFileChooser());
}

// The user shouldn't be prompted on a resumed download unless a prompt is
// necessary due to the interrupt reason.
IN_PROC_BROWSER_TEST_F(DownloadTest, Resumption_WithPromptAlways) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, true);
  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));
  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      CreateWaiter(browser(), 1));
  EnableFileChooser(true);

  DownloadItem* download = StartMockDownloadAndInjectError(
      error_injector.get(), download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(download);

  // Prompts the user initially because of the kPromptForDownload preference.
  EXPECT_TRUE(DidShowFileChooser());

  download->Resume(true);
  completion_observer->WaitForFinished();

  EXPECT_EQ(
      1u, completion_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  // Shouldn't prompt for resumption.
  EXPECT_FALSE(DidShowFileChooser());
}

// A download that is interrupted due to a transient error should be resumed
// automatically.
IN_PROC_BROWSER_TEST_F(DownloadTest, Resumption_Automatic) {
  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));

  DownloadItem* download = StartMockDownloadAndInjectError(
      error_injector.get(),
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);
  ASSERT_TRUE(download);

  // The number of times this the download is resumed automatically is defined
  // in DownloadItemImpl::kMaxAutoResumeAttempts. The number of DownloadFiles
  // created should be that number + 1 (for the original download request). We
  // only care that it is greater than 1.
  EXPECT_GT(1u, error_injector->TotalFileCount());

  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      CreateWaiter(browser(), 1));
  download->Resume(true);
  completion_observer->WaitForFinished();

  // Automatic resumption causes download target determination to be run
  // multiple times. Make sure we end up with the correct filename at the end.
  EXPECT_STREQ(kDownloadTest1Path,
               download->GetTargetFilePath().BaseName().AsUTF8Unsafe().c_str());
}

// An interrupting download should be resumable multiple times.
IN_PROC_BROWSER_TEST_F(DownloadTest, Resumption_MultipleAttempts) {
  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));
  std::unique_ptr<DownloadTestObserverNotInProgress> completion_observer(
      new DownloadTestObserverNotInProgress(
          DownloadManagerForBrowser(browser()), 1));
  // Wait for two transitions to a resumable state
  std::unique_ptr<content::DownloadTestObserver> resumable_observer(
      new DownloadTestObserverResumable(DownloadManagerForBrowser(browser()),
                                        2));

  EnableFileChooser(true);
  DownloadItem* download = StartMockDownloadAndInjectError(
      error_injector.get(), download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(download);

  content::TestFileErrorInjector::FileErrorInfo error_info;
  error_info.code = content::TestFileErrorInjector::FILE_OPERATION_WRITE;
  error_info.operation_instance = 0;
  error_info.error = download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  error_injector->InjectError(error_info);

  // Resuming should cause the download to be interrupted again due to the
  // errors we are injecting.
  download->Resume(false);
  resumable_observer->WaitForFinished();
  ASSERT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  ASSERT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
            download->GetLastReason());

  error_injector->ClearError();

  // No errors this time. The download should complete successfully.
  EXPECT_FALSE(completion_observer->IsFinished());
  completion_observer->StartObserving();
  download->Resume(false);
  completion_observer->WaitForFinished();
  EXPECT_EQ(DownloadItem::COMPLETE, download->GetState());

  EXPECT_FALSE(DidShowFileChooser());
}

// The file empty.bin is served with a MIME type of application/octet-stream.
// The content body is empty. Make sure this case is handled properly and we
// don't regress on http://crbug.com/320394.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_GZipWithNoContent) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/empty.bin");

  // Downloading the same URL twice causes the second request to be served from
  // cached (with a high probability). This test verifies that that doesn't
  // happen regardless of whether the request is served via the cache or from
  // the network.
  DownloadAndWait(browser(), url);
  DownloadAndWait(browser(), url);
}

// Test that the SecurityLevel of the initiating page is used for the histogram
// rather than the SecurityLevel of the download URL, and that downloads in new
// tabs are not tracked.
IN_PROC_BROWSER_TEST_F(DownloadTest, SecurityLevels) {
  base::HistogramTester histogram_tester;
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  http_server.ServeFilesFromDirectory(GetTestDataDirectory());
  https_server.ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(http_server.Start());
  ASSERT_TRUE(https_server.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           http_server.GetURL("/simple.html")));
  DownloadAndWait(browser(), https_server.GetURL("/downloads/a_zip_file.zip"));
  histogram_tester.ExpectBucketCount("Security.SecurityLevel.DownloadStarted",
                                     security_state::NONE, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/simple.html")));
  DownloadAndWait(browser(), http_server.GetURL("/downloads/a_zip_file.zip"));
  histogram_tester.ExpectBucketCount("Security.SecurityLevel.DownloadStarted",
                                     security_state::SECURE, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           http_server.GetURL("/simple.html")));
  DownloadAndWaitWithDisposition(
      browser(), https_server.GetURL("/downloads/a_zip_file.zip"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  histogram_tester.ExpectTotalCount("Security.SecurityLevel.DownloadStarted",
                                    2);
}

// Tests that opening the downloads page will cause file existence check.
IN_PROC_BROWSER_TEST_F(DownloadTest, FileExistenceCheckOpeningDownloadsPage) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url);

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* item = downloads[0];
  base::DeleteFile(item->GetTargetFilePath());
  ASSERT_FALSE(item->GetFileExternallyRemoved());

  // Open the downloads tab.
  chrome::ShowDownloads(browser());
  // Check file removal update will eventually come.
  content::DownloadUpdatedObserver(
      item, base::BindRepeating(&IsDownloadExternallyRemoved))
      .WaitForEvent();
}

// Checks that the navigation resulting from a cross origin download navigates
// the correct iframe.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrossOriginDownloadNavigatesIframe) {
  EmbeddedTestServer origin_one;
  EmbeddedTestServer origin_two;
  EmbeddedTestServer origin_three;

  origin_one.ServeFilesFromDirectory(GetTestDataDirectory());
  origin_two.ServeFilesFromDirectory(GetTestDataDirectory());
  origin_three.ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());
  ASSERT_TRUE(origin_three.InitializeAndListen());

  // We load a page on origin_one which iframes a page from origin_two which
  // downloads a file that redirects to origin_three.
  GURL download_url =
      origin_two.GetURL(std::string("/redirect?") +
                        origin_three.GetURL("/downloads/message.html").spec());
  GURL referrer_url = origin_two.GetURL(
      std::string("/downloads/download-attribute.html?target=") +
      download_url.spec());
  GURL main_url =
      origin_one.GetURL(std::string("/downloads/page-with-frame.html?url=") +
                        referrer_url.spec());

  origin_two.RegisterRequestHandler(
      base::BindRepeating(&ServerRedirectRequestHandler));

  origin_one.StartAcceptingConnections();
  origin_two.StartAcceptingConnections();
  origin_three.StartAcceptingConnections();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);

  // Clicking the <a download> in the iframe should navigate the iframe,
  // not the main frame.
  std::u16string expected_title(u"Loaded as iframe");
  std::u16string failed_title(u"Loaded as main frame");
  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(failed_title);
  render_frame_host->ExecuteJavaScriptForTests(
      u"runTest();", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Also verify that there's no download.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_three.ShutdownAndWaitUntilComplete());
}

// Test is flaky on multiple platforms.
// https://crbug.com/1064435
IN_PROC_BROWSER_TEST_F(DownloadWakeLockTest,
                       DISABLED_WakeLockAcquireAndCancel) {
  Initialize();
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  DownloadItem* download_item = CreateSlowTestDownload();
  ASSERT_TRUE(download_item);
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  download_item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, download_item->GetState());
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
}

// Downloading a data URL that's bigger than url::kMaxURLChars should work.
// Flaky: https://crbug.com/1141278
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadLargeDataURL) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());

  auto completion_observer =
      std::make_unique<content::DownloadTestObserverTerminal>(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  // Navigating to large_data_url.html will trigger a download of a data URL
  // that is larger than 2MB.
  GURL url = embedded_test_server()->GetURL("/downloads/large_data_url.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::FilePath data_file = ui_test_utils::GetTestFilePath(
      base::FilePath().AppendASCII("downloads"),
      base::FilePath().AppendASCII("large_image.png"));
  std::string png_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::ReadFileToString(data_file, &png_data));
  }

  completion_observer->WaitForFinished();
  EXPECT_EQ(
      1u, completion_observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Validate that the correct file was downloaded via the context menu.
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("large.png")),
            download_items[0]->GetFileNameToReportUser());

  std::string downloaded_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::ReadFileToString(download_items[0]->GetFullPath(),
                                 &downloaded_data));
  }
  ASSERT_EQ(downloaded_data, png_data);
}

// Testing the behavior of resuming with only in-progress download manager.
class InProgressDownloadTest : public DownloadTest {
 public:
  InProgressDownloadTest() {
    feature_list_.InitWithFeatures(
        {download::features::kUseInProgressDownloadManagerForDownloadService},
        {});

    // The in progress download manager will be released from
    // `DownloadManagerUtils` during creation of the `DownloadManagerImpl`. As
    // the `DownloadManagerImpl` may be created before test bodies can run,
    // register a callback to cache a pointer before release occurs.
    DownloadManagerUtils::
        SetRetrieveInProgressDownloadManagerCallbackForTesting(
            base::BindRepeating(
                &InProgressDownloadTest::set_in_progress_manager,
                base::Unretained(this)));
  }

  // DownloadTest:
  void SetUpOnMainThread() override {
    EXPECT_TRUE(CheckTestDir());

    if (!in_progress_manager_) {
      // This will only occur if `DownloadManagerImpl` has not already been
      // created in which case the in progress download manager has not yet been
      // released from `DownloadManagerUtils`.
      set_in_progress_manager(
          DownloadManagerUtils::GetInProgressDownloadManager(
              browser()->profile()->GetProfileKey()));
    }

    // As a pointer to the in progress download manager has now been cached,
    // watching for release from `DownloadManagerUtils` (if it has not already
    // occurred) is no longer necessary.
    DownloadManagerUtils::
        SetRetrieveInProgressDownloadManagerCallbackForTesting(
            base::NullCallback());
  }

  download::InProgressDownloadManager* in_progress_manager() {
    return in_progress_manager_;
  }

  void set_in_progress_manager(
      download::InProgressDownloadManager* in_progress_manager) {
    in_progress_manager_ = in_progress_manager;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<download::InProgressDownloadManager, DanglingUntriaged>
      in_progress_manager_ = nullptr;
};

// Check that if a download exists in both in-progress and history DB,
// resuming the download after loading the in-progress DB and before
// history initialization will continue downloading the item even if it
// is in a terminal state in history DB.
IN_PROC_BROWSER_TEST_F(InProgressDownloadTest,
                       ResumeInProgressDownloadBeforeLoadingHistory) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");
  base::FilePath origin(OriginFile(
      base::FilePath(FILE_PATH_LITERAL("downloads/a_zip_file.zip"))));
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(origin));
  // Gets the file size.
  int64_t origin_file_size = 0;
  EXPECT_TRUE(base::GetFileSize(origin, &origin_file_size));
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Wait for in-progress download manager to initialize.
  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  SimpleDownloadManagerCoordinatorWaiter coordinator_waiter(coordinator);
  coordinator_waiter.WaitForInitialization();

  base::FilePath target_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &target_path));
  target_path =
      target_path.Append(base::FilePath(FILE_PATH_LITERAL("a_zip_file.zip")));
  std::vector<GURL> url_chain;
  url_chain.emplace_back(url);
  base::Time current_time = base::Time::Now();
  in_progress_manager()->AddInProgressDownloadForTest(
      std::make_unique<download::DownloadItemImpl>(
          in_progress_manager(), guid, 1 /* id */,
          target_path.AddExtensionASCII("crdownload"), target_path, url_chain,
          GURL() /* referrer_url */,
          std::string() /* serialized_embedder_data */, GURL() /* tab_url */,
          GURL() /* tab_referrer_url */, url::Origin() /* request_initiator */,
          "" /* mime_type */, "" /* original_mime_type */, current_time,
          current_time, "" /* etag */, "" /* last_modified */,
          0 /* received_bytes */, origin_file_size, 0 /* auto_resume_count */,
          "" /* hash */, download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
          download::DOWNLOAD_INTERRUPT_REASON_CRASH, false /* paused */,
          false /* allow_metered */, false /* opened */, current_time,
          false /* transient */,
          std::vector<download::DownloadItem::ReceivedSlice>(),
          download::kInvalidRange, download::kInvalidRange,
          nullptr /* download_entry */));

  download::DownloadItem* download = coordinator->GetDownloadByGuid(guid);
  content::DownloadManager* manager = DownloadManagerForBrowser(browser());
  DownloadCoreService* service =
      DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile());
  service->SetDownloadHistoryForTesting(nullptr);

  ASSERT_TRUE(download);
  PercentWaiter waiter(download);
  // Resume the download first, before download history loads.
  download->Resume(true);
  // Now simulate that history DB is loaded.
  manager->OnHistoryQueryComplete(
      base::BindOnce(CreateCompletedDownload, base::Unretained(manager), guid,
                     target_path, std::move(url_chain), origin_file_size));
  // Download should continue and complete.
  ASSERT_TRUE(waiter.WaitForFinished());
  download::DownloadItem* history_download = manager->GetDownloadByGuid(guid);
  CHECK_EQ(download, history_download);
}

// Check that InProgressDownloadManager can handle transient downloads with the
// same GUID.
IN_PROC_BROWSER_TEST_F(InProgressDownloadTest,
                       DownloadURLWithInProgressManager) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");
  base::FilePath origin(OriginFile(
      base::FilePath(FILE_PATH_LITERAL("downloads/a_zip_file.zip"))));
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(origin));
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Wait for in-progress download manager to initialize.
  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  SimpleDownloadManagerCoordinatorWaiter coordinator_waiter(coordinator);
  coordinator_waiter.WaitForInitialization();

  base::FilePath target_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &target_path));
  target_path =
      target_path.Append(base::FilePath(FILE_PATH_LITERAL("a_zip_file.zip")));
  std::vector<GURL> url_chain;
  url_chain.emplace_back(url);
  // Kick off 2 download with the same GUID
  auto params = std::make_unique<DownloadUrlParameters>(
      url, TRAFFIC_ANNOTATION_FOR_TESTS);
  params->set_guid(guid);
  params->set_file_path(target_path);
  params->set_transient(true);
  params->set_require_safety_checks(false);
  in_progress_manager()->DownloadUrl(std::move(params));
  auto params2 = std::make_unique<DownloadUrlParameters>(
      url, TRAFFIC_ANNOTATION_FOR_TESTS);
  params2->set_guid(guid);
  params2->set_file_path(target_path);
  params2->set_transient(true);
  params2->set_require_safety_checks(false);
  in_progress_manager()->DownloadUrl(std::move(params2));
  coordinator_waiter.WaitForDownloadCreation(1);
  download::DownloadItem* download = coordinator->GetDownloadByGuid(guid);
  ASSERT_TRUE(download);

  PercentWaiter waiter(download);
  // Download should continue and complete.
  ASSERT_TRUE(waiter.WaitForFinished());

  // Only 1 download is created above, no more new downloads are created.
  ASSERT_EQ(coordinator_waiter.num_download_created(), 1);
}

// Tests that download a canvas image will show the file chooser.
IN_PROC_BROWSER_TEST_F(DownloadTest, SaveCanvasImage) {
  EnableFileChooser(true);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/downloads/page_with_canvas_image.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Try to download a canvas image via a context menu.
  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Right-click on the link and choose Save Image As. This will download the
  // canvas image.
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_SAVEIMAGEAS);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_TRUE(DidShowFileChooser());
}

// Tests that accept header is correctly set when using context menu to download
// an image.
IN_PROC_BROWSER_TEST_F(DownloadTest, ContextMenuSaveImageWithAcceptHeader) {
  EnableFileChooser(true);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/large_image.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL download_url =
      embedded_test_server()->GetURL("/downloads/large_image.png");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Try to download a canvas image via a context menu.
  std::unique_ptr<content::DownloadTestObserver> waiter(
      new content::DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  net::HttpRequestHeaders headers;
  base::RunLoop request_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == download_url) {
          headers = params->url_request.headers;
          request_waiter.Quit();
        }
        return false;
      }));

  // Right-click on the link and choose Save Image As. This will download the
  // image.
  ContextMenuNotificationObserver context_menu_observer(
      IDC_CONTENT_CONTEXT_SAVEIMAGEAS);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  waiter->WaitForFinished();
  EXPECT_EQ(headers.GetHeader(net::HttpRequestHeaders::kAccept)
                .value_or(std::string()),
            blink::network_utils::ImageAcceptHeader());
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)

namespace {

// This is a custom DownloadTestObserver for
// DangerousFileWithSBDisabledBeforeCompletion test that disables the
// SafeBrowsing service when a single download is IN_PROGRESS and has a target
// path assigned.  DownloadItemImpl is expected to call MaybeCompleteDownload
// soon afterwards and we want to disable the service before then.
class DisableSafeBrowsingOnInProgressDownload
    : public content::DownloadTestObserver {
 public:
  explicit DisableSafeBrowsingOnInProgressDownload(Browser* browser)
      : DownloadTestObserver(DownloadManagerForBrowser(browser),
                             1,
                             ON_DANGEROUS_DOWNLOAD_QUIT),
        browser_(browser),
        final_state_seen_(false) {
    Init();
  }
  ~DisableSafeBrowsingOnInProgressDownload() override {}

  bool IsDownloadInFinalState(DownloadItem* download) override {
    if (download->GetState() != DownloadItem::IN_PROGRESS ||
        download->GetTargetFilePath().empty())
      return false;

    if (final_state_seen_)
      return true;

    final_state_seen_ = true;
    browser_->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                false);
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              download->GetDangerType());
    EXPECT_FALSE(download->IsDangerous());
    EXPECT_NE(safe_browsing::DownloadFileType::NOT_DANGEROUS,
              DownloadItemModel(download).GetDangerLevel());
    return true;
  }

 private:
  raw_ptr<Browser> browser_;
  bool final_state_seen_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DownloadTest,
                       DangerousFileWithSBDisabledBeforeCompletion) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               true);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);

  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  std::unique_ptr<content::DownloadTestObserver> in_progress_observer(
      new DisableSafeBrowsingOnInProgressDownload(browser()));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), download_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  in_progress_observer->WaitForFinished();

  // SafeBrowsing should have been disabled by our observer.
  ASSERT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingEnabled));

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* download = downloads[0];

  dangerous_observer->WaitForFinished();

  EXPECT_TRUE(download->IsDangerous());
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download->GetDangerType());
  download->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DangerousFileWithSBDisabledBeforeStart) {
  // Disable SafeBrowsing
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);

  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), download_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  dangerous_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  DownloadItem* download = downloads[0];
  EXPECT_TRUE(download->IsDangerous());
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download->GetDangerType());

  download->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, SafeSupportedFile) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  DownloadAndWait(browser(), download_url);

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  DownloadItem* download = downloads[0];
  EXPECT_FALSE(download->IsDangerous());
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            download->GetDangerType());

  download->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadTestWithFakeSafeBrowsing,
                       SendUncommonDownloadReportIfUserProceed) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               true);
  // Make a dangerous file.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);

  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), download_url));
  dangerous_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* download = downloads[0];
  DownloadItemModel model(download);
  DownloadCommands(model.GetWeakPtr()).ExecuteCommand(DownloadCommands::KEEP);

  safe_browsing::ClientSafeBrowsingReportRequest actual_report;
  actual_report.ParseFromString(
      test_safe_browsing_factory_->fake_safe_browsing_service()
          ->serialized_download_report());
  EXPECT_EQ(safe_browsing::ClientSafeBrowsingReportRequest::
                DANGEROUS_DOWNLOAD_WARNING,
            actual_report.type());
  EXPECT_EQ(safe_browsing::ClientDownloadResponse::UNCOMMON,
            actual_report.download_verdict());
  EXPECT_EQ(download_url.spec(), actual_report.url());
  EXPECT_TRUE(actual_report.did_proceed());

  download->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadTestWithFakeSafeBrowsing,
                       SendDownloadReportIfUserProceedsDeepScanning) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               true);
  // Make a dangerous file.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);
  auto* download_protection_service =
      static_cast<FakeDownloadProtectionService*>(
          g_browser_process->safe_browsing_service()
              ->download_protection_service());
  download_protection_service->SetFakeResponse(
      safe_browsing::DownloadCheckResult::PROMPT_FOR_SCANNING,
      safe_browsing::ClientDownloadResponse::UNCOMMON);
  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), download_url));
  dangerous_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* download = downloads[0];
  DownloadItemModel model(download);
  DownloadCommands(model.GetWeakPtr())
      .ExecuteCommand(DownloadCommands::BYPASS_DEEP_SCANNING);

  safe_browsing::ClientSafeBrowsingReportRequest actual_report;
  actual_report.ParseFromString(
      test_safe_browsing_factory_->fake_safe_browsing_service()
          ->serialized_download_report());
  EXPECT_EQ(safe_browsing::ClientSafeBrowsingReportRequest::
                DANGEROUS_DOWNLOAD_WARNING,
            actual_report.type());
  EXPECT_EQ(safe_browsing::ClientDownloadResponse::UNCOMMON,
            actual_report.download_verdict());
  EXPECT_EQ(download_url.spec(), actual_report.url());
  EXPECT_TRUE(actual_report.did_proceed());

  // Trying to quit when the download hasn't completed will show a "Continue
  // downloading?" prompt, and the test will timeout trying to quit. Instead
  // wait for the download to complete before quitting.
  std::unique_ptr<content::DownloadTestObserver> completed_observer(
      CreateWaiter(browser(), 1));
  completed_observer->WaitForFinished();
}

IN_PROC_BROWSER_TEST_F(DownloadTestWithFakeSafeBrowsing,
                       SendUncommonDownloadReportIfUserDiscard) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               true);
  // Make a dangerous file.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL(DownloadTestBase::kDangerousMockFilePath);
  std::unique_ptr<content::DownloadTestObserver> dangerous_observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), download_url));
  dangerous_observer->WaitForFinished();

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  DownloadItem* download = downloads[0];
  DownloadItemModel model(download);
  DownloadCommands(model.GetWeakPtr())
      .ExecuteCommand(DownloadCommands::DISCARD);

  safe_browsing::ClientSafeBrowsingReportRequest actual_report;
  actual_report.ParseFromString(
      test_safe_browsing_factory_->fake_safe_browsing_service()
          ->serialized_download_report());
  EXPECT_EQ(safe_browsing::ClientSafeBrowsingReportRequest::
                DANGEROUS_DOWNLOAD_WARNING,
            actual_report.type());
  EXPECT_EQ(safe_browsing::ClientDownloadResponse::UNCOMMON,
            actual_report.download_verdict());
  EXPECT_EQ(download_url.spec(), actual_report.url());
  EXPECT_FALSE(actual_report.did_proceed());
}

#endif  // FULL_SAFE_BROWSING

// The rest of these tests rely on the download surface, which ChromeOS doesn't
// use (crbug.com/1323505 is tracking Download Bubble on ChromeOS).
#if !BUILDFLAG(IS_CHROMEOS)
// Test that the download surface is shown by starting a download.
//
// TODO(crbug.com/40266279): This test is flaky. Perhaps because it depends on
// focus, in which case it should be an interactive ui test instead of a
// browser test?
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadAndWait) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  DownloadAndWait(browser(), url);

  // The download surface should be visible.
  EXPECT_TRUE(IsDownloadDetailedUiVisible(browser()->window()));
}

// Tests for the download shelf.
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that the download shelf is per-window by starting a download in one
// tab, opening a second tab, closing the shelf, going back to the first tab,
// and checking that the shelf is closed.
IN_PROC_BROWSER_TEST_F(DownloadTest, PerWindowShelf) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-test3.gif");
  base::FilePath download_file(
      FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url);

  base::FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Open a second tab and wait.
  EXPECT_TRUE(chrome::AddSelectedTabWithURL(
      browser(), GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Hide the download shelf.
  browser()->window()->GetDownloadShelf()->Close();
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Go to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // The shelf should now be closed.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Check whether the downloads shelf is closed when the downloads tab is
// invoked.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseShelfOnDownloadsTab) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url);

  // Check state.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Open the downloads tab.
  chrome::ShowDownloads(browser());
  // The download shelf should now be closed.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Flaky. crbug.com/1383009
// Test that when downloading an item in Incognito mode, the download surface is
// not visible after closing the Incognito window.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       DISABLED_IncognitoDownloadSurfaceVisibility) {
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito);

  // Download a file in the Incognito window and wait.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  // Since |incognito| is a separate browser, we have to set it up explicitly.
  incognito->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  DownloadAndWait(incognito, url);

  // Verify that the download surface is showing for the Incognito window.
  EXPECT_TRUE(IsDownloadDetailedUiVisible(incognito->window()));

  // Verify that the regular window does not have a download surface.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));
}

// Download a file in a new window.
// Verify that we have 2 windows, and the download surface is not visible in the
// first window, but is visible in the second window.
// Close the new window.
// Verify that we have 1 window, and the download surface is not visible.
//
// Regression test for http://crbug.com/44454
// TODO(crbug.com/40262026): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_NewWindow DISABLED_NewWindow
#else
#define MAYBE_NewWindow NewWindow
#endif
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_NewWindow) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  const Browser* first_browser = browser();

  // Download a file in a new window and wait.
  DownloadAndWaitWithDisposition(browser(), url,
                                 WindowOpenDisposition::NEW_WINDOW,
                                 ui_test_utils::BROWSER_TEST_NO_WAIT);

  // When the download finishes, the download surface SHOULD NOT be visible in
  // the first window.
  ExpectWindowCountAfterDownload(2);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  // Download surface should close.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));

  // The download surface SHOULD be visible in the second window.
  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  Browser* download_browser =
      ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(download_browser);
  EXPECT_NE(download_browser, browser());
  EXPECT_EQ(1, download_browser->tab_strip_model()->count());
  EXPECT_TRUE(IsDownloadDetailedUiVisible(download_browser->window()));

  // Close the new window.
  chrome::CloseWindow(download_browser);

  ui_test_utils::WaitForBrowserToClose(download_browser);
  EXPECT_EQ(first_browser, browser());
  ExpectWindowCountAfterDownload(1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  // Download surface should close.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));

  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CheckDownload(browser(), file, file);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, PRE_DownloadTest_History) {
  // Download a file and wait for it to be stored.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  HistoryObserver observer(browser()->profile());
  DownloadAndWait(browser(), download_url);
  observer.WaitForStored();
  base::RunLoop run_loop;
  HistoryServiceFactory::GetForProfile(browser()->profile(),
                                       ServiceAccessType::IMPLICIT_ACCESS)
      ->FlushForTest(run_loop.QuitWhenIdleClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadTest_History) {
  // This starts up right after PRE_DownloadTest_History and shares the same
  // profile directory.
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL download_url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> downloads;
  content::DownloadManager* manager = DownloadManagerForBrowser(browser());

  // Wait for the history to be loaded with a single DownloadItem. Check that
  // it's the file that was downloaded in PRE_DownloadTest_History.
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CreatedObserver created_observer(manager);
  created_observer.Wait();
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1UL, downloads.size());
  DownloadItem* item = downloads[0];
  EXPECT_EQ(file.value(), item->GetFullPath().BaseName().value());
  EXPECT_EQ(file.value(), item->GetTargetFilePath().BaseName().value());
  // Only compare the host name, port will be different for each embedded test
  // server session.
  EXPECT_EQ(download_url.host(), item->GetURL().host());
  // The following are set by download-test1.lib.mock-http-headers.
  std::string etag = item->GetETag();
  base::TrimWhitespaceASCII(etag, base::TRIM_ALL, &etag);
  EXPECT_EQ("abracadabra", etag);

  std::string last_modified = item->GetLastModifiedTime();
  base::TrimWhitespaceASCII(last_modified, base::TRIM_ALL, &last_modified);
  EXPECT_EQ("Mon, 13 Nov 2006 20:31:09 GMT", last_modified);

  // Downloads that were restored from history shouldn't cause the download
  // surface to be displayed.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, HiddenDownload) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));

  DownloadManager* download_manager = DownloadManagerForBrowser(browser());
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Download and set IsHiddenDownload to true.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<DownloadUrlParameters> params(
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents, url, TRAFFIC_ANNOTATION_FOR_TESTS));
  params->set_callback(base::BindOnce(&SetHiddenDownloadCallback));
  download_manager->DownloadUrl(std::move(params));
  observer->WaitForFinished();

  // Verify that download surface is not shown.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));
}

// High flake rate; https://crbug.com/1247392.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_AutoOpenClosesSurface) {
  base::FilePath file(FILE_PATH_LITERAL("download-autoopen.txt"));
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/download-autoopen.txt");

  ASSERT_TRUE(
      GetDownloadPrefs(browser())->EnableAutoOpenByUserBasedOnExtension(file));

  DownloadAndWait(browser(), url);

  // Download surface should close.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, CrxDenyInstallClosesSurface) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL extension_url =
      embedded_test_server()->GetURL("/" + std::string(kGoodCrxPath));

  std::unique_ptr<content::DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY));
  NavigateParams params(browser(), extension_url, ui::PAGE_TRANSITION_TYPED);
  params.user_gesture = false;
  ui_test_utils::NavigateToURL(&params);

  observer->WaitForFinished();

  // Download surface should close.
  EXPECT_FALSE(IsDownloadDetailedUiVisible(browser()->window()));
}

// Test that the download UI surface only shows on the appropriate window for a
// web app.
IN_PROC_BROWSER_TEST_F(DownloadTest, WebAppDownloadOnlyShowsUiInWebAppWindow) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  // Load an app.
  webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "testapp", embedded_test_server()->GetURL("/"));
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  DownloadAndWait(app_browser, url);

  EXPECT_FALSE(IsDownloadUiVisible(browser()->window()));
  EXPECT_TRUE(IsDownloadUiVisible(app_browser->window()));
}

// Test that the download UI surface only does not show in a web app window
// for a regular Chrome window's downloads, even if it is the same domain.
IN_PROC_BROWSER_TEST_F(DownloadTest,
                       RegularBrowserDownloadDoesNotShowInWebAppWindow) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  // Load an app.
  webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "testapp", embedded_test_server()->GetURL("/"));
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  DownloadAndWait(browser(), url);

  EXPECT_TRUE(IsDownloadUiVisible(browser()->window()));
  EXPECT_FALSE(IsDownloadUiVisible(app_browser->window()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Test that web app info is properly attached to the download.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadFromWebApp) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  // Load an app.
  webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), "testapp", embedded_test_server()->GetURL("/"));
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  DownloadAndWait(app_browser, url);

  DownloadManager* manager = DownloadManagerForBrowser(app_browser);
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> all_downloads;
  manager->GetAllDownloads(&all_downloads);
  ASSERT_EQ(all_downloads.size(), 1u);
  auto* web_app_data = DownloadItemWebAppData::Get(all_downloads[0]);
  EXPECT_NE(web_app_data, nullptr);
  EXPECT_EQ(web_app_data->id(), app_id);
}
