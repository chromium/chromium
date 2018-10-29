// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_request_interceptor.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_url_loader.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "components/offline_pages/core/stub_system_download_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/filename_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const char kPrivateOfflineFileDir[] = "offline_pages";
const char kPublicOfflineFileDir[] = "public_offline_pages";

const GURL kUrl("http://test.org/page");
const GURL kUrl2("http://test.org/another");
const base::FilePath kFilename1(FILE_PATH_LITERAL("hello.mhtml"));
const base::FilePath kFilename2(FILE_PATH_LITERAL("test.mhtml"));
const base::FilePath kNonexistentFilename(
    FILE_PATH_LITERAL("nonexistent.mhtml"));
const int kFileSize1 = 471;  // Real size of hello.mhtml.
const int kFileSize2 = 444;  // Real size of test.mhtml.
const int kMismatchedFileSize = 99999;
const std::string kDigest1(
    "\x43\x60\x62\x02\x06\x15\x0f\x3e\x77\x99\x3d\xed\xdc\xd4\xe2\x0d\xbe\xbd"
    "\x77\x1a\xfb\x32\x00\x51\x7e\x63\x7d\x3b\x2e\x46\x63\xf6",
    32);  // SHA256 Hash of hello.mhtml.
const std::string kDigest2(
    "\xBD\xD3\x37\x79\xDA\x7F\x4E\x6A\x16\x66\xED\x49\x67\x18\x54\x48\xC6\x8E"
    "\xA1\x47\x16\xA5\x44\x45\x43\xD0\x0E\x04\x9F\x4C\x45\xDC",
    32);  // SHA256 Hash of test.mhtml.
const std::string kMismatchedDigest(
    "\xff\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);  // Wrong SHA256 Hash.

const int kTabId = 1;
const int kBufSize = 1024;

const char kAggregatedRequestResultHistogram[] =
    "OfflinePages.AggregatedRequestResult2";
const char kOpenFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.OpenFileErrorCode";
const char kSeekFileErrorCodeHistogram[] =
    "OfflinePages.RequestJob.SeekFileErrorCode";
const char kAccessEntryPointHistogram[] = "OfflinePages.AccessEntryPoint.";
const char kPageSizeAccessOfflineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Offline.";
const char kPageSizeAccessOnlineHistogramBase[] =
    "OfflinePages.PageSizeOnAccess.Online.";

const int64_t kDownloadId = 42LL;

struct ResponseInfo {
  explicit ResponseInfo(int request_status) : request_status(request_status) {
    DCHECK_NE(net::OK, request_status);
  }
  ResponseInfo(int request_status,
               const std::string& mime_type,
               const std::string& data_received)
      : request_status(request_status),
        mime_type(mime_type),
        data_received(data_received) {}

  int request_status;
  std::string mime_type;
  std::string data_received;
};

class TestURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  typedef base::Callback<void(const ResponseInfo&)> ReadCompletedCallback;

  explicit TestURLRequestDelegate(const ReadCompletedCallback& callback)
      : read_completed_callback_(callback),
        buffer_(base::MakeRefCounted<net::IOBuffer>(kBufSize)),
        request_status_(net::ERR_IO_PENDING) {}

  void OnResponseStarted(net::URLRequest* request, int net_error) override {
    DCHECK_NE(net::ERR_IO_PENDING, net_error);
    if (net_error != net::OK) {
      OnReadCompleted(request, net_error);
      return;
    }
    // Initiate the first read.
    int bytes_read = request->Read(buffer_.get(), kBufSize);
    if (bytes_read >= 0) {
      OnReadCompleted(request, bytes_read);
    } else if (bytes_read != net::ERR_IO_PENDING) {
      request_status_ = bytes_read;
      OnResponseCompleted(request);
    }
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    if (bytes_read > 0)
      data_received_.append(buffer_->data(), bytes_read);

    // If it was not end of stream, request to read more.
    while (bytes_read > 0) {
      bytes_read = request->Read(buffer_.get(), kBufSize);
      if (bytes_read > 0)
        data_received_.append(buffer_->data(), bytes_read);
    }

    request_status_ = (bytes_read >= 0) ? net::OK : bytes_read;
    if (bytes_read != net::ERR_IO_PENDING)
      OnResponseCompleted(request);
  }

 private:
  void OnResponseCompleted(net::URLRequest* request) {
    if (request_status_ != net::OK)
      data_received_.clear();
    if (read_completed_callback_.is_null())
      return;
    std::string mime_type;
    request->GetMimeType(&mime_type);
    read_completed_callback_.Run(
        ResponseInfo(request_status_, mime_type, data_received_));
  }

  ReadCompletedCallback read_completed_callback_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_received_;
  int request_status_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestDelegate);
};

content::WebContents* GetWebContents(content::WebContents* web_contents) {
  return web_contents;
}

bool GetTabId(int tab_id_value,
              content::WebContents* web_content,
              int* tab_id) {
  *tab_id = tab_id_value;
  return true;
}

class TestURLRequestInterceptingJobFactory
    : public net::URLRequestInterceptingJobFactory {
 public:
  TestURLRequestInterceptingJobFactory(
      std::unique_ptr<net::URLRequestJobFactory> job_factory,
      std::unique_ptr<net::URLRequestInterceptor> interceptor,
      content::WebContents* web_contents)
      : net::URLRequestInterceptingJobFactory(std::move(job_factory),
                                              std::move(interceptor)),
        web_contents_(web_contents) {}
  ~TestURLRequestInterceptingJobFactory() override { web_contents_ = nullptr; }

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    net::URLRequestJob* job = net::URLRequestInterceptingJobFactory::
        MaybeCreateJobWithProtocolHandler(scheme, request, network_delegate);
    if (job) {
      OfflinePageRequestJob* offline_page_request_job =
          static_cast<OfflinePageRequestJob*>(job);
      offline_page_request_job->SetWebContentsGetterForTesting(
          base::BindRepeating(&GetWebContents, web_contents_));
      offline_page_request_job->SetTabIdGetterForTesting(
          base::BindRepeating(&GetTabId, kTabId));
    }
    return job;
  }

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestInterceptingJobFactory);
};

class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier() : online_(true) {}
  ~TestNetworkChangeNotifier() override {}

  net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType()
      const override {
    return online_ ? net::NetworkChangeNotifier::CONNECTION_UNKNOWN
                   : net::NetworkChangeNotifier::CONNECTION_NONE;
  }

  bool online() const { return online_; }
  void set_online(bool online) { online_ = online; }

 private:
  bool online_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

// TODO(jianli, carlosk): This should be removed in favor of using with
// OfflinePageTestArchiver.
class TestOfflinePageArchiver : public OfflinePageArchiver {
 public:
  TestOfflinePageArchiver(const GURL& url,
                          const base::FilePath& archive_file_path,
                          int archive_file_size,
                          const std::string& digest)
      : url_(url),
        archive_file_path_(archive_file_path),
        archive_file_size_(archive_file_size),
        digest_(digest) {}
  ~TestOfflinePageArchiver() override {}

  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     content::WebContents* web_contents,
                     CreateArchiveCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  ArchiverResult::SUCCESSFULLY_CREATED, url_,
                                  archive_file_path_, base::string16(),
                                  archive_file_size_, digest_));
  }

  void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const base::FilePath& new_file_path,
      SystemDownloadManager* download_manager,
      PublishArchiveDoneCallback publish_done_callback) override {
    publish_archive_result_.move_result = SavePageResult::SUCCESS;
    publish_archive_result_.new_file_path = offline_page.file_path;
    publish_archive_result_.download_id = 0;
    std::move(publish_done_callback).Run(offline_page, publish_archive_result_);
  }

 private:
  const GURL url_;
  const base::FilePath archive_file_path_;
  const int archive_file_size_;
  const std::string digest_;
  PublishArchiveResult publish_archive_result_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflinePageArchiver);
};

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  class Observer {
   public:
    virtual void OnReceiveRedirect(const GURL& redirected_url) = 0;
    virtual void OnReceiveResponse(
        const network::ResourceResponseHead& response_head) = 0;
    virtual void OnStartLoadingResponseBody() = 0;
    virtual void OnComplete() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit TestURLLoaderClient(Observer* observer)
      : observer_(observer), binding_(this) {}
  ~TestURLLoaderClient() override {}

  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override {
    observer_->OnReceiveResponse(response_head);
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override {
    observer_->OnReceiveRedirect(redirect_info.new_url);
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {}

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    response_body_ = std::move(body);
    observer_->OnStartLoadingResponseBody();
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
    observer_->OnComplete();
  }

  network::mojom::URLLoaderClientPtr CreateInterfacePtr() {
    network::mojom::URLLoaderClientPtr client_ptr;
    binding_.Bind(mojo::MakeRequest(&client_ptr));
    binding_.set_connection_error_handler(base::BindOnce(
        &TestURLLoaderClient::OnConnectionError, base::Unretained(this)));
    return client_ptr;
  }

  mojo::DataPipeConsumerHandle response_body() { return response_body_.get(); }

  const network::URLLoaderCompletionStatus& completion_status() const {
    return completion_status_;
  }

 private:
  void OnConnectionError() {}

  Observer* observer_ = nullptr;
  mojo::Binding<network::mojom::URLLoaderClient> binding_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::URLLoaderCompletionStatus completion_status_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderClient);
};

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++)
    result.append(1, static_cast<char>(i % 256));
  return result;
}

static network::ResourceRequest CreateResourceRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_main_frame) {
  network::ResourceRequest request;
  request.method = method;
  request.headers = extra_headers;
  request.url = url;
  request.is_main_frame = is_main_frame;
  return request;
}

}  // namespace

class OfflinePageRequestHandlerTestBase : public testing::Test {
 public:
  OfflinePageRequestHandlerTestBase();
  ~OfflinePageRequestHandlerTestBase() override {}

  virtual void InterceptRequest(const GURL& url,
                                const std::string& method,
                                const net::HttpRequestHeaders& extra_headers,
                                bool is_main_frame) = 0;

  void SetUp() override;
  void TearDown() override;

  void SimulateHasNetworkConnectivity(bool has_connectivity);
  void RunUntilIdle();
  void WaitForAsyncOperation();

  base::FilePath CreateFileWithContent(const std::string& content);

  // Returns an offline id of the saved page.
  // |file_path| in SavePublicPage and SaveInternalPage can be either absolute
  // or relative. If relative, |file_path| will be appended to public/internal
  // archive directory used for the testing.
  // |file_path| in SavePage should be absolute.
  int64_t SavePublicPage(const GURL& url,
                         const GURL& original_url,
                         const base::FilePath& file_path,
                         int64_t file_size,
                         const std::string& digest);
  int64_t SaveInternalPage(const GURL& url,
                           const GURL& original_url,
                           const base::FilePath& file_path,
                           int64_t file_size,
                           const std::string& digest);
  int64_t SavePage(const GURL& url,
                   const GURL& original_url,
                   const base::FilePath& file_path,
                   int64_t file_size,
                   const std::string& digest);

  OfflinePageItem GetPage(int64_t offline_id);

  void LoadPage(const GURL& url);
  void LoadPageWithHeaders(const GURL& url,
                           const net::HttpRequestHeaders& extra_headers);

  void ReadCompleted(const ResponseInfo& reponse,
                     bool is_offline_page_set_in_navigation_data);

  // Expect exactly one count of |result| UMA reported. No other bucket should
  // have sample.
  void ExpectOneUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult result);
  // Expect exactly |count| of |result| UMA reported. No other bucket should
  // have sample.
  void ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult result,
      int count);
  // Expect one count of |result| UMA reported. Other buckets may have samples
  // as well.
  void ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult result);
  // Expect no samples to have been reported to the aggregated results
  // histogram.
  void ExpectNoSamplesInAggregatedRequestResult();

  void ExpectOpenFileErrorCode(int result);
  void ExpectSeekFileErrorCode(int result);

  void ExpectAccessEntryPoint(
      OfflinePageRequestHandler::AccessEntryPoint entry_point);
  void ExpectNoAccessEntryPoint();

  void ExpectOfflinePageSizeUniqueSample(int bucket, int count);
  void ExpectOfflinePageSizeTotalSuffixCount(int count);
  void ExpectOnlinePageSizeUniqueSample(int bucket, int count);
  void ExpectOnlinePageSizeTotalSuffixCount(int count);
  void ExpectOfflinePageAccessCount(int64_t offline_id, int count);

  void ExpectNoOfflinePageServed(
      int64_t offline_id,
      OfflinePageRequestHandler::AggregatedRequestResult
          expected_request_result);
  void ExpectOfflinePageServed(
      int64_t expected_offline_id,
      int expected_file_size,
      OfflinePageRequestHandler::AggregatedRequestResult
          expected_request_result);

  // Use the offline header with specific reason and offline_id. Return the
  // full header string.
  std::string UseOfflinePageHeader(OfflinePageHeader::Reason reason,
                                   int64_t offline_id);
  std::string UseOfflinePageHeaderForIntent(OfflinePageHeader::Reason reason,
                                            int64_t offline_id,
                                            const GURL& intent_url);

  Profile* profile() { return profile_; }
  content::WebContents* web_contents() const { return web_contents_.get(); }
  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }
  int request_status() const { return response_.request_status; }
  int bytes_read() const { return response_.data_received.length(); }
  const std::string& data_received() const { return response_.data_received; }
  const std::string& mime_type() const { return response_.mime_type; }
  bool is_offline_page_set_in_navigation_data() const {
    return is_offline_page_set_in_navigation_data_;
  }

  bool is_connected_with_good_network() {
    return network_change_notifier_->online() &&
           // Exclude prohibitively slow network.
           !allow_preview() &&
           // Exclude flaky network.
           offline_page_header_.reason != OfflinePageHeader::Reason::NET_ERROR;
  }

  void set_allow_preview(bool allow_preview) { allow_preview_ = allow_preview; }

  bool allow_preview() const { return allow_preview_; }

 private:
  static std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
      content::BrowserContext* context);

  // TODO(https://crbug.com/809610): The static members below will be removed
  // once the reference to BuildTestOfflinePageModel in SetUp is converted to a
  // base::OnceCallback.
  static base::FilePath private_archives_dir_;
  static base::FilePath public_archives_dir_;

  OfflinePageRequestHandler::AccessEntryPoint GetExpectedAccessEntryPoint()
      const;

  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  void OnGetPageByOfflineIdDone(const OfflinePageItem* pages);

  // Runs on IO thread.
  void CreateFileWithContentOnIO(const std::string& content,
                                 const base::Closure& callback);

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.
  int64_t last_offline_id_;
  ResponseInfo response_;
  bool is_offline_page_set_in_navigation_data_;
  OfflinePageItem page_;
  OfflinePageHeader offline_page_header_;

  // These are not thread-safe. But they can be used in the pattern that
  // setting the state is done first from one thread and reading this state
  // can be from any other thread later.
  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  bool allow_preview_ = false;

  // These should only be accessed purely from IO thread.
  base::ScopedTempDir private_archives_temp_base_dir_;
  base::ScopedTempDir public_archives_temp_base_dir_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  int file_name_sequence_num_ = 0;

  bool async_operation_completed_ = false;
  base::Closure async_operation_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestHandlerTestBase);
};

OfflinePageRequestHandlerTestBase::OfflinePageRequestHandlerTestBase()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      last_offline_id_(0),
      response_(net::ERR_IO_PENDING),
      is_offline_page_set_in_navigation_data_(false),
      network_change_notifier_(new TestNetworkChangeNotifier) {}

void OfflinePageRequestHandlerTestBase::SetUp() {
  // Create a test profile.
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_ = profile_manager_.CreateTestingProfile("Profile 1");

  // Create a test web contents.

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  OfflinePageTabHelper::CreateForWebContents(web_contents_.get());
  offline_page_tab_helper_ =
      OfflinePageTabHelper::FromWebContents(web_contents_.get());

  // Set up the factory for testing.
  // Note: The extra dir into the temp folder is needed so that the helper
  // dir-copy operation works properly. That operation copies the source dir
  // final path segment into the destination, and not only its immediate
  // contents so this same-named path here makes the archive dir variable point
  // to the correct location.
  // TODO(romax): add the more recent "temporary" dir here instead of reusing
  // the private one.
  ASSERT_TRUE(private_archives_temp_base_dir_.CreateUniqueTempDir());
  private_archives_dir_ = private_archives_temp_base_dir_.GetPath().AppendASCII(
      kPrivateOfflineFileDir);
  ASSERT_TRUE(public_archives_temp_base_dir_.CreateUniqueTempDir());
  public_archives_dir_ = public_archives_temp_base_dir_.GetPath().AppendASCII(
      kPublicOfflineFileDir);
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &OfflinePageRequestHandlerTestBase::BuildTestOfflinePageModel));

  // Initialize OfflinePageModel.
  OfflinePageModelTaskified* model = static_cast<OfflinePageModelTaskified*>(
      OfflinePageModelFactory::GetForBrowserContext(profile()));

  // Skip the logic to clear the original URL if it is same as final URL.
  // This is needed in order to test that offline page request handler can
  // omit the redirect under this circumstance, for compatibility with the
  // metadata already written to the store.
  model->SetSkipClearingOriginalUrlForTesting();

  // Avoid running the model's maintenance tasks.
  model->DoNotRunMaintenanceTasksForTesting();

  // Move test data files into their respective temporary test directories. The
  // model's maintenance tasks must not be executed in the meantime otherwise
  // these files will be wiped by consistency checks.
  base::FilePath test_data_dir_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_path);
  base::FilePath test_data_private_archives_dir =
      test_data_dir_path.AppendASCII(kPrivateOfflineFileDir);
  ASSERT_TRUE(base::CopyDirectory(test_data_private_archives_dir,
                                  private_archives_dir_.DirName(), true));
  base::FilePath test_data_public_archives_dir =
      test_data_dir_path.AppendASCII(kPublicOfflineFileDir);
  ASSERT_TRUE(base::CopyDirectory(test_data_public_archives_dir,
                                  public_archives_dir_.DirName(), true));

  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void OfflinePageRequestHandlerTestBase::TearDown() {
  EXPECT_TRUE(private_archives_temp_base_dir_.Delete());
  EXPECT_TRUE(public_archives_temp_base_dir_.Delete());
  // This check confirms that the model's maintenance tasks were not executed
  // during the test run.
  histogram_tester_->ExpectTotalCount("OfflinePages.ClearTemporaryPages.Result",
                                      0);
}

void OfflinePageRequestHandlerTestBase::SimulateHasNetworkConnectivity(
    bool online) {
  network_change_notifier_->set_online(online);
}

void OfflinePageRequestHandlerTestBase::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageRequestHandlerTestBase::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_)
    return;
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void OfflinePageRequestHandlerTestBase::CreateFileWithContentOnIO(
    const std::string& content,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  std::string file_name("test");
  file_name += base::IntToString(file_name_sequence_num_++);
  file_name += ".mht";
  temp_file_path_ = temp_dir_.GetPath().AppendASCII(file_name);
  ASSERT_TRUE(base::WriteFile(temp_file_path_, content.c_str(),
                              content.length()) != -1);
  callback.Run();
}

base::FilePath OfflinePageRequestHandlerTestBase::CreateFileWithContent(
    const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &OfflinePageRequestHandlerTestBase::CreateFileWithContentOnIO,
          base::Unretained(this), content, run_loop.QuitClosure()));
  run_loop.Run();
  return temp_file_path_;
}

void OfflinePageRequestHandlerTestBase::
    ExpectOneUniqueSampleForAggregatedRequestResult(
        OfflinePageRequestHandler::AggregatedRequestResult result) {
  histogram_tester_->ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                        static_cast<int>(result), 1);
}

void OfflinePageRequestHandlerTestBase::
    ExpectMultiUniqueSampleForAggregatedRequestResult(
        OfflinePageRequestHandler::AggregatedRequestResult result,
        int count) {
  histogram_tester_->ExpectUniqueSample(kAggregatedRequestResultHistogram,
                                        static_cast<int>(result), count);
}

void OfflinePageRequestHandlerTestBase::
    ExpectOneNonuniqueSampleForAggregatedRequestResult(
        OfflinePageRequestHandler::AggregatedRequestResult result) {
  histogram_tester_->ExpectBucketCount(kAggregatedRequestResultHistogram,
                                       static_cast<int>(result), 1);
}

void OfflinePageRequestHandlerTestBase::
    ExpectNoSamplesInAggregatedRequestResult() {
  histogram_tester_->ExpectTotalCount(kAggregatedRequestResultHistogram, 0);
}

void OfflinePageRequestHandlerTestBase::ExpectOpenFileErrorCode(int result) {
  histogram_tester_->ExpectUniqueSample(kOpenFileErrorCodeHistogram, -result,
                                        1);
}

void OfflinePageRequestHandlerTestBase::ExpectSeekFileErrorCode(int result) {
  histogram_tester_->ExpectUniqueSample(kSeekFileErrorCodeHistogram, -result,
                                        1);
}

void OfflinePageRequestHandlerTestBase::ExpectAccessEntryPoint(
    OfflinePageRequestHandler::AccessEntryPoint entry_point) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kAccessEntryPointHistogram) + kDownloadNamespace,
      static_cast<int>(entry_point), 1);
}

void OfflinePageRequestHandlerTestBase::ExpectNoAccessEntryPoint() {
  EXPECT_TRUE(
      histogram_tester_->GetTotalCountsForPrefix(kAccessEntryPointHistogram)
          .empty());
}

void OfflinePageRequestHandlerTestBase::ExpectOfflinePageSizeUniqueSample(
    int bucket,
    int count) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kPageSizeAccessOfflineHistogramBase) + kDownloadNamespace,
      bucket, count);
}

void OfflinePageRequestHandlerTestBase::ExpectOfflinePageSizeTotalSuffixCount(
    int count) {
  int total_offline_count = 0;
  base::HistogramTester::CountsMap all_offline_counts =
      histogram_tester_->GetTotalCountsForPrefix(
          kPageSizeAccessOfflineHistogramBase);
  for (const std::pair<std::string, base::HistogramBase::Count>&
           namespace_and_count : all_offline_counts) {
    total_offline_count += namespace_and_count.second;
  }
  EXPECT_EQ(count, total_offline_count)
      << "Wrong histogram samples count under prefix "
      << kPageSizeAccessOfflineHistogramBase << "*";
}

void OfflinePageRequestHandlerTestBase::ExpectOnlinePageSizeUniqueSample(
    int bucket,
    int count) {
  histogram_tester_->ExpectUniqueSample(
      std::string(kPageSizeAccessOnlineHistogramBase) + kDownloadNamespace,
      bucket, count);
}

void OfflinePageRequestHandlerTestBase::ExpectOnlinePageSizeTotalSuffixCount(
    int count) {
  int online_count = 0;
  base::HistogramTester::CountsMap all_online_counts =
      histogram_tester_->GetTotalCountsForPrefix(
          kPageSizeAccessOnlineHistogramBase);
  for (const std::pair<std::string, base::HistogramBase::Count>&
           namespace_and_count : all_online_counts) {
    online_count += namespace_and_count.second;
  }
  EXPECT_EQ(count, online_count)
      << "Wrong histogram samples count under prefix "
      << kPageSizeAccessOnlineHistogramBase << "*";
}

void OfflinePageRequestHandlerTestBase::ExpectOfflinePageAccessCount(
    int64_t offline_id,
    int count) {
  OfflinePageItem offline_page = GetPage(offline_id);
  EXPECT_EQ(count, offline_page.access_count);
}

void OfflinePageRequestHandlerTestBase::ExpectNoOfflinePageServed(
    int64_t offline_id,
    OfflinePageRequestHandler::AggregatedRequestResult
        expected_request_result) {
  EXPECT_NE("multipart/related", mime_type());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
  if (expected_request_result !=
      OfflinePageRequestHandler::AggregatedRequestResult::
          AGGREGATED_REQUEST_RESULT_MAX) {
    ExpectOneUniqueSampleForAggregatedRequestResult(expected_request_result);
  }
  ExpectNoAccessEntryPoint();
  ExpectOfflinePageSizeTotalSuffixCount(0);
  ExpectOnlinePageSizeTotalSuffixCount(0);
  ExpectOfflinePageAccessCount(offline_id, 0);
}

void OfflinePageRequestHandlerTestBase::ExpectOfflinePageServed(
    int64_t expected_offline_id,
    int expected_file_size,
    OfflinePageRequestHandler::AggregatedRequestResult
        expected_request_result) {
  EXPECT_EQ(net::OK, request_status());
  EXPECT_EQ("multipart/related", mime_type());
  EXPECT_EQ(expected_file_size, bytes_read());
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(expected_offline_id,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  OfflinePageTrustedState expected_trusted_state =
      private_archives_dir_.IsParent(
          offline_page_tab_helper()->GetOfflinePageForTest()->file_path)
          ? OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR
          : OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR;
  EXPECT_EQ(expected_trusted_state,
            offline_page_tab_helper()->GetTrustedStateForTest());
  if (expected_request_result !=
      OfflinePageRequestHandler::AggregatedRequestResult::
          AGGREGATED_REQUEST_RESULT_MAX) {
    ExpectOneUniqueSampleForAggregatedRequestResult(expected_request_result);
  }
  OfflinePageRequestHandler::AccessEntryPoint expected_entry_point =
      GetExpectedAccessEntryPoint();
  ExpectAccessEntryPoint(expected_entry_point);
  if (is_connected_with_good_network()) {
    ExpectOnlinePageSizeUniqueSample(expected_file_size / 1024, 1);
    ExpectOfflinePageSizeTotalSuffixCount(0);
  } else {
    ExpectOfflinePageSizeUniqueSample(expected_file_size / 1024, 1);
    ExpectOnlinePageSizeTotalSuffixCount(0);
  }
  ExpectOfflinePageAccessCount(expected_offline_id, 1);
}

OfflinePageRequestHandler::AccessEntryPoint
OfflinePageRequestHandlerTestBase::GetExpectedAccessEntryPoint() const {
  switch (offline_page_header_.reason) {
    case OfflinePageHeader::Reason::DOWNLOAD:
      return OfflinePageRequestHandler::AccessEntryPoint::DOWNLOADS;
    case OfflinePageHeader::Reason::NOTIFICATION:
      return OfflinePageRequestHandler::AccessEntryPoint::NOTIFICATION;
    case OfflinePageHeader::Reason::FILE_URL_INTENT:
      return OfflinePageRequestHandler::AccessEntryPoint::FILE_URL_INTENT;
    case OfflinePageHeader::Reason::CONTENT_URL_INTENT:
      return OfflinePageRequestHandler::AccessEntryPoint::CONTENT_URL_INTENT;
    case OfflinePageHeader::Reason::NET_ERROR_SUGGESTION:
      return OfflinePageRequestHandler::AccessEntryPoint::NET_ERROR_PAGE;
    default:
      return OfflinePageRequestHandler::AccessEntryPoint::LINK;
  }
}

std::string OfflinePageRequestHandlerTestBase::UseOfflinePageHeader(
    OfflinePageHeader::Reason reason,
    int64_t offline_id) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  offline_page_header_.reason = reason;
  if (offline_id)
    offline_page_header_.id = base::Int64ToString(offline_id);
  return offline_page_header_.GetCompleteHeaderString();
}

std::string OfflinePageRequestHandlerTestBase::UseOfflinePageHeaderForIntent(
    OfflinePageHeader::Reason reason,
    int64_t offline_id,
    const GURL& intent_url) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  DCHECK(offline_id);
  offline_page_header_.reason = reason;
  offline_page_header_.id = base::Int64ToString(offline_id);
  offline_page_header_.intent_url = intent_url;
  return offline_page_header_.GetCompleteHeaderString();
}

int64_t OfflinePageRequestHandlerTestBase::SavePublicPage(
    const GURL& url,
    const GURL& original_url,
    const base::FilePath& file_path,
    int64_t file_size,
    const std::string& digest) {
  base::FilePath final_path;
  if (file_path.IsAbsolute()) {
    final_path = file_path;
  } else {
    final_path = public_archives_dir_.Append(file_path);
  }

  return SavePage(url, original_url, final_path, file_size, digest);
}

int64_t OfflinePageRequestHandlerTestBase::SaveInternalPage(
    const GURL& url,
    const GURL& original_url,
    const base::FilePath& file_path,
    int64_t file_size,
    const std::string& digest) {
  base::FilePath final_path;
  if (file_path.IsAbsolute()) {
    final_path = file_path;
  } else {
    final_path = private_archives_dir_.Append(file_path);
  }

  return SavePage(url, original_url, final_path, file_size, digest);
}

int64_t OfflinePageRequestHandlerTestBase::SavePage(
    const GURL& url,
    const GURL& original_url,
    const base::FilePath& file_path,
    int64_t file_size,
    const std::string& digest) {
  DCHECK(file_path.IsAbsolute());

  static int item_counter = 0;
  ++item_counter;

  std::unique_ptr<TestOfflinePageArchiver> archiver(
      new TestOfflinePageArchiver(url, file_path, file_size, digest));

  async_operation_completed_ = false;
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id =
      ClientId(kDownloadNamespace, base::IntToString(item_counter));
  save_page_params.original_url = original_url;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver), nullptr,
      base::Bind(&OfflinePageRequestHandlerTestBase::OnSavePageDone,
                 base::Unretained(this)));
  WaitForAsyncOperation();
  return last_offline_id_;
}

// static
std::unique_ptr<KeyedService>
OfflinePageRequestHandlerTestBase::BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  base::FilePath store_path =
      context->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStore(task_runner, store_path));
  std::unique_ptr<SystemDownloadManager> download_manager(
      new StubSystemDownloadManager(kDownloadId, true));

  // Since we're not saving page into temporary dir, it's set the same as the
  // private dir.
  std::unique_ptr<ArchiveManager> archive_manager(
      new ArchiveManager(private_archives_dir_, private_archives_dir_,
                         public_archives_dir_, task_runner));

  return std::unique_ptr<KeyedService>(new OfflinePageModelTaskified(
      std::move(metadata_store), std::move(archive_manager),
      std::move(download_manager), task_runner,
      base::DefaultClock::GetInstance()));
}

// static
base::FilePath OfflinePageRequestHandlerTestBase::private_archives_dir_;
base::FilePath OfflinePageRequestHandlerTestBase::public_archives_dir_;

void OfflinePageRequestHandlerTestBase::OnSavePageDone(SavePageResult result,
                                                       int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  last_offline_id_ = offline_id;

  async_operation_completed_ = true;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

OfflinePageItem OfflinePageRequestHandlerTestBase::GetPage(int64_t offline_id) {
  OfflinePageModelFactory::GetForBrowserContext(profile())->GetPageByOfflineId(
      offline_id,
      base::Bind(&OfflinePageRequestHandlerTestBase::OnGetPageByOfflineIdDone,
                 base::Unretained(this)));
  RunUntilIdle();
  return page_;
}

void OfflinePageRequestHandlerTestBase::OnGetPageByOfflineIdDone(
    const OfflinePageItem* page) {
  ASSERT_TRUE(page);
  page_ = *page;
}

void OfflinePageRequestHandlerTestBase::LoadPage(const GURL& url) {
  InterceptRequest(url, "GET", net::HttpRequestHeaders(),
                   true /* is_main_frame */);
}

void OfflinePageRequestHandlerTestBase::LoadPageWithHeaders(
    const GURL& url,
    const net::HttpRequestHeaders& extra_headers) {
  InterceptRequest(url, "GET", extra_headers, true /* is_main_frame */);
}

void OfflinePageRequestHandlerTestBase::ReadCompleted(
    const ResponseInfo& response,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  response_ = response;
  is_offline_page_set_in_navigation_data_ =
      is_offline_page_set_in_navigation_data;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
}

template <typename T>
class OfflinePageRequestHandlerTest : public OfflinePageRequestHandlerTestBase {
 public:
  OfflinePageRequestHandlerTest() : interceptor_factory_(this) {}

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
                        bool is_main_frame) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    interceptor_factory_.InterceptRequest(url, method, extra_headers,
                                          is_main_frame);
  }

 private:
  T interceptor_factory_;
};

// Builds an OfflinePageRequestJob to test the request interception without
// network service enabled.
class OfflinePageRequestJobBuilder {
 public:
  explicit OfflinePageRequestJobBuilder(
      OfflinePageRequestHandlerTestBase* test_base)
      : test_base_(test_base) {}

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
                        bool is_main_frame);

  OfflinePageRequestHandlerTestBase* test_base() { return test_base_; }

 private:
  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url,
                                                 const std::string& method,
                                                 bool is_main_frame);

  // Runs on IO thread.
  void SetUpNetworkObjectsOnIO();
  void TearDownNetworkObjectsOnIO();
  void InterceptRequestOnIO(const GURL& url,
                            const std::string& method,
                            const net::HttpRequestHeaders& extra_headers,
                            bool is_main_frame);
  void ReadCompletedOnIO(const ResponseInfo& response);
  void TearDownOnReadCompletedOnIO(const ResponseInfo& response,
                                   bool is_offline_page_set_in_navigation_data);

  OfflinePageRequestHandlerTestBase* test_base_;

  // These should only be accessed purely from IO thread.
  std::unique_ptr<net::TestURLRequestContext> test_url_request_context_;
  std::unique_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  std::unique_ptr<net::URLRequestInterceptingJobFactory>
      intercepting_job_factory_;
  std::unique_ptr<TestURLRequestDelegate> url_request_delegate_;
  std::unique_ptr<net::URLRequest> request_;
};

void OfflinePageRequestJobBuilder::SetUpNetworkObjectsOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (test_url_request_context_.get())
    return;

  url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);

  // Create a context with delayed initialization.
  test_url_request_context_.reset(new net::TestURLRequestContext(true));

  // Install the interceptor.
  std::unique_ptr<net::URLRequestInterceptor> interceptor(
      new OfflinePageRequestInterceptor());
  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
      new net::URLRequestJobFactoryImpl());
  intercepting_job_factory_.reset(new TestURLRequestInterceptingJobFactory(
      std::move(job_factory_impl), std::move(interceptor),
      test_base_->web_contents()));

  test_url_request_context_->set_job_factory(intercepting_job_factory_.get());
  test_url_request_context_->Init();
}

void OfflinePageRequestJobBuilder::TearDownNetworkObjectsOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  request_.reset();
  url_request_delegate_.reset();
  intercepting_job_factory_.reset();
  url_request_job_factory_.reset();
  test_url_request_context_.reset();
}

std::unique_ptr<net::URLRequest> OfflinePageRequestJobBuilder::CreateRequest(
    const GURL& url,
    const std::string& method,
    bool is_main_frame) {
  url_request_delegate_ = std::make_unique<TestURLRequestDelegate>(
      base::Bind(&OfflinePageRequestJobBuilder::ReadCompletedOnIO,
                 base::Unretained(this)));

  std::unique_ptr<net::URLRequest> request =
      test_url_request_context_->CreateRequest(url, net::DEFAULT_PRIORITY,
                                               url_request_delegate_.get());
  request->set_method(method);

  content::ResourceRequestInfo::AllocateForTesting(
      request.get(),
      is_main_frame ? content::RESOURCE_TYPE_MAIN_FRAME
                    : content::RESOURCE_TYPE_SUB_FRAME,
      nullptr,
      /*render_process_id=*/1,
      /*render_view_id=*/-1,
      /*render_frame_id=*/1,
      /*is_main_frame=*/true,
      /*allow_download=*/true,
      /*is_async=*/true,
      test_base_->allow_preview() ? content::OFFLINE_PAGE_ON
                                  : content::PREVIEWS_OFF,
      std::make_unique<ChromeNavigationUIData>());

  return request;
}

void OfflinePageRequestJobBuilder::InterceptRequestOnIO(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  SetUpNetworkObjectsOnIO();

  request_ = CreateRequest(url, method, is_main_frame);
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
  request_->Start();
}

void OfflinePageRequestJobBuilder::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&OfflinePageRequestJobBuilder::InterceptRequestOnIO,
                     base::Unretained(this), url, method, extra_headers,
                     is_main_frame));
  base::RunLoop().Run();
}

void OfflinePageRequestJobBuilder::ReadCompletedOnIO(
    const ResponseInfo& response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  bool is_offline_page_set_in_navigation_data = false;
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_.get());
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if (navigation_data) {
    offline_pages::OfflinePageNavigationUIData* offline_page_data =
        navigation_data->GetOfflinePageNavigationUIData();
    if (offline_page_data && offline_page_data->is_offline_page())
      is_offline_page_set_in_navigation_data = true;
  }

  // Since the caller is still holding a request object which we want to dispose
  // as part of tearing down on IO thread, we need to do it in a separate task.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&OfflinePageRequestJobBuilder::TearDownOnReadCompletedOnIO,
                     base::Unretained(this), response,
                     is_offline_page_set_in_navigation_data));
}

void OfflinePageRequestJobBuilder::TearDownOnReadCompletedOnIO(
    const ResponseInfo& response,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  TearDownNetworkObjectsOnIO();

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&OfflinePageRequestHandlerTestBase::ReadCompleted,
                     base::Unretained(test_base()), response,
                     is_offline_page_set_in_navigation_data));
}

// Builds an OfflinePageURLLoader to test the request interception with network
// service enabled.
class OfflinePageURLLoaderBuilder : public TestURLLoaderClient::Observer {
 public:
  explicit OfflinePageURLLoaderBuilder(
      OfflinePageRequestHandlerTestBase* test_base);

  void OnReceiveRedirect(const GURL& redirected_url) override;
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnStartLoadingResponseBody() override;
  void OnComplete() override;

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
                        bool is_main_frame);

  OfflinePageRequestHandlerTestBase* test_base() { return test_base_; }

 private:
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);
  void InterceptRequestOnIO(const GURL& url,
                            const std::string& method,
                            const net::HttpRequestHeaders& extra_headers,
                            bool is_main_frame);
  void MaybeStartLoader(
      const network::ResourceRequest& request,
      content::URLLoaderRequestInterceptor::RequestHandler request_handler);
  void ReadBody();
  void ReadCompletedOnIO(const ResponseInfo& response);

  OfflinePageRequestHandlerTestBase* test_base_;
  std::unique_ptr<ChromeNavigationUIData> navigation_ui_data_;
  std::unique_ptr<OfflinePageURLLoader> url_loader_;
  std::unique_ptr<TestURLLoaderClient> client_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  network::mojom::URLLoaderPtr loader_;
  std::string mime_type_;
  std::string body_;
};

OfflinePageURLLoaderBuilder::OfflinePageURLLoaderBuilder(
    OfflinePageRequestHandlerTestBase* test_base)
    : test_base_(test_base) {
  navigation_ui_data_ = std::make_unique<ChromeNavigationUIData>();
}

void OfflinePageURLLoaderBuilder::OnReceiveRedirect(
    const GURL& redirected_url) {
  InterceptRequestOnIO(redirected_url, "GET", net::HttpRequestHeaders(), true);
}

void OfflinePageURLLoaderBuilder::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  mime_type_ = response_head.mime_type;
}

void OfflinePageURLLoaderBuilder::OnStartLoadingResponseBody() {
  ReadBody();
}

void OfflinePageURLLoaderBuilder::OnComplete() {
  if (client_->completion_status().error_code != net::OK) {
    mime_type_.clear();
    body_.clear();
  }
  ReadCompletedOnIO(
      ResponseInfo(client_->completion_status().error_code, mime_type_, body_));
}

void OfflinePageURLLoaderBuilder::InterceptRequestOnIO(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  client_ = std::make_unique<TestURLLoaderClient>(this);

  network::ResourceRequest request =
      CreateResourceRequest(url, method, extra_headers, is_main_frame);

  request.previews_state = test_base_->allow_preview()
                               ? content::OFFLINE_PAGE_ON
                               : content::PREVIEWS_OFF;

  url_loader_ = OfflinePageURLLoader::Create(
      navigation_ui_data_.get(),
      test_base_->web_contents()->GetMainFrame()->GetFrameTreeNodeId(), request,
      base::BindOnce(&OfflinePageURLLoaderBuilder::MaybeStartLoader,
                     base::Unretained(this), request));

  // |url_loader_| may not be created.
  if (!url_loader_)
    return;

  url_loader_->SetTabIdGetterForTesting(base::BindRepeating(&GetTabId, kTabId));
}

void OfflinePageURLLoaderBuilder::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&OfflinePageURLLoaderBuilder::InterceptRequestOnIO,
                     base::Unretained(this), url, method, extra_headers,
                     is_main_frame));
  base::RunLoop().Run();
}

void OfflinePageURLLoaderBuilder::MaybeStartLoader(
    const network::ResourceRequest& request,
    content::URLLoaderRequestInterceptor::RequestHandler request_handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!request_handler) {
    ReadCompletedOnIO(ResponseInfo(net::ERR_FAILED));
    return;
  }

  // OfflinePageURLLoader decides to handle the request as offline page. Since
  // now, OfflinePageURLLoader will own itself and live as long as its URLLoader
  // and URLLoaderClientPtr are alive.
  url_loader_.release();

  std::move(request_handler)
      .Run(request, mojo::MakeRequest(&loader_), client_->CreateInterfacePtr());
}

void OfflinePageURLLoaderBuilder::ReadBody() {
  while (true) {
    MojoHandle consumer = client_->response_body().value();

    const void* buffer;
    uint32_t num_bytes;
    MojoResult rv = MojoBeginReadData(consumer, nullptr, &buffer, &num_bytes);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
          FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
          base::SequencedTaskRunnerHandle::Get());
      handle_watcher_->Watch(
          client_->response_body(),
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_WATCH_CONDITION_SATISFIED,
          base::BindRepeating(&OfflinePageURLLoaderBuilder::OnHandleReady,
                              base::Unretained(this)));
      return;
    }

    // The pipe was closed.
    if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
      ReadCompletedOnIO(ResponseInfo(net::ERR_FAILED));
      return;
    }

    CHECK_EQ(rv, MOJO_RESULT_OK);

    body_.append(static_cast<const char*>(buffer), num_bytes);
    MojoEndReadData(consumer, num_bytes, nullptr);
  }
}

void OfflinePageURLLoaderBuilder::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    ReadCompletedOnIO(ResponseInfo(net::ERR_FAILED));
    return;
  }
  ReadBody();
}

void OfflinePageURLLoaderBuilder::ReadCompletedOnIO(
    const ResponseInfo& response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  handle_watcher_.reset();
  client_.reset();
  url_loader_.reset();
  loader_.reset();

  bool is_offline_page_set_in_navigation_data = false;
  offline_pages::OfflinePageNavigationUIData* offline_page_data =
      navigation_ui_data_->GetOfflinePageNavigationUIData();
  if (offline_page_data && offline_page_data->is_offline_page())
    is_offline_page_set_in_navigation_data = true;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&OfflinePageRequestHandlerTestBase::ReadCompleted,
                     base::Unretained(test_base()), response,
                     is_offline_page_set_in_navigation_data));
}

// Lists all scenarios we want to test.
typedef testing::Types<OfflinePageRequestJobBuilder,
                       OfflinePageURLLoaderBuilder>
    MyTypes;

TYPED_TEST_CASE(OfflinePageRequestHandlerTest, MyTypes);

TYPED_TEST(OfflinePageRequestHandlerTest, FailedToCreateRequestJob) {
  this->SimulateHasNetworkConnectivity(false);

  // Must be http/https URL.
  this->InterceptRequest(GURL("ftp://host/doc"), "GET",
                         net::HttpRequestHeaders(), true /* is_main_frame */);
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());

  this->InterceptRequest(GURL("file:///path/doc"), "GET",
                         net::HttpRequestHeaders(), true /* is_main_frame */);
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be GET method.
  this->InterceptRequest(kUrl, "POST", net::HttpRequestHeaders(),
                         true /* is_main_frame */);
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());

  this->InterceptRequest(kUrl, "HEAD", net::HttpRequestHeaders(),
                         true /* is_main_frame */);
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be main resource.
  this->InterceptRequest(kUrl, "POST", net::HttpRequestHeaders(),
                         false /* is_main_frame */);
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());

  this->ExpectNoSamplesInAggregatedRequestResult();
  this->ExpectOfflinePageSizeTotalSuffixCount(0);
  this->ExpectOnlinePageSizeTotalSuffixCount(0);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           LoadOfflinePageOnDisconnectedNetwork) {
  this->SimulateHasNetworkConnectivity(false);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, PageNotFoundOnDisconnectedNetwork) {
  this->SimulateHasNetworkConnectivity(false);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(kUrl2);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           NetErrorPageSuggestionOnDisconnectedNetwork) {
  this->SimulateHasNetworkConnectivity(false);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeader(
      OfflinePageHeader::Reason::NET_ERROR_SUGGESTION, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           LoadOfflinePageOnProhibitivelySlowNetwork) {
  this->SimulateHasNetworkConnectivity(true);
  this->set_allow_preview(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           DontLoadReloadOfflinePageOnProhibitivelySlowNetwork) {
  this->SimulateHasNetworkConnectivity(true);
  this->set_allow_preview(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // Treat this as a reloaded page.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::RELOAD, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  // The existentce of RELOAD header will force to treat the network as
  // connected regardless current network condition. So we will fall back to
  // the default handling immediately and no request result should be reported.
  // Passing AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      AGGREGATED_REQUEST_RESULT_MAX);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           PageNotFoundOnProhibitivelySlowNetwork) {
  this->SimulateHasNetworkConnectivity(true);
  this->set_allow_preview(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(kUrl2);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, LoadOfflinePageOnFlakyNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_FLAKY_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, PageNotFoundOnFlakyNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  this->LoadPageWithHeaders(kUrl2, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      PAGE_NOT_FOUND_ON_FLAKY_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           ForceLoadOfflinePageOnConnectedNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, PageNotFoundOnConnectedNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  // Save an offline page.
  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  this->LoadPageWithHeaders(kUrl2, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           DoNotLoadOfflinePageOnConnectedNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(kUrl);

  // When the network is good, we will fall back to the default handling
  // immediately. So no request result should be reported. Passing
  // AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      AGGREGATED_REQUEST_RESULT_MAX);
}

// TODO(https://crbug.com/830282): Flaky on "Marshmallow Phone Tester (rel)".
TYPED_TEST(OfflinePageRequestHandlerTest,
           DISABLED_LoadMostRecentlyCreatedOfflinePage) {
  this->SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  int64_t offline_id1 = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                               kFileSize1, std::string());
  int64_t offline_id2 = this->SaveInternalPage(kUrl, GURL(), kFilename2,
                                               kFileSize2, std::string());

  // Load an URL that matches multiple offline pages. Expect that the most
  // recently created offline page is fetched.
  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id2, kFileSize2,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  this->ExpectOfflinePageAccessCount(offline_id1, 0);
}

TYPED_TEST(OfflinePageRequestHandlerTest, LoadOfflinePageByOfflineID) {
  this->SimulateHasNetworkConnectivity(true);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  int64_t offline_id1 = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                               kFileSize1, std::string());
  int64_t offline_id2 = this->SaveInternalPage(kUrl, GURL(), kFilename2,
                                               kFileSize2, std::string());

  // Load an URL with a specific offline ID designated in the custom header.
  // Expect the offline page matching the offline id is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeader(
      OfflinePageHeader::Reason::DOWNLOAD, offline_id1));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOfflinePageServed(
      offline_id1, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);
}

TYPED_TEST(OfflinePageRequestHandlerTest, FailToLoadByOfflineIDOnUrlMismatch) {
  this->SimulateHasNetworkConnectivity(true);

  int64_t offline_id = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                              kFileSize1, std::string());

  // The offline page found with specific offline ID does not match the passed
  // online URL. Should fall back to find the offline page based on the online
  // URL.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeader(
      OfflinePageHeader::Reason::DOWNLOAD, offline_id));
  this->LoadPageWithHeaders(kUrl2, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      PAGE_NOT_FOUND_ON_CONNECTED_NETWORK);
}

// TODO(https://crbug.com/830282): Flaky on "Marshmallow Phone Tester (rel)".
TYPED_TEST(OfflinePageRequestHandlerTest,
           DISABLED_LoadOfflinePageForUrlWithFragment) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page associated with online URL without fragment.
  int64_t offline_id1 = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                               kFileSize1, std::string());

  // Save another offline page associated with online URL that has a fragment.
  GURL url2_with_fragment(kUrl2.spec() + "#ref");
  int64_t offline_id2 = this->SaveInternalPage(
      url2_with_fragment, GURL(), kFilename2, kFileSize2, std::string());

  this->ExpectOfflinePageAccessCount(offline_id1, 0);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);

  // Loads an url with fragment, that will match the offline URL without the
  // fragment.
  GURL url_with_fragment(kUrl.spec() + "#ref");
  this->LoadPage(url_with_fragment);

  this->ExpectOfflinePageServed(
      offline_id1, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);

  // Loads an url without fragment, that will match the offline URL with the
  // fragment.
  this->LoadPage(kUrl2);

  EXPECT_EQ(kFileSize2, this->bytes_read());
  ASSERT_TRUE(this->offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(
      offline_id2,
      this->offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  this->ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK,
      2);
  this->ExpectOfflinePageSizeTotalSuffixCount(2);
  this->ExpectOnlinePageSizeTotalSuffixCount(0);
  this->ExpectOfflinePageAccessCount(offline_id1, 1);
  this->ExpectOfflinePageAccessCount(offline_id2, 1);

  // Loads an url with fragment, that will match the offline URL with different
  // fragment.
  GURL url2_with_different_fragment(kUrl2.spec() + "#different_ref");
  this->LoadPage(url2_with_different_fragment);

  EXPECT_EQ(kFileSize2, this->bytes_read());
  ASSERT_TRUE(this->offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(
      offline_id2,
      this->offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
  this->ExpectMultiUniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK,
      3);
  this->ExpectOfflinePageSizeTotalSuffixCount(3);
  this->ExpectOnlinePageSizeTotalSuffixCount(0);
  this->ExpectOfflinePageAccessCount(offline_id1, 1);
  this->ExpectOfflinePageAccessCount(offline_id2, 2);
}

TYPED_TEST(OfflinePageRequestHandlerTest, LoadOfflinePageAfterRedirect) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page with same original URL and final URL.
  int64_t offline_id = this->SaveInternalPage(kUrl, kUrl2, kFilename1,
                                              kFileSize1, std::string());

  // This should trigger redirect first.
  this->LoadPage(kUrl2);

  // Passing AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function. Different checks will be done after that.
  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          AGGREGATED_REQUEST_RESULT_MAX);
  this->ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult::
          REDIRECTED_ON_DISCONNECTED_NETWORK);
  this->ExpectOneNonuniqueSampleForAggregatedRequestResult(
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           NoRedirectForOfflinePageWithSameOriginalURL) {
  this->SimulateHasNetworkConnectivity(false);

  // Skip the logic to clear the original URL if it is same as final URL.
  // This is needed in order to test that offline page request handler can
  // omit the redirect under this circumstance, for compatibility with the
  // metadata already written to the store.
  OfflinePageModelTaskified* model = static_cast<OfflinePageModelTaskified*>(
      OfflinePageModelFactory::GetForBrowserContext(this->profile()));
  model->SetSkipClearingOriginalUrlForTesting();

  // Save an offline page with same original URL and final URL.
  int64_t offline_id =
      this->SaveInternalPage(kUrl, kUrl, kFilename1, kFileSize1, std::string());

  // Check if the original URL is still present.
  OfflinePageItem page = this->GetPage(offline_id);
  EXPECT_EQ(kUrl, page.original_url);

  // No redirect should be triggered when original URL is same as final URL.
  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           LoadOfflinePageFromNonExistentInternalFile) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page pointing to non-existent internal archive file.
  int64_t offline_id = this->SaveInternalPage(
      kUrl, GURL(), kNonexistentFilename, kFileSize1, std::string());

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id,
      OfflinePageRequestHandler::AggregatedRequestResult::FILE_NOT_FOUND);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           LoadOfflinePageFromNonExistentPublicFile) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page pointing to non-existent public archive file.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kNonexistentFilename,
                                            kFileSize1, kDigest1);

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id,
      OfflinePageRequestHandler::AggregatedRequestResult::FILE_NOT_FOUND);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           FileSizeMismatchOnDisconnectedNetwork) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kMismatchedFileSize, kDigest1);

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           FileSizeMismatchOnProhibitivelySlowNetwork) {
  this->SimulateHasNetworkConnectivity(true);
  this->set_allow_preview(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kMismatchedFileSize, kDigest1);

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, FileSizeMismatchOnConnectedNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kMismatchedFileSize, kDigest1);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, FileSizeMismatchOnFlakyNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched file size.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kMismatchedFileSize, kDigest1);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_FLAKY_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, DigestMismatchOnDisconnectedNetwork) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kFileSize1, kMismatchedDigest);

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           DigestMismatchOnProhibitivelySlowNetwork) {
  this->SimulateHasNetworkConnectivity(true);
  this->set_allow_preview(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kFileSize1, kMismatchedDigest);

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, DigestMismatchOnConnectedNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kFileSize1, kMismatchedDigest);

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, DigestMismatchOnFlakyNetwork) {
  this->SimulateHasNetworkConnectivity(true);

  // Save an offline page in public location with mismatched digest.
  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                            kFileSize1, kMismatchedDigest);

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      this->UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_FLAKY_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest, FailOnNoDigestForPublicArchiveFile) {
  this->SimulateHasNetworkConnectivity(false);

  // Save an offline page in public location with no digest.
  int64_t offline_id =
      this->SavePublicPage(kUrl, GURL(), kFilename1, kFileSize1, std::string());

  this->LoadPage(kUrl);

  this->ExpectNoOfflinePageServed(
      offline_id, OfflinePageRequestHandler::AggregatedRequestResult::
                      DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK);
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           FailToLoadByOfflineIDOnDigestMismatch) {
  this->SimulateHasNetworkConnectivity(true);

  // Save 2 offline pages associated with same online URL, one in internal
  // location, while another in public location with mismatched digest.
  int64_t offline_id1 = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                               kFileSize1, std::string());
  int64_t offline_id2 = this->SavePublicPage(kUrl, GURL(), kFilename1,
                                             kFileSize1, kMismatchedDigest);

  // The offline page found with specific offline ID does not pass the
  // validation. Though there is another page with the same URL, it will not be
  // fetched. Instead, fall back to load the online URL.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeader(
      OfflinePageHeader::Reason::DOWNLOAD, offline_id2));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectNoOfflinePageServed(
      offline_id1, OfflinePageRequestHandler::AggregatedRequestResult::
                       DIGEST_MISMATCH_ON_CONNECTED_NETWORK);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);
}

TYPED_TEST(OfflinePageRequestHandlerTest, LoadOtherPageOnDigestMismatch) {
  this->SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, one in internal
  // location, while another in public location with mismatched digest.
  int64_t offline_id1 = this->SaveInternalPage(kUrl, GURL(), kFilename1,
                                               kFileSize1, std::string());
  int64_t offline_id2 = this->SavePublicPage(kUrl, GURL(), kFilename2,
                                             kFileSize2, kMismatchedDigest);
  this->ExpectOfflinePageAccessCount(offline_id1, 0);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);

  // There're 2 offline pages matching kUrl. The most recently created one
  // should fail on mistmatched digest. The second most recently created offline
  // page should work.
  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id1, kFileSize1,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  this->ExpectOfflinePageAccessCount(offline_id2, 0);
}

TYPED_TEST(OfflinePageRequestHandlerTest, TinyFile) {
  this->SimulateHasNetworkConnectivity(false);

  std::string expected_data("hello world");
  base::FilePath temp_file_path = this->CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), temp_file_path,
                                            expected_size, expected_digest);

  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, expected_size,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, this->data_received());
}

TYPED_TEST(OfflinePageRequestHandlerTest, SmallFile) {
  this->SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  base::FilePath temp_file_path = this->CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), temp_file_path,
                                            expected_size, expected_digest);

  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, expected_size,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, this->data_received());
}

TYPED_TEST(OfflinePageRequestHandlerTest, BigFile) {
  this->SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(3 * 1024 * 1024));
  base::FilePath temp_file_path = this->CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), temp_file_path,
                                            expected_size, expected_digest);

  this->LoadPage(kUrl);

  this->ExpectOfflinePageServed(
      offline_id, expected_size,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK);
  EXPECT_EQ(expected_data, this->data_received());
}

TYPED_TEST(OfflinePageRequestHandlerTest, LoadFromFileUrlIntent) {
  this->SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. The path to this file will be feed
  // into "intent_url" of extra headers.
  base::FilePath unmodified_file_path =
      this->CreateFileWithContent(expected_data);

  // Create a file with modified data. An offline page is created to associate
  // with this file, but with size and digest matching the unmodified version.
  std::string modified_data(expected_data);
  modified_data[10] = '@';
  base::FilePath modified_file_path =
      this->CreateFileWithContent(modified_data);

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), modified_file_path,
                                            expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // unmodified file. Expect the file from the intent URL is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(unmodified_file_path)));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOfflinePageServed(
      offline_id, expected_size,
      OfflinePageRequestHandler::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK);
  EXPECT_EQ(expected_data, this->data_received());
}

TYPED_TEST(OfflinePageRequestHandlerTest, IntentFileNotFound) {
  this->SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. An offline page is created to associate
  // with this file.
  base::FilePath unmodified_file_path =
      this->CreateFileWithContent(expected_data);

  // Get a path pointing to non-existing file. This path will be feed into
  // "intent_url" of extra headers.
  base::FilePath nonexistent_file_path =
      unmodified_file_path.DirName().AppendASCII("nonexistent");

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), unmodified_file_path,
                                            expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // non-existent file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(nonexistent_file_path)));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  this->ExpectOpenFileErrorCode(net::ERR_FILE_NOT_FOUND);
  EXPECT_EQ(net::ERR_FAILED, this->request_status());
  EXPECT_NE("multipart/related", this->mime_type());
  EXPECT_EQ(0, this->bytes_read());
  EXPECT_FALSE(this->is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());
}

TYPED_TEST(OfflinePageRequestHandlerTest, IntentFileModifiedInTheMiddle) {
  this->SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with modified data in the middle. An offline page is created
  // to associate with this modified file, but with size and digest matching the
  // unmodified version.
  std::string modified_data(expected_data);
  modified_data[10] = '@';
  base::FilePath modified_file_path =
      this->CreateFileWithContent(modified_data);

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), modified_file_path,
                                            expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, this->request_status());
  EXPECT_NE("multipart/related", this->mime_type());
  EXPECT_EQ(0, this->bytes_read());
  // Note that the offline bit is not cleared on purpose due to the fact that
  // other flag, like request status, should already indicate that the offline
  // page fails to load.
  EXPECT_TRUE(this->is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());
}

TYPED_TEST(OfflinePageRequestHandlerTest,
           IntentFileModifiedWithMoreDataAppended) {
  this->SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with more data appended. An offline page is created to
  // associate with this modified file, but with size and digest matching the
  // unmodified version.
  std::string modified_data(expected_data);
  modified_data += "foo";
  base::FilePath modified_file_path =
      this->CreateFileWithContent(modified_data);

  int64_t offline_id = this->SavePublicPage(kUrl, GURL(), modified_file_path,
                                            expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(this->UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  this->LoadPageWithHeaders(kUrl, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, this->request_status());
  EXPECT_NE("multipart/related", this->mime_type());
  EXPECT_EQ(0, this->bytes_read());
  // Note that the offline bit is not cleared on purpose due to the fact that
  // other flag, like request status, should already indicate that the offline
  // page fails to load.
  EXPECT_TRUE(this->is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(this->offline_page_tab_helper()->GetOfflinePageForTest());
}

}  // namespace offline_pages
