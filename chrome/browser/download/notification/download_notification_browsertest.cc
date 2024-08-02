// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "components/user_manager/user_manager.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {

enum {
  DUMMY_ACCOUNT_INDEX = 0,
  PRIMARY_ACCOUNT_INDEX = 1,
  SECONDARY_ACCOUNT_INDEX_START = 2,
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Structure to describe an account info.
struct TestAccountInfo {
  const char* const email;
  const char* const gaia_id;
  const char* const hash;
  const char* const display_name;
};

// Accounts for multi profile test.
static const TestAccountInfo kTestAccounts[] = {
    {"__dummy__@invalid.domain", "10000", "hashdummy", "Dummy Account"},
    {"alice@invalid.domain", "10001", "hashalice", "Alice"},
    {"bob@invalid.domain", "10002", "hashbobbo", "Bob"},
    {"charlie@invalid.domain", "10003", "hashcharl", "Charlie"},
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile), opened_(false) {}
  ~TestChromeDownloadManagerDelegate() override = default;

  // ChromeDownloadManagerDelegate override:
  void OpenDownload(download::DownloadItem* item) override { opened_ = true; }

  // Return if  the download is opened.
  bool opened() const { return opened_; }

 protected:
  // Disable DownloadProtectionService in order to disable content checking.
  safe_browsing::DownloadProtectionService* GetDownloadProtectionService()
      override {
    return nullptr;
  }

 private:
  bool opened_;
};

// Simulates a slow download.
class SlowDownloadInterceptor {
 public:
  static const char kUnknownSizeUrl[];
  static const char kKnownSizeUrl[];
  static const char kFinishDownloadUrl[];
  static const char kErrorDownloadUrl[];

  SlowDownloadInterceptor()
      : handlers_(
            {{kKnownSizeUrl, &SlowDownloadInterceptor::HandleKnownSize},
             {kUnknownSizeUrl, &SlowDownloadInterceptor::HandleUnknownSize},
             {kFinishDownloadUrl, &SlowDownloadInterceptor::HandleFinish},
             {kErrorDownloadUrl, &SlowDownloadInterceptor::HandleError}}),
        interceptor_(base::BindRepeating(&SlowDownloadInterceptor::OnIntercept,
                                         base::Unretained(this))) {}

 private:
  using Handler = void (SlowDownloadInterceptor::*)(
      content::URLLoaderInterceptor::RequestParams*);

  // A wrapper around a URLLoaderInterceptor::RequestParams object that will
  // make sure things are called on the right sequence. Owns itself.
  class PendingRequest {
   public:
    PendingRequest(content::URLLoaderInterceptor::RequestParams&& params)
        : params_(std::move(params)),
          task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

    PendingRequest(const PendingRequest&) = delete;
    PendingRequest& operator=(const PendingRequest&) = delete;

    void Complete(net::Error error_code) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&PendingRequest::CompleteOnOriginalSequence,
                                    base::Unretained(this), error_code));
    }

   private:
    void CompleteOnOriginalSequence(net::Error error_code) {
      network::URLLoaderCompletionStatus status;
      status.error_code = error_code;
      params_.client->OnComplete(status);
      delete this;
    }

    content::URLLoaderInterceptor::RequestParams params_;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  // Can be called on the UI or IO thread depending on which factory we hooked.
  bool OnIntercept(content::URLLoaderInterceptor::RequestParams* params) {
    const auto& it = handlers_.find(params->url_request.url.spec());
    if (it == handlers_.end())
      return false;
    Handler handler = it->second;
    (this->*handler)(params);
    return true;
  }

  void HandleKnownSize(content::URLLoaderInterceptor::RequestParams* params) {
    SendHead(params, "application/octet-stream", /*content_length=*/1024,
             "some random data");
    base::AutoLock lock(lock_);
    pending_requests_.push_back(
        std::make_unique<PendingRequest>(std::move(*params)));
  }

  void HandleUnknownSize(content::URLLoaderInterceptor::RequestParams* params) {
    SendHead(params, "application/octet-stream", /*content_length=*/-1,
             "some random data");
    base::AutoLock lock(lock_);
    pending_requests_.push_back(
        std::make_unique<PendingRequest>(std::move(*params)));
  }

  void HandleFinish(content::URLLoaderInterceptor::RequestParams* params) {
    CompletePendingRequests(net::OK);
    SendOk(params);
  }

  void HandleError(content::URLLoaderInterceptor::RequestParams* params) {
    CompletePendingRequests(net::ERR_CONNECTION_RESET);
    SendOk(params);
  }

  void CompletePendingRequests(net::Error error_code) {
    base::AutoLock lock(lock_);
    for (auto& request : pending_requests_)
      request.release()->Complete(error_code);
    pending_requests_.clear();
  }

  static void SendOk(content::URLLoaderInterceptor::RequestParams* params) {
    std::string response = "OK";
    SendHead(params, "text/http", response.size(), response);
    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    params->client->OnComplete(status);
  }

  static void SendHead(content::URLLoaderInterceptor::RequestParams* params,
                       std::string mime_type,
                       int64_t content_length,
                       std::string data) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Cache-Control: max-age=0\n";
    headers += base::StringPrintf("Content-type: %s\n", mime_type.c_str());
    if (content_length >= 0) {
      headers +=
          base::StringPrintf("Content-Length: %" PRId64 "\n", content_length);
      head->content_length = content_length;
    }
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->headers->GetMimeType(&head->mime_type);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(
        mojo::CreateDataPipe(data.size(), producer_handle, consumer_handle),
        MOJO_RESULT_OK);

    size_t bytes_written = 0;
    MojoResult result = producer_handle->WriteData(
        base::as_byte_span(data), MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
    ASSERT_EQ(MOJO_RESULT_OK, result);
    ASSERT_EQ(data.size(), bytes_written);
    ASSERT_TRUE(consumer_handle.is_valid());
    params->client->OnReceiveResponse(std::move(head),
                                      std::move(consumer_handle), std::nullopt);
  }

  const std::map<std::string, Handler> handlers_;
  base::Lock lock_;
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_
      GUARDED_BY(lock_);
  content::URLLoaderInterceptor interceptor_;
};

const char SlowDownloadInterceptor::kUnknownSizeUrl[] =
    "https://url.handled.by.slow.download/download-unknown-size";
const char SlowDownloadInterceptor::kKnownSizeUrl[] =
    "https://url.handled.by.slow.download/download-known-size";
const char SlowDownloadInterceptor::kFinishDownloadUrl[] =
    "https://url.handled.by.slow.download/download-finish";
const char SlowDownloadInterceptor::kErrorDownloadUrl[] =
    "https://url.handled.by.slow.download/download-error";

// Utility method to retrieve a notification object by id. Warning: this will
// check the last display service that was created. If there's a normal and an
// incognito one, you may want to be explicit.
std::optional<message_center::Notification> GetNotification(
    const std::string& id) {
  return NotificationDisplayServiceTester::Get()->GetNotification(id);
}

// Waits for a notification to be updated/added on |display_service|.
void WaitForDownloadNotificationForDisplayService(
    NotificationDisplayServiceTester* display_service) {
  base::RunLoop run_loop;
  display_service->SetNotificationAddedClosure(base::BindRepeating(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop));
  run_loop.Run();
  display_service->SetNotificationAddedClosure(base::RepeatingClosure());
}

}  // namespace

// Base class for tests of download notifications.
class DownloadNotificationTestBase : public InProcessBrowserTest {
 public:
  DownloadNotificationTestBase() = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());

    interceptor_ = std::make_unique<SlowDownloadInterceptor>();
  }

  void TearDownOnMainThread() override {
    // Make sure any pending requests have finished.
    base::RunLoop().RunUntilIdle();
    interceptor_.reset();
  }

 protected:
  content::DownloadManager* GetDownloadManager(Browser* browser) {
    return browser->profile()->GetDownloadManager();
  }

  // Requests to complete the download and wait for it.
  void CompleteTheDownload(size_t wait_count = 1u) {
    content::DownloadTestObserverTerminal download_terminal_observer(
        GetDownloadManager(browser()), wait_count,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(SlowDownloadInterceptor::kFinishDownloadUrl)));
    download_terminal_observer.WaitForFinished();
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<NotificationDisplayServiceTester> incognito_display_service_;
  std::unique_ptr<SlowDownloadInterceptor> interceptor_;
};

//////////////////////////////////////////////////
// Test with a single profile
//////////////////////////////////////////////////

class DownloadNotificationTest : public DownloadNotificationTestBase {
 public:
  DownloadNotificationTest() = default;

  DownloadNotificationTest(const DownloadNotificationTest&) = delete;
  DownloadNotificationTest& operator=(const DownloadNotificationTest&) = delete;

  ~DownloadNotificationTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();

    std::unique_ptr<TestChromeDownloadManagerDelegate> test_delegate;
    test_delegate =
        std::make_unique<TestChromeDownloadManagerDelegate>(profile);
    test_delegate->GetDownloadIdReceiverCallback().Run(
        download::DownloadItem::kInvalidId + 1);
    DownloadCoreServiceFactory::GetForBrowserContext(profile)
        ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));

    DownloadNotificationTestBase::SetUpOnMainThread();
  }

  TestChromeDownloadManagerDelegate* GetDownloadManagerDelegate() const {
    return static_cast<TestChromeDownloadManagerDelegate*>(
        DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
            ->GetDownloadManagerDelegate());
  }

  void PrepareIncognitoBrowser() {
    incognito_browser_ = CreateIncognitoBrowser();
    Profile* incognito_profile = incognito_browser_->profile();

    std::unique_ptr<TestChromeDownloadManagerDelegate> incognito_test_delegate;
    incognito_test_delegate =
        std::make_unique<TestChromeDownloadManagerDelegate>(incognito_profile);
    DownloadCoreServiceFactory::GetForBrowserContext(incognito_profile)
        ->SetDownloadManagerDelegateForTesting(
            std::move(incognito_test_delegate));

    incognito_display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(
            incognito_browser()->profile());
  }

  TestChromeDownloadManagerDelegate* GetIncognitoDownloadManagerDelegate()
      const {
    Profile* incognito_profile = incognito_browser()->profile();
    return static_cast<TestChromeDownloadManagerDelegate*>(
        DownloadCoreServiceFactory::GetForBrowserContext(incognito_profile)
            ->GetDownloadManagerDelegate());
  }

  void CreateDownload() {
    return CreateDownloadForBrowserAndURL(
        browser(), GURL(SlowDownloadInterceptor::kKnownSizeUrl));
  }

  // Returns the correct display service for the given Browser. If |browser| is
  // null, returns the service for browser().
  NotificationDisplayServiceTester* GetDisplayServiceForBrowser(
      Browser* browser) {
    return (browser && browser == DownloadNotificationTest::incognito_browser())
               ? incognito_display_service_.get()
               : display_service_.get();
  }

  void WaitForDownloadNotification(Browser* browser = nullptr) {
    WaitForDownloadNotificationForDisplayService(
        GetDisplayServiceForBrowser(browser));
  }

  void CreateDownloadForBrowserAndURL(Browser* browser, GURL url) {
    // Starts a download.
    content::DownloadTestObserverInProgress download_in_progress_observer(
        GetDownloadManager(browser), /*wait_count=*/1u);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    download_in_progress_observer.WaitForFinished();

    // Confirms that a download is started.
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
    GetDownloadManager(browser)->GetAllDownloads(&downloads);
    EXPECT_EQ(1u, downloads.size());
    download_item_ = downloads[0].get();
    ASSERT_TRUE(download_item_);

    // Confirms that a notification is created.
    WaitForDownloadNotification(browser);
    CacheNotification(browser);
  }

  void CacheNotification(Browser* browser) {
    ASSERT_FALSE(notification());
    ASSERT_TRUE(notification_id_.empty());

    auto download_notifications =
        GetDisplayServiceForBrowser(browser)->GetDisplayedNotificationsForType(
            NotificationHandler::Type::TRANSIENT);
    ASSERT_EQ(1u, download_notifications.size());
    notification_id_ = download_notifications[0].id();
    EXPECT_FALSE(notification_id_.empty());
    ASSERT_TRUE(notification());
  }

  void CloseNotification() {
    EXPECT_TRUE(notification());
    display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                         notification_id(), true /* by_user */);
    EXPECT_FALSE(notification());
    EXPECT_EQ(0u, GetDownloadNotifications().size());
  }

  void VerifyDownloadState(download::DownloadItem::DownloadState state) {
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
    GetDownloadManager(browser())->GetAllDownloads(&downloads);
    ASSERT_EQ(1u, downloads.size());
    EXPECT_EQ(state, downloads[0]->GetState());
  }

  void VerifyUpdatePropagatesToNotification(download::DownloadItem* item) {
    bool notification_updated = false;
    display_service_->SetNotificationAddedClosure(base::BindRepeating(
        [](bool* updated) { *updated = true; }, &notification_updated));
    item->UpdateObservers();
    EXPECT_TRUE(notification_updated);
    display_service_->SetNotificationAddedClosure(base::RepeatingClosure());
  }

  void InterruptTheDownload() {
    content::DownloadTestObserverInterrupted download_interrupted_observer(
        GetDownloadManager(browser()), 1u, /* wait_count */
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(SlowDownloadInterceptor::kErrorDownloadUrl)));
    download_interrupted_observer.WaitForFinished();
  }

  download::DownloadItem* download_item() const { return download_item_; }
  std::string notification_id() const { return notification_id_; }
  std::optional<message_center::Notification> notification() const {
    return GetNotification(notification_id_);
  }
  Browser* incognito_browser() const { return incognito_browser_; }
  base::FilePath GetDownloadPath() {
    return DownloadPrefs::FromDownloadManager(GetDownloadManager(browser()))
        ->DownloadPath();
  }
  std::vector<message_center::Notification> GetDownloadNotifications() {
    return display_service_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::TRANSIENT);
  }

 private:
  raw_ptr<download::DownloadItem, DanglingUntriaged> download_item_ = nullptr;
  raw_ptr<Browser, DanglingUntriaged> incognito_browser_ = nullptr;
  std::string notification_id_;
};

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadFile) {
  CreateDownload();

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            notification()->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification()->type());

  // Confirms that the download update is delivered to the notification.
  EXPECT_TRUE(notification());
  VerifyUpdatePropagatesToNotification(download_item());

  CompleteTheDownload();

  // Checks strings.
  ASSERT_TRUE(notification());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_COMPLETE_TITLE),
            notification()->title());
  EXPECT_EQ(download_item()->GetFileNameToReportUser().LossyDisplayName(),
            notification()->message());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification()->type());

  // Confirms that there is only one notification.
  ASSERT_EQ(1u, GetDownloadNotifications().size());

  // Try to open the downloaded item by clicking the notification.
  EXPECT_FALSE(GetDownloadManagerDelegate()->opened());
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification_id(), std::nullopt,
                                  std::nullopt);
  EXPECT_TRUE(GetDownloadManagerDelegate()->opened());

  EXPECT_FALSE(GetNotification(notification_id()));
}

// Flaky test: crbug/822470.
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DISABLED_DownloadDangerousFile) {
  GURL download_url(
      embedded_test_server()->GetURL("/downloads/dangerous/dangerous.swf"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()), 1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  base::FilePath filename = download_item()->GetFileNameToReportUser();

  // Checks the download status.
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download_item()->GetDangerType());
  EXPECT_TRUE(download_item()->IsDangerous());

  // Clicks the "keep" button.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification_id(), 1,  // 2nd button: "Keep"
                                  std::nullopt);

  // The notification is closed and re-shown.
  EXPECT_TRUE(notification());

  // Checks the download status.
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED,
            download_item()->GetDangerType());
  EXPECT_FALSE(download_item()->IsDangerous());

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  // Checks the download status.
  EXPECT_FALSE(download_item()->IsDangerous());
  EXPECT_EQ(download::DownloadItem::COMPLETE, download_item()->GetState());

  // Checks the downloaded file.
  EXPECT_TRUE(base::PathExists(GetDownloadPath().Append(filename.BaseName())));
}

// Disabled due to timeouts; see https://crbug.com/810302.
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DISABLED_DiscardDangerousFile) {
  GURL download_url(
      embedded_test_server()->GetURL("/downloads/dangerous/dangerous.swf"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()), 1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  base::FilePath filename = download_item()->GetFileNameToReportUser();

  // Checks the download status.
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            download_item()->GetDangerType());
  EXPECT_TRUE(download_item()->IsDangerous());

  // Ensures the notification exists.
  EXPECT_TRUE(notification());

  // Clicks the "Discard" button.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification_id(),
                                  0,  // 1st button: "Discard"
                                  std::nullopt);

  EXPECT_FALSE(notification());

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  // Checks there is neither any download nor any notification.
  EXPECT_FALSE(notification());
  EXPECT_EQ(0u, GetDownloadNotifications().size());
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());

  // Checks the downloaded file doesn't exist.
  EXPECT_FALSE(base::PathExists(GetDownloadPath().Append(filename.BaseName())));
}

// Disabled due to timeouts; see https://crbug.com/810302.
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DISABLED_DownloadImageFile) {
  GURL download_url(
      embedded_test_server()->GetURL("/downloads/image-octet-stream.png"));

  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser()), 1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_IGNORE);

  CreateDownloadForBrowserAndURL(browser(), download_url);

  // Wait for the download completion.
  download_terminal_observer.WaitForFinished();

  WaitForDownloadNotification();
  EXPECT_FALSE(notification()->image().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       CloseNotificationAfterDownload) {
  CreateDownload();

  CompleteTheDownload();

  CloseNotification();

  VerifyDownloadState(download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       CloseNotificationWhileDownloading) {
  CreateDownload();

  CloseNotification();

  VerifyDownloadState(download::DownloadItem::IN_PROGRESS);

  CompleteTheDownload();

  EXPECT_TRUE(notification());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, InterruptDownload) {
  CreateDownload();

  InterruptTheDownload();

  EXPECT_EQ(1u, GetDownloadNotifications().size());
  ASSERT_TRUE(notification());

  // Checks strings.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            notification()->title());
  EXPECT_NE(notification()->message().find(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_INTERRUPTED,
                l10n_util::GetStringUTF16(
                    IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_NETWORK_ERROR))),
            std::string::npos);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification()->type());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       InterruptDownloadAfterClosingNotification) {
  CreateDownload();

  CloseNotification();

  // Confirms that a download is still in progress.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  content::DownloadManager* download_manager = GetDownloadManager(browser());
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, downloads[0]->GetState());

  // Installs observers before requesting the completion.
  content::DownloadTestObserverInterrupted download_terminal_observer(
      download_manager, 1u, /* wait_count */
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  InterruptTheDownload();

  // Confirms that there is only one notification.
  EXPECT_EQ(1u, GetDownloadNotifications().size());
  ASSERT_TRUE(notification());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, DownloadRemoved) {
  CreateDownload();

  EXPECT_TRUE(notification());

  download_item()->Remove();
  EXPECT_FALSE(notification());

  // Confirms that the download item is removed.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());
}

// Test is flaky: https://crbug.com/1252430
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DISABLED_DownloadMultipleFiles) {
  GURL url1(SlowDownloadInterceptor::kUnknownSizeUrl);
  GURL url2(SlowDownloadInterceptor::kKnownSizeUrl);

  // Starts the 1st download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  WaitForDownloadNotification();
  auto notifications = GetDownloadNotifications();
  ASSERT_EQ(1u, notifications.size());
  std::string notification_id1 = notifications[0].id();
  EXPECT_FALSE(notification_id1.empty());

  // Confirms that there is a download.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  download::DownloadItem* download1 = downloads[0];

  // Starts the 2nd download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  WaitForDownloadNotification();

  // Confirms that there are 2 downloads.
  downloads.clear();
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());
  download::DownloadItem* download2;
  if (download1 == downloads[0])
    download2 = downloads[1];
  else if (download1 == downloads[1])
    download2 = downloads[0];
  else
    NOTREACHED_IN_MIGRATION();
  EXPECT_NE(download1, download2);

  notifications = GetDownloadNotifications();

  // Confirms that there are 2 notifications.
  EXPECT_EQ(2u, notifications.size());

  std::string notification_id2;
  for (const auto& notification : notifications) {
    if (notification.id() == notification_id1) {
      continue;
    }

    notification_id2 = notification.id();
  }
  EXPECT_FALSE(notification_id2.empty());

  // Confirms that the old one is low priority, and the new one is default.
  EXPECT_EQ(message_center::LOW_PRIORITY,
            GetNotification(notification_id1)->priority());
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id2)->priority());

  // Confirms that the updates of both download are delivered to the
  // notifications.
  VerifyUpdatePropagatesToNotification(download1);
  VerifyUpdatePropagatesToNotification(download2);

  // Confirms the correct type of notification while download is in progress.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            GetNotification(notification_id2)->type());

  // Requests to complete the downloads.
  CompleteTheDownload(2);

  // Confirms that the both notifications are visible.
  notifications = GetDownloadNotifications();
  EXPECT_EQ(2u, notifications.size());

  ASSERT_TRUE(GetNotification(notification_id1));
  ASSERT_TRUE(GetNotification(notification_id2));

  // Confirms that both ask to be re-shown when finished.
  EXPECT_TRUE(GetNotification(notification_id1)->renotify());
  EXPECT_TRUE(GetNotification(notification_id2)->renotify());

  // Confirms that both are default priority after finishing.
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id1)->priority());
  EXPECT_EQ(message_center::DEFAULT_PRIORITY,
            GetNotification(notification_id2)->priority());

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id1)->type());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            GetNotification(notification_id2)->type());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DownloadMultipleFilesOneByOne) {
  CreateDownload();
  download::DownloadItem* first_download_item = download_item();
  std::string first_notification_id = notification_id();

  CompleteTheDownload();
  EXPECT_EQ(download::DownloadItem::COMPLETE, first_download_item->GetState());

  // Checks the message center.
  EXPECT_TRUE(notification());

  // Starts the second download.
  GURL url(SlowDownloadInterceptor::kKnownSizeUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WaitForDownloadNotification();

  // Confirms that the second notification is created.
  auto notifications = GetDownloadNotifications();
  ASSERT_EQ(2u, notifications.size());
  std::string second_notification_id =
      notifications[(notifications[0].id() == notification_id() ? 1 : 0)].id();
  EXPECT_FALSE(second_notification_id.empty());
  ASSERT_TRUE(GetNotification(second_notification_id));

  // Confirms that the second download is also started.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(2u, downloads.size());
  EXPECT_TRUE(first_download_item == downloads[0] ||
              first_download_item == downloads[1]);
  download::DownloadItem* second_download_item =
      downloads[first_download_item == downloads[0] ? 1 : 0];

  EXPECT_EQ(download::DownloadItem::IN_PROGRESS,
            second_download_item->GetState());

  // Requests to complete the second download.
  CompleteTheDownload();

  EXPECT_EQ(2u, GetDownloadNotifications().size());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest, CancelDownload) {
  CreateDownload();

  // Cancels the notification by clicking the "cancel" button.
  display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                  notification_id(), 1, std::nullopt);
  EXPECT_FALSE(notification());
  EXPECT_EQ(0u, GetDownloadNotifications().size());

  // Confirms that the download is cancelled.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::CANCELLED, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DownloadCancelledByUserExternally) {
  CreateDownload();

  // Cancels the notification through the DownloadItem.
  download_item()->Cancel(true /* by_user */);
  EXPECT_FALSE(notification());
  EXPECT_EQ(0u, GetDownloadNotifications().size());

  // Confirms that the download is cancelled.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::CANCELLED, downloads[0]->GetState());
}

// TODO(crbug.com/41445173): Reenable this.
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DISABLED_IncognitoDownloadFile) {
  PrepareIncognitoBrowser();

  // Starts an incognito download.
  CreateDownloadForBrowserAndURL(incognito_browser(),
                                 GURL(SlowDownloadInterceptor::kKnownSizeUrl));

  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE,
                download_item()->GetFileNameToReportUser().LossyDisplayName()),
            notification()->title());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification()->type());
  EXPECT_TRUE(content::DownloadItemUtils::GetBrowserContext(download_item())
                  ->IsOffTheRecord());

  // Requests to complete the download.
  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(incognito_browser()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser(), GURL(SlowDownloadInterceptor::kFinishDownloadUrl)));
  download_terminal_observer.WaitForFinished();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_COMPLETE_TITLE),
            notification()->title());
  EXPECT_EQ(download_item()->GetFileNameToReportUser().LossyDisplayName(),
            notification()->message());
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification()->type());

  // Try to open the downloaded item by clicking the notification.
  EXPECT_TRUE(incognito_display_service_->GetNotification(notification_id()));
  EXPECT_FALSE(GetIncognitoDownloadManagerDelegate()->opened());
  incognito_display_service_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification_id(), std::nullopt,
      std::nullopt);
  EXPECT_TRUE(GetIncognitoDownloadManagerDelegate()->opened());
  EXPECT_FALSE(GetDownloadManagerDelegate()->opened());

  EXPECT_FALSE(incognito_display_service_->GetNotification(notification_id()));
  chrome::CloseWindow(incognito_browser());
}

// TODO(crbug.com/41445173): Reenable this.
IN_PROC_BROWSER_TEST_F(DownloadNotificationTest,
                       DISABLED_SimultaneousIncognitoAndNormalDownloads) {
  PrepareIncognitoBrowser();

  GURL url_incognito(SlowDownloadInterceptor::kUnknownSizeUrl);
  GURL url_normal(SlowDownloadInterceptor::kKnownSizeUrl);

  // Starts the incognito download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser(), url_incognito));
  WaitForDownloadNotification(incognito_browser());
  auto incognito_notifications =
      incognito_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, incognito_notifications.size());
  std::string notification_id1 = incognito_notifications[0].id();
  EXPECT_FALSE(notification_id1.empty());

  // Confirms that there is a download.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(0u, downloads.size());
  downloads.clear();
  GetDownloadManager(incognito_browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  download::DownloadItem* download_incognito = downloads[0];

  // Starts the normal download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_normal));
  WaitForDownloadNotification();
  auto normal_notifications = GetDownloadNotifications();
  ASSERT_EQ(1u, normal_notifications.size());
  std::string notification_id2 = normal_notifications[0].id();
  EXPECT_FALSE(notification_id2.empty());

  // Confirms that there are 2 downloads.
  downloads.clear();
  GetDownloadManager(browser())->GetAllDownloads(&downloads);
  download::DownloadItem* download_normal = downloads[0];
  EXPECT_EQ(1u, downloads.size());
  EXPECT_NE(download_normal, download_incognito);
  downloads.clear();
  GetDownloadManager(incognito_browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download_incognito, downloads[0]);

  // Confirms the types of download notifications are correct.
  auto incognito_notification =
      incognito_display_service_->GetNotification(notification_id1);
  ASSERT_TRUE(incognito_notification);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            incognito_notification->type());
  EXPECT_EQ(-1, incognito_notification->progress());

  auto normal_notification =
      display_service_->GetNotification(notification_id2);
  ASSERT_TRUE(normal_notification);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            normal_notification->type());
  EXPECT_LE(0, normal_notification->progress());

  EXPECT_TRUE(content::DownloadItemUtils::GetBrowserContext(download_incognito)
                  ->IsOffTheRecord());
  EXPECT_FALSE(content::DownloadItemUtils::GetBrowserContext(download_normal)
                   ->IsOffTheRecord());

  // Request to complete the normal download.
  CompleteTheDownload();

  // Confirms the types of download notifications are correct.
  incognito_notification =
      incognito_display_service_->GetNotification(notification_id1);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            incognito_notification->type());
  normal_notification = display_service_->GetNotification(notification_id2);
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            normal_notification->type());

  chrome::CloseWindow(incognito_browser());
}

// These tests have ash dependency so they are only available for ash.
// TODO(crbug.com/40204280): Enable these tests for Lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)

//////////////////////////////////////////////////
// Test with multi profiles
//////////////////////////////////////////////////

class MultiProfileDownloadNotificationTest
    : public DownloadNotificationTestBase {
 public:
  MultiProfileDownloadNotificationTest() = default;

  MultiProfileDownloadNotificationTest(
      const MultiProfileDownloadNotificationTest&) = delete;
  MultiProfileDownloadNotificationTest& operator=(
      const MultiProfileDownloadNotificationTest&) = delete;

  ~MultiProfileDownloadNotificationTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DownloadNotificationTestBase::SetUpCommandLine(command_line);

    // Logs in to a dummy profile.
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].email);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].hash);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(ash::switches::kProfileRequiresPolicy,
                                    "false");
  }

  // Logs in to the primary profile.
  void SetUpOnMainThread() override {
    const TestAccountInfo& info = kTestAccounts[PRIMARY_ACCOUNT_INDEX];

    AddUser(info, true);
    DownloadNotificationTestBase::SetUpOnMainThread();
  }

  // Loads all users to the current session and sets up necessary fields.
  // This is used for preparing all accounts in PRE_ test setup, and for testing
  // actual login behavior.
  void AddAllUsers() {
    for (size_t i = 0; i < std::size(kTestAccounts); ++i) {
      // The primary account was already set up in SetUpOnMainThread, so skip it
      // here.
      if (i == PRIMARY_ACCOUNT_INDEX)
        continue;
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
    }
  }

  Profile* GetProfileByIndex(int index) {
    return g_browser_process->profile_manager()->GetProfileByPath(
        ash::ProfileHelper::GetProfilePathByUserIdHash(
            kTestAccounts[index].hash));
  }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    if (log_in) {
      session_manager::SessionManager::Get()->CreateSession(
          AccountId::FromUserEmailGaiaId(info.email, info.gaia_id), info.hash,
          false);
    }
    user_manager::UserManager::Get()->SaveUserDisplayName(
        AccountId::FromUserEmailGaiaId(info.email, info.gaia_id),
        base::UTF8ToUTF16(info.display_name));
    Profile& profile = profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(),
        ash::ProfileHelper::GetProfilePathByUserIdHash(info.hash));

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile);
    if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync))
      signin::MakePrimaryAccountAvailable(identity_manager, info.email,
                                          signin::ConsentLevel::kSync);
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service1_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service2_;
};

IN_PROC_BROWSER_TEST_F(MultiProfileDownloadNotificationTest,
                       PRE_DownloadMultipleFiles) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileDownloadNotificationTest,
                       DownloadMultipleFiles) {
  AddAllUsers();

  GURL url(SlowDownloadInterceptor::kUnknownSizeUrl);

  Profile* profile1 = GetProfileByIndex(1);
  Profile* profile2 = GetProfileByIndex(2);
  Browser* browser1 = CreateBrowser(profile1);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_NE(browser1, browser2);

  display_service1_ =
      std::make_unique<NotificationDisplayServiceTester>(profile1);
  display_service2_ =
      std::make_unique<NotificationDisplayServiceTester>(profile2);

  // First user starts a download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, url));

  WaitForDownloadNotificationForDisplayService(display_service1_.get());

  // Confirms that the download is started.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  GetDownloadManager(browser1)->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  download::DownloadItem* download1 = downloads[0];

  // Confirms that a download notification is generated.
  auto notifications1 = display_service1_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications1.size());
  std::string notification_id_user1 = notifications1[0].id();
  EXPECT_FALSE(notification_id_user1.empty());

  // Second user starts a download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, url));

  WaitForDownloadNotificationForDisplayService(display_service2_.get());
  auto notifications2 = display_service2_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1u, notifications2.size());
  std::string notification_id_user2 = notifications2[0].id();
  EXPECT_FALSE(notification_id_user2.empty());

  // Confirms that the second user has only 1 download.
  downloads.clear();
  GetDownloadManager(browser2)->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  // Second user starts another download.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser2, url));

  WaitForDownloadNotificationForDisplayService(display_service2_.get());
  notifications2 = display_service2_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(2u, notifications2.size());

  // Confirms that the second user has 2 downloads.
  downloads.clear();
  GetDownloadManager(browser2)->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());
  download::DownloadItem* download2 = downloads[0];
  download::DownloadItem* download3 = downloads[1];
  EXPECT_NE(download1, download2);
  EXPECT_NE(download1, download3);
  EXPECT_NE(download2, download3);

  // Confirms that the first user still has only 1 download.
  downloads.clear();
  GetDownloadManager(browser1)->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(download1, downloads[0]);

  // Confirms the types of download notifications are correct.
  // Normal notification for user1.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS,
            display_service1_->GetNotification(notification_id_user1)->type());
  EXPECT_EQ(
      -1,
      display_service1_->GetNotification(notification_id_user1)->progress());
  // Normal notifications for user2.
  notifications2 = display_service2_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  EXPECT_EQ(2u, notifications2.size());
  for (const auto& notification : notifications2) {
    EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());
    EXPECT_EQ(-1, notification.progress());
  }

  // Requests to complete the downloads.
  content::DownloadTestObserverTerminal download_terminal_observer(
      GetDownloadManager(browser1), 1u /* wait_count */,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  content::DownloadTestObserverTerminal download_terminal_observer2(
      GetDownloadManager(browser2), 2u /* wait_count */,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, GURL(SlowDownloadInterceptor::kFinishDownloadUrl)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, GURL(SlowDownloadInterceptor::kFinishDownloadUrl)));
  download_terminal_observer.WaitForFinished();
  download_terminal_observer2.WaitForFinished();

  // Confirms the types of download notifications are correct.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE,
            display_service1_->GetNotification(notification_id_user1)->type());
  notifications2 = display_service2_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  EXPECT_EQ(2u, notifications2.size());
  for (const auto& notification : notifications2) {
    EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
