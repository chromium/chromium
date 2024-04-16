// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_url_loader.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_taskified.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_test_archive_publisher.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/wait.h"
#include "net/base/filename_util.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/gcm_driver/instance_id/instance_id_android.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace offline_pages {

namespace {

constexpr char kPrivateOfflineFileDir[] = "offline_pages";
constexpr char kPublicOfflineFileDir[] = "public_offline_pages";

const base::FilePath kFilename1(FILE_PATH_LITERAL("hello.mhtml"));
const base::FilePath kFilename2(FILE_PATH_LITERAL("welcome.mhtml"));
const base::FilePath kNonexistentFilename(
    FILE_PATH_LITERAL("nonexistent.mhtml"));
constexpr int kFileSize1 = 471;  // Real size of hello.mhtml.
constexpr int kFileSize2 = 461;  // Real size of welcome.mhtml.
const std::string kDigest1(
    "\x43\x60\x62\x02\x06\x15\x0f\x3e\x77\x99\x3d\xed\xdc\xd4\xe2\x0d\xbe\xbd"
    "\x77\x1a\xfb\x32\x00\x51\x7e\x63\x7d\x3b\x2e\x46\x63\xf6",
    32);  // SHA256 Hash of hello.mhtml.
const std::string kDigest2(
    "\xBD\xD3\x37\x79\xDA\x7F\x4E\x6A\x16\x66\xED\x49\x67\x18\x54\x48\xC6\x8E"
    "\xA1\x47\x16\xA5\x44\x45\x43\xD0\x0E\x04\x9F\x4C\x45\xDC",
    32);  // SHA256 Hash of welcome.mhtml.
const std::string kMismatchedDigest(
    "\xff\x64\xF9\x7C\x94\xE5\x9E\x91\x83\x3D\x41\xB0\x36\x90\x0A\xDF\xB3\xB1"
    "\x5C\x13\xBE\xB8\x35\x8C\xF6\x5B\xC4\xB5\x5A\xFC\x3A\xCC",
    32);  // Wrong SHA256 Hash.

constexpr int kTabId = 1;

constexpr int64_t kDownloadId = 42LL;

constexpr char kTestUrl[] = "http://test.org/page";
constexpr char kTestUrl2[] = "http://test.org/another";

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

bool GetTabId(int tab_id_value,
              content::WebContents* web_content,
              int* tab_id) {
  *tab_id = tab_id_value;
  return true;
}

class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier() : online_(true) {}

  TestNetworkChangeNotifier(const TestNetworkChangeNotifier&) = delete;
  TestNetworkChangeNotifier& operator=(const TestNetworkChangeNotifier&) =
      delete;

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
};

class TestURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  class Observer {
   public:
    virtual void OnReceiveRedirect(const GURL& redirected_url) = 0;
    virtual void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr response_head) = 0;
    virtual void OnComplete() = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit TestURLLoaderClient(Observer* observer) : observer_(observer) {}

  TestURLLoaderClient(const TestURLLoaderClient&) = delete;
  TestURLLoaderClient& operator=(const TestURLLoaderClient&) = delete;

  ~TestURLLoaderClient() override {}

  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    response_body_ = std::move(body);
    observer_->OnReceiveResponse(std::move(response_head));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    observer_->OnReceiveRedirect(redirect_info.new_url);
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    completion_status_ = status;
    observer_->OnComplete();
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient> CreateRemote() {
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestURLLoaderClient::OnMojoDisconnect, base::Unretained(this)));
    return client_remote;
  }

  mojo::DataPipeConsumerHandle response_body() { return response_body_.get(); }

  const network::URLLoaderCompletionStatus& completion_status() const {
    return completion_status_;
  }

 private:
  void OnMojoDisconnect() {}

  raw_ptr<Observer> observer_ = nullptr;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::URLLoaderCompletionStatus completion_status_;
};

// Helper function to make a character array filled with |size| bytes of
// test content.
std::string MakeContentOfSize(int size) {
  EXPECT_GE(size, 0);
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; i++) {
    result.append(1, static_cast<char>(i % 256));
  }
  return result;
}

static network::ResourceRequest CreateResourceRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_outermost_main_frame) {
  network::ResourceRequest request;
  request.method = method;
  request.headers = extra_headers;
  request.url = url;
  request.is_outermost_main_frame = is_outermost_main_frame;
  return request;
}

}  // namespace

class OfflinePageRequestHandlerTest;

// Builds an OfflinePageURLLoader to test the request interception with network
// service enabled.
class OfflinePageURLLoaderBuilder : public TestURLLoaderClient::Observer {
 public:
  explicit OfflinePageURLLoaderBuilder(OfflinePageRequestHandlerTest* test);

  void OnReceiveRedirect(const GURL& redirected_url) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnComplete() override;

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
                        bool is_outermost_main_frame);

  OfflinePageRequestHandlerTest* test() { return test_; }

  void Quit() { std::move(quit_closure_).Run(); }

 private:
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);
  void InterceptRequestInternal(const GURL& url,
                                const std::string& method,
                                const net::HttpRequestHeaders& extra_headers,
                                bool is_outermost_main_frame);
  void MaybeStartLoader(
      const network::ResourceRequest& request,
      content::URLLoaderRequestInterceptor::RequestHandler request_handler);
  void ReadBody();
  void ReadCompleted(const ResponseInfo& response);

  raw_ptr<OfflinePageRequestHandlerTest> test_;
  std::unique_ptr<ChromeNavigationUIData> navigation_ui_data_;
  std::unique_ptr<OfflinePageURLLoader> url_loader_;
  std::unique_ptr<TestURLLoaderClient> client_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  mojo::Remote<network::mojom::URLLoader> loader_;
  std::string mime_type_;
  std::string body_;
  base::OnceClosure quit_closure_;
};

class OfflinePageRequestHandlerTest : public testing::Test {
 public:
  OfflinePageRequestHandlerTest();

  OfflinePageRequestHandlerTest(const OfflinePageRequestHandlerTest&) = delete;
  OfflinePageRequestHandlerTest& operator=(
      const OfflinePageRequestHandlerTest&) = delete;

  ~OfflinePageRequestHandlerTest() override {}

  void SetUp() override;
  void TearDown() override;

  void InterceptRequest(const GURL& url,
                        const std::string& method,
                        const net::HttpRequestHeaders& extra_headers,
                        bool is_outermost_main_frame);
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

  void ExpectNoOfflinePageServed(int64_t offline_id);
  void ExpectOfflinePageServed(int64_t expected_offline_id,
                               int expected_file_size);

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
           // Exclude flaky network.
           offline_page_header_.reason != OfflinePageHeader::Reason::NET_ERROR;
  }

 private:
  static std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
      SimpleFactoryKey* key);

  // TODO(crbug.com/40561648): The static members below will be removed
  // once the reference to BuildTestOfflinePageModel in SetUp is converted to a
  // base::OnceCallback.
  static base::FilePath private_archives_dir_;
  static base::FilePath public_archives_dir_;

  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  void OnGetPageByOfflineIdDone(const OfflinePageItem* pages);

  // Runs on IO thread.
  void CreateFileWithContentOnIO(const std::string& content,
                                 base::OnceClosure callback);

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  raw_ptr<OfflinePageTabHelper> offline_page_tab_helper_;  // Not owned.
  int64_t last_offline_id_;
  ResponseInfo response_;
  bool is_offline_page_set_in_navigation_data_;
  OfflinePageItem page_;
  OfflinePageHeader offline_page_header_;

#if BUILDFLAG(IS_ANDROID)
  // OfflinePageTabHelper instantiates PrefetchService which in turn requests a
  // fresh GCM token automatically. This causes the request to be done
  // synchronously instead of with a posted task.
  instance_id::InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting
      block_async_;
#endif  // BUILDFLAG(IS_ANDROID)

  // These are not thread-safe. But they can be used in the pattern that
  // setting the state is done first from one thread and reading this state
  // can be from any other thread later.
  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;

  // These should only be accessed purely from IO thread.
  base::ScopedTempDir private_archives_temp_base_dir_;
  base::ScopedTempDir public_archives_temp_base_dir_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  int file_name_sequence_num_ = 0;

  bool async_operation_completed_ = false;
  base::OnceClosure async_operation_completed_callback_;
  OfflinePageURLLoaderBuilder interceptor_factory_;
};

OfflinePageRequestHandlerTest::OfflinePageRequestHandlerTest()
    : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
      profile_manager_(TestingBrowserProcess::GetGlobal()),
      last_offline_id_(0),
      response_(net::ERR_IO_PENDING),
      is_offline_page_set_in_navigation_data_(false),
      network_change_notifier_(new TestNetworkChangeNotifier),
      interceptor_factory_(this) {}

void OfflinePageRequestHandlerTest::SetUp() {
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
      profile()->GetProfileKey(),
      base::BindRepeating(
          &OfflinePageRequestHandlerTest::BuildTestOfflinePageModel));

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

void OfflinePageRequestHandlerTest::TearDown() {
  EXPECT_TRUE(private_archives_temp_base_dir_.Delete());
  EXPECT_TRUE(public_archives_temp_base_dir_.Delete());
}

void OfflinePageRequestHandlerTest::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_outermost_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  interceptor_factory_.InterceptRequest(url, method, extra_headers,
                                        is_outermost_main_frame);
}

void OfflinePageRequestHandlerTest::SimulateHasNetworkConnectivity(
    bool online) {
  network_change_notifier_->set_online(online);
}

void OfflinePageRequestHandlerTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageRequestHandlerTest::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_) {
    return;
  }
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void OfflinePageRequestHandlerTest::CreateFileWithContentOnIO(
    const std::string& content,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  std::string file_name("test");
  file_name += base::NumberToString(file_name_sequence_num_++);
  file_name += ".mht";
  temp_file_path_ = temp_dir_.GetPath().AppendASCII(file_name);
  ASSERT_TRUE(base::WriteFile(temp_file_path_, content));
  std::move(callback).Run();
}

base::FilePath OfflinePageRequestHandlerTest::CreateFileWithContent(
    const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OfflinePageRequestHandlerTest::CreateFileWithContentOnIO,
                     base::Unretained(this), content, run_loop.QuitClosure()));
  run_loop.Run();
  return temp_file_path_;
}

void OfflinePageRequestHandlerTest::ExpectNoOfflinePageServed(
    int64_t offline_id) {
  EXPECT_NE("multipart/related", mime_type());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

void OfflinePageRequestHandlerTest::ExpectOfflinePageServed(
    int64_t expected_offline_id,
    int expected_file_size) {
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
}

std::string OfflinePageRequestHandlerTest::UseOfflinePageHeader(
    OfflinePageHeader::Reason reason,
    int64_t offline_id) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  offline_page_header_.reason = reason;
  if (offline_id) {
    offline_page_header_.id = base::NumberToString(offline_id);
  }
  return offline_page_header_.GetCompleteHeaderString();
}

std::string OfflinePageRequestHandlerTest::UseOfflinePageHeaderForIntent(
    OfflinePageHeader::Reason reason,
    int64_t offline_id,
    const GURL& intent_url) {
  DCHECK_NE(OfflinePageHeader::Reason::NONE, reason);
  DCHECK(offline_id);
  offline_page_header_.reason = reason;
  offline_page_header_.id = base::NumberToString(offline_id);
  offline_page_header_.intent_url = intent_url;
  return offline_page_header_.GetCompleteHeaderString();
}

int64_t OfflinePageRequestHandlerTest::SavePublicPage(
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

int64_t OfflinePageRequestHandlerTest::SaveInternalPage(
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

int64_t OfflinePageRequestHandlerTest::SavePage(const GURL& url,
                                                const GURL& original_url,
                                                const base::FilePath& file_path,
                                                int64_t file_size,
                                                const std::string& digest) {
  DCHECK(file_path.IsAbsolute());

  static int item_counter = 0;
  ++item_counter;

  auto archiver = std::make_unique<OfflinePageTestArchiver>(
      nullptr, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      std::u16string(), file_size, digest,
      base::SingleThreadTaskRunner::GetCurrentDefault());
  archiver->set_filename(file_path);

  async_operation_completed_ = false;
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id =
      ClientId(kDownloadNamespace, base::NumberToString(item_counter));
  save_page_params.original_url = original_url;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params, std::move(archiver), nullptr,
      base::BindOnce(&OfflinePageRequestHandlerTest::OnSavePageDone,
                     base::Unretained(this)));
  WaitForAsyncOperation();
  return last_offline_id_;
}

// static
std::unique_ptr<KeyedService>
OfflinePageRequestHandlerTest::BuildTestOfflinePageModel(
    SimpleFactoryKey* key) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  base::FilePath store_path =
      key->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStore(task_runner, store_path));

  // Since we're not saving page into temporary dir, it's set the same as the
  // private dir.
  auto archive_manager = std::make_unique<ArchiveManager>(
      private_archives_dir_, private_archives_dir_, public_archives_dir_,
      task_runner);

  auto archive_publisher = std::make_unique<OfflinePageTestArchivePublisher>(
      archive_manager.get(), kDownloadId);
  // TODO(iwells): Figure out how to make use_verbatim_archive_path go away.
  archive_publisher->use_verbatim_archive_path(true);

  return std::unique_ptr<KeyedService>(new OfflinePageModelTaskified(
      std::move(metadata_store), std::move(archive_manager),
      std::move(archive_publisher), task_runner));
}

// static
base::FilePath OfflinePageRequestHandlerTest::private_archives_dir_;
base::FilePath OfflinePageRequestHandlerTest::public_archives_dir_;

void OfflinePageRequestHandlerTest::OnSavePageDone(SavePageResult result,
                                                   int64_t offline_id) {
  ASSERT_EQ(SavePageResult::SUCCESS, result);
  last_offline_id_ = offline_id;

  async_operation_completed_ = true;
  if (!async_operation_completed_callback_.is_null()) {
    std::move(async_operation_completed_callback_).Run();
  }
}

OfflinePageItem OfflinePageRequestHandlerTest::GetPage(int64_t offline_id) {
  OfflinePageModelFactory::GetForBrowserContext(profile())->GetPageByOfflineId(
      offline_id,
      base::BindOnce(&OfflinePageRequestHandlerTest::OnGetPageByOfflineIdDone,
                     base::Unretained(this)));
  RunUntilIdle();
  return page_;
}

void OfflinePageRequestHandlerTest::OnGetPageByOfflineIdDone(
    const OfflinePageItem* page) {
  ASSERT_TRUE(page);
  page_ = *page;
}

void OfflinePageRequestHandlerTest::LoadPage(const GURL& url) {
  InterceptRequest(url, "GET", net::HttpRequestHeaders(),
                   true /* is_outermost_main_frame */);
}

void OfflinePageRequestHandlerTest::LoadPageWithHeaders(
    const GURL& url,
    const net::HttpRequestHeaders& extra_headers) {
  InterceptRequest(url, "GET", extra_headers,
                   true /* is_outermost_main_frame */);
}

void OfflinePageRequestHandlerTest::ReadCompleted(
    const ResponseInfo& response,
    bool is_offline_page_set_in_navigation_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  response_ = response;
  is_offline_page_set_in_navigation_data_ =
      is_offline_page_set_in_navigation_data;

  interceptor_factory_.Quit();
}

OfflinePageURLLoaderBuilder::OfflinePageURLLoaderBuilder(
    OfflinePageRequestHandlerTest* test)
    : test_(test) {
  navigation_ui_data_ = std::make_unique<ChromeNavigationUIData>();
}

void OfflinePageURLLoaderBuilder::OnReceiveRedirect(
    const GURL& redirected_url) {
  InterceptRequestInternal(redirected_url, "GET", net::HttpRequestHeaders(),
                           true);
}

void OfflinePageURLLoaderBuilder::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  mime_type_ = response_head->mime_type;
  ReadBody();
}

void OfflinePageURLLoaderBuilder::OnComplete() {
  if (client_->completion_status().error_code != net::OK) {
    mime_type_.clear();
    body_.clear();
  }
  ReadCompleted(
      ResponseInfo(client_->completion_status().error_code, mime_type_, body_));
  // Clear intermediate data in preparation for next potential page loading.
  mime_type_.clear();
  body_.clear();
}

void OfflinePageURLLoaderBuilder::InterceptRequestInternal(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_outermost_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  client_ = std::make_unique<TestURLLoaderClient>(this);

  network::ResourceRequest request = CreateResourceRequest(
      url, method, extra_headers, is_outermost_main_frame);

  url_loader_ = OfflinePageURLLoader::Create(
      navigation_ui_data_.get(),
      test_->web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
      request,
      base::BindOnce(&OfflinePageURLLoaderBuilder::MaybeStartLoader,
                     base::Unretained(this), request));

  // |url_loader_| may not be created.
  if (!url_loader_) {
    return;
  }

  url_loader_->SetTabIdGetterForTesting(base::BindRepeating(&GetTabId, kTabId));
}

void OfflinePageURLLoaderBuilder::InterceptRequest(
    const GURL& url,
    const std::string& method,
    const net::HttpRequestHeaders& extra_headers,
    bool is_outermost_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  InterceptRequestInternal(url, method, extra_headers, is_outermost_main_frame);
  loop.Run();
}

void OfflinePageURLLoaderBuilder::MaybeStartLoader(
    const network::ResourceRequest& request,
    content::URLLoaderRequestInterceptor::RequestHandler request_handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!request_handler) {
    ReadCompleted(ResponseInfo(net::ERR_FAILED));
    return;
  }

  // OfflinePageURLLoader decides to handle the request as offline page. Since
  // now, OfflinePageURLLoader will own itself and live as long as its URLLoader
  // and URLLoaderClient are alive.
  url_loader_.release();

  loader_.reset();
  std::move(request_handler)
      .Run(request, loader_.BindNewPipeAndPassReceiver(),
           client_->CreateRemote());
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
          base::SequencedTaskRunner::GetCurrentDefault());
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
      ReadCompleted(ResponseInfo(net::ERR_FAILED));
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
    ReadCompleted(ResponseInfo(net::ERR_FAILED));
    return;
  }
  ReadBody();
}

void OfflinePageURLLoaderBuilder::ReadCompleted(const ResponseInfo& response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  handle_watcher_.reset();
  client_.reset();
  url_loader_.reset();
  loader_.reset();

  bool is_offline_page_set_in_navigation_data = false;
  offline_pages::OfflinePageNavigationUIData* offline_page_data =
      navigation_ui_data_->GetOfflinePageNavigationUIData();
  if (offline_page_data && offline_page_data->is_offline_page()) {
    is_offline_page_set_in_navigation_data = true;
  }

  test()->ReadCompleted(response, is_offline_page_set_in_navigation_data);
}

TEST_F(OfflinePageRequestHandlerTest, FailedToCreateRequestJob) {
  SimulateHasNetworkConnectivity(false);

  // Must be http/https URL.
  InterceptRequest(GURL("ftp://host/doc"), "GET", net::HttpRequestHeaders(),
                   true /* is_outermost_main_frame */);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(GURL("file:///path/doc"), "GET", net::HttpRequestHeaders(),
                   true /* is_outermost_main_frame */);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be GET method.
  InterceptRequest(GURL(kTestUrl), "POST", net::HttpRequestHeaders(),
                   true /* is_outermost_main_frame */);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  InterceptRequest(GURL(kTestUrl), "HEAD", net::HttpRequestHeaders(),
                   true /* is_outermost_main_frame */);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());

  // Must be main resource.
  InterceptRequest(GURL(kTestUrl), "POST", net::HttpRequestHeaders(),
                   false /* is_outermost_main_frame */);
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestHandlerTest, LoadOfflinePageOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id, kFileSize1);
}

TEST_F(OfflinePageRequestHandlerTest,
       DoNotLoadOfflinePageOnDisconnectedNetworkWhenNetworkStateLikelyUnknown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      offline_pages::kOfflinePagesNetworkStateLikelyUnknown);

  this->SimulateHasNetworkConnectivity(false);

  const GURL test_url(kTestUrl);
  int64_t offline_id = this->SaveInternalPage(test_url, GURL(), kFilename1,
                                              kFileSize1, std::string());

  this->LoadPage(test_url);

  // When the network is good, we will fall back to the default handling
  // immediately. So no request result should be reported. Passing
  // AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  this->ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, PageNotFoundOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  int64_t offline_id = SaveInternalPage(GURL(kTestUrl), GURL(), kFilename1,
                                        kFileSize1, std::string());

  LoadPage(GURL(kTestUrl2));

  ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest,
       NetErrorPageSuggestionOnDisconnectedNetwork) {
  SimulateHasNetworkConnectivity(false);

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR_SUGGESTION, 0));
  LoadPageWithHeaders(test_url, extra_headers);

  ExpectOfflinePageServed(offline_id, kFileSize1);
}

TEST_F(OfflinePageRequestHandlerTest, LoadOfflinePageOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(test_url, extra_headers);

  ExpectOfflinePageServed(offline_id, kFileSize1);
}

TEST_F(OfflinePageRequestHandlerTest, PageNotFoundOnFlakyNetwork) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id = SaveInternalPage(GURL(kTestUrl), GURL(), kFilename1,
                                        kFileSize1, std::string());

  // When custom offline header exists and contains "reason=error", it means
  // that net error is hit in last request due to flaky network.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::NET_ERROR, 0));
  LoadPageWithHeaders(GURL(kTestUrl2), extra_headers);

  ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, ForceLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(test_url, extra_headers);

  ExpectOfflinePageServed(offline_id, kFileSize1);
}

TEST_F(OfflinePageRequestHandlerTest, PageNotFoundOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  // Save an offline page.
  int64_t offline_id = SaveInternalPage(GURL(kTestUrl), GURL(), kFilename1,
                                        kFileSize1, std::string());

  // When custom offline header exists and contains value other than
  // "reason=error", it means that offline page is forced to load.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, 0));
  LoadPageWithHeaders(GURL(kTestUrl2), extra_headers);

  ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, DoNotLoadOfflinePageOnConnectedNetwork) {
  SimulateHasNetworkConnectivity(true);

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  LoadPage(test_url);

  // When the network is good, we will fall back to the default handling
  // immediately. So no request result should be reported. Passing
  // AGGREGATED_REQUEST_RESULT_MAX to skip checking request result in
  // the helper function.
  ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, LoadMostRecentlyCreatedOfflinePage) {
  SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  const GURL test_url(kTestUrl);
  int64_t offline_id2 =
      SaveInternalPage(test_url, GURL(), kFilename2, kFileSize2, std::string());

  // Load an URL that matches multiple offline pages. Expect that the most
  // recently created offline page is fetched.
  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id2, kFileSize2);
}

TEST_F(OfflinePageRequestHandlerTest, LoadOfflinePageByOfflineID) {
  SimulateHasNetworkConnectivity(true);

  // Save 2 offline pages associated with same online URL, but pointing to
  // different archive file.
  const GURL test_url(kTestUrl);
  int64_t offline_id1 =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  // Load an URL with a specific offline ID designated in the custom header.
  // Expect the offline page matching the offline id is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, offline_id1));
  LoadPageWithHeaders(test_url, extra_headers);

  ExpectOfflinePageServed(offline_id1, kFileSize1);
}

TEST_F(OfflinePageRequestHandlerTest, FailToLoadByOfflineIDOnUrlMismatch) {
  SimulateHasNetworkConnectivity(true);

  int64_t offline_id = SaveInternalPage(GURL(kTestUrl), GURL(), kFilename1,
                                        kFileSize1, std::string());

  // The offline page found with specific offline ID does not match the passed
  // online URL. Should fall back to find the offline page based on the online
  // URL.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(
      UseOfflinePageHeader(OfflinePageHeader::Reason::DOWNLOAD, offline_id));
  LoadPageWithHeaders(GURL(kTestUrl2), extra_headers);

  ExpectNoOfflinePageServed(offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, LoadOfflinePageForUrlWithFragment) {
  SimulateHasNetworkConnectivity(false);

  // Save an offline page associated with online URL without fragment.
  const GURL test_url(kTestUrl);
  int64_t offline_id1 =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  // Save another offline page associated with online URL that has a fragment.
  const GURL test_url2(kTestUrl2);
  GURL url2_with_fragment(test_url2.spec() + "#ref");
  int64_t offline_id2 = SaveInternalPage(url2_with_fragment, GURL(), kFilename2,
                                         kFileSize2, std::string());

  // Loads an url with fragment, that will match the offline URL without the
  // fragment.
  GURL url_with_fragment(test_url.spec() + "#ref");
  LoadPage(url_with_fragment);

  ExpectOfflinePageServed(offline_id1, kFileSize1);

  // Loads an url without fragment, that will match the offline URL with the
  // fragment.
  LoadPage(test_url2);

  EXPECT_EQ(kFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);

  // Loads an url with fragment, that will match the offline URL with different
  // fragment.
  GURL url2_with_different_fragment(test_url2.spec() + "#different_ref");
  LoadPage(url2_with_different_fragment);

  EXPECT_EQ(kFileSize2, bytes_read());
  ASSERT_TRUE(offline_page_tab_helper()->GetOfflinePageForTest());
  EXPECT_EQ(offline_id2,
            offline_page_tab_helper()->GetOfflinePageForTest()->offline_id);
}

TEST_F(OfflinePageRequestHandlerTest, LoadOtherPageOnDigestMismatch) {
  SimulateHasNetworkConnectivity(false);

  // Save 2 offline pages associated with same online URL, one in internal
  // location, while another in public location with mismatched digest.
  const GURL test_url(kTestUrl);
  int64_t offline_id1 =
      SaveInternalPage(test_url, GURL(), kFilename1, kFileSize1, std::string());

  // There are 2 offline pages matching |test_url|. The most recently created
  // one should fail on mistmatched digest. The second most recently created
  // offline page should work.
  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id1, kFileSize1);
}

// Disabled due to https://crbug.com/917113.
TEST_F(OfflinePageRequestHandlerTest, DISABLED_EmptyFile) {
  SimulateHasNetworkConnectivity(false);

  const std::string expected_data("");
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  const std::string expected_digest = archive_validator.Finish();

  const GURL test_url(kTestUrl);
  int64_t offline_id =
      SavePublicPage(test_url, GURL(), temp_file_path, 0, expected_digest);

  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id, 0);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestHandlerTest, TinyFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data("hello world");
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id, expected_size);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestHandlerTest, SmallFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id, expected_size);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestHandlerTest, BigFile) {
  SimulateHasNetworkConnectivity(false);

  std::string expected_data(MakeContentOfSize(3 * 1024 * 1024));
  base::FilePath temp_file_path = CreateFileWithContent(expected_data);
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), temp_file_path,
                                      expected_size, expected_digest);

  LoadPage(test_url);

  ExpectOfflinePageServed(offline_id, expected_size);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestHandlerTest, LoadFromFileUrlIntent) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. The path to this file will be feed
  // into "intent_url" of extra headers.
  base::FilePath unmodified_file_path = CreateFileWithContent(expected_data);

  // Create a file with modified data. An offline page is created to associate
  // with this file, but with size and digest matching the unmodified version.
  std::string modified_data(expected_data);
  modified_data[10] = '@';
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // unmodified file. Expect the file from the intent URL is fetched.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(unmodified_file_path)));
  LoadPageWithHeaders(test_url, extra_headers);

  ExpectOfflinePageServed(offline_id, expected_size);
  EXPECT_EQ(expected_data, data_received());
}

TEST_F(OfflinePageRequestHandlerTest, IntentFileNotFound) {
  SimulateHasNetworkConnectivity(true);

  std::string expected_data(MakeContentOfSize(2 * 1024));
  ArchiveValidator archive_validator;
  archive_validator.Update(expected_data.c_str(), expected_data.length());
  std::string expected_digest = archive_validator.Finish();
  int expected_size = expected_data.length();

  // Create a file with unmodified data. An offline page is created to associate
  // with this file.
  base::FilePath unmodified_file_path = CreateFileWithContent(expected_data);

  // Get a path pointing to non-existing file. This path will be feed into
  // "intent_url" of extra headers.
  base::FilePath nonexistent_file_path =
      unmodified_file_path.DirName().AppendASCII("nonexistent");

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), unmodified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // non-existent file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(nonexistent_file_path)));
  LoadPageWithHeaders(test_url, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_NE("multipart/related", mime_type());
  EXPECT_EQ(0, bytes_read());
  EXPECT_FALSE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestHandlerTest, IntentFileModifiedInTheMiddle) {
  SimulateHasNetworkConnectivity(true);

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
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  LoadPageWithHeaders(test_url, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_NE("multipart/related", mime_type());
  EXPECT_EQ(0, bytes_read());
  // Note that the offline bit is not cleared on purpose due to the fact that
  // other flag, like request status, should already indicate that the offline
  // page fails to load.
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

TEST_F(OfflinePageRequestHandlerTest, IntentFileModifiedWithMoreDataAppended) {
  SimulateHasNetworkConnectivity(true);

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
  base::FilePath modified_file_path = CreateFileWithContent(modified_data);

  const GURL test_url(kTestUrl);
  int64_t offline_id = SavePublicPage(test_url, GURL(), modified_file_path,
                                      expected_size, expected_digest);

  // Load an URL with custom header that contains "intent_url" pointing to
  // modified file. Expect the request fails.
  net::HttpRequestHeaders extra_headers;
  extra_headers.AddHeaderFromString(UseOfflinePageHeaderForIntent(
      OfflinePageHeader::Reason::FILE_URL_INTENT, offline_id,
      net::FilePathToFileURL(modified_file_path)));
  LoadPageWithHeaders(test_url, extra_headers);

  EXPECT_EQ(net::ERR_FAILED, request_status());
  EXPECT_NE("multipart/related", mime_type());
  EXPECT_EQ(0, bytes_read());
  // Note that the offline bit is not cleared on purpose due to the fact that
  // other flag, like request status, should already indicate that the offline
  // page fails to load.
  EXPECT_TRUE(is_offline_page_set_in_navigation_data());
  EXPECT_FALSE(offline_page_tab_helper()->GetOfflinePageForTest());
}

}  // namespace offline_pages
