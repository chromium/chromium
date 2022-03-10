// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <memory>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("chrome/test/data");

// URL of mock WebAPK server.
const char* kServerUrl = "/webapkserver/";

// Start URL for the WebAPK
const char* kStartUrl = "/index.html";

// The URLs of best icons from Web Manifest. We use a random file in the test
// data directory. Since WebApkInstaller does not try to decode the file as an
// image it is OK that the file is not an image.
const char* kBestPrimaryIconUrl = "/simple.html";
const char* kBestSplashIconUrl = "/nostore.html";
const char* kBestShortcutIconUrl = "/title1.html";

// Icon which has Cross-Origin-Resource-Policy: same-origin set.
const char* kBestPrimaryIconCorpUrl = "/banners/image-512px-corp.png";

// Timeout for getting response from WebAPK server.
const int kWebApkServerRequestTimeoutMs = 1000;

// Token from the WebAPK server. In production, the token is sent to Google
// Play. Google Play uses the token to retrieve the WebAPK from the WebAPK
// server.
const char* kToken = "token";

// The package name of the downloaded WebAPK.
const char* kDownloadedWebApkPackageName = "party.unicode";

const char* kUnusedIconPath = "https://example.com/unused_icon.png";

// WebApkInstaller subclass where
// WebApkInstaller::StartInstallingDownloadedWebApk() and
// WebApkInstaller::StartUpdateUsingDownloadedWebApk() and
// WebApkInstaller::CanUseGooglePlayInstallService() and
// WebApkInstaller::InstallOrUpdateWebApkFromGooglePlay() are stubbed out.
class TestWebApkInstaller : public WebApkInstaller {
 public:
  explicit TestWebApkInstaller(content::BrowserContext* browser_context,
                               SpaceStatus status)
      : WebApkInstaller(browser_context), test_space_status_(status) {}

  TestWebApkInstaller(const TestWebApkInstaller&) = delete;
  TestWebApkInstaller& operator=(const TestWebApkInstaller&) = delete;

  void InstallOrUpdateWebApk(const std::string& package_name,
                             const std::string& token) override {
    PostTaskToRunSuccessCallback();
  }

  void PostTaskToRunSuccessCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestWebApkInstaller::OnResult, base::Unretained(this),
                       WebApkInstallResult::SUCCESS));
  }

 private:
  void CheckFreeSpace() override {
    OnGotSpaceStatus(nullptr, base::android::JavaParamRef<jobject>(nullptr),
                     static_cast<int>(test_space_status_));
  }

  // The space status used in tests.
  SpaceStatus test_space_status_;
};

// Runs the WebApkInstaller installation process/update and blocks till done.
class WebApkInstallerRunner {
 public:
  WebApkInstallerRunner() {}

  WebApkInstallerRunner(const WebApkInstallerRunner&) = delete;
  WebApkInstallerRunner& operator=(const WebApkInstallerRunner&) = delete;

  ~WebApkInstallerRunner() {}

  void RunInstallWebApk(std::unique_ptr<WebApkInstaller> installer,
                        content::WebContents* web_contents,
                        const webapps::ShortcutInfo& info) {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();

    // WebApkInstaller owns itself.
    WebApkInstaller::InstallAsyncForTesting(
        installer.release(), web_contents, info, SkBitmap(), false,
        base::BindOnce(&WebApkInstallerRunner::OnCompleted,
                       base::Unretained(this)));

    run_loop.Run();
  }

  void RunInstallForService(std::unique_ptr<WebApkInstaller> installer,
                            std::unique_ptr<std::string> serialized_webapk,
                            const std::u16string& short_name,
                            webapps::ShortcutInfo::Source source) {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();

    GURL manifest_url("httsp://manifest.com");

    // WebApkInstaller owns itself.
    WebApkInstaller::InstallForServiceAsyncForTesting(
        installer.release(), std::move(serialized_webapk), short_name, source,
        SkBitmap(), false, manifest_url,
        base::BindOnce(&WebApkInstallerRunner::OnCompleted,
                       base::Unretained(this)));

    run_loop.Run();
  }

  void RunUpdateWebApk(std::unique_ptr<WebApkInstaller> installer,
                       const base::FilePath& update_request_path) {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();

    // WebApkInstaller owns itself.
    WebApkInstaller::UpdateAsyncForTesting(
        installer.release(), update_request_path,
        base::BindOnce(&WebApkInstallerRunner::OnCompleted,
                       base::Unretained(this)));

    run_loop.Run();
  }

  WebApkInstallResult result() { return result_; }

 private:
  void OnCompleted(WebApkInstallResult result,
                   bool relax_updates,
                   const std::string& webapk_package) {
    result_ = result;
    std::move(on_completed_callback_).Run();
  }

  // Called after the installation process has succeeded or failed.
  base::OnceClosure on_completed_callback_;

  // The result of the installation process.
  WebApkInstallResult result_;
};

// Helper class for calling WebApkInstaller::StoreUpdateRequestToFile()
// synchronously.
class UpdateRequestStorer {
 public:
  UpdateRequestStorer() {}

  UpdateRequestStorer(const UpdateRequestStorer&) = delete;
  UpdateRequestStorer& operator=(const UpdateRequestStorer&) = delete;

  void StoreSync(const base::FilePath& update_request_path) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    WebApkInstaller::StoreUpdateRequestToFile(
        update_request_path, webapps::ShortcutInfo((GURL())), "", false, "", "",
        "", std::map<std::string, webapps::WebApkIconHasher::Icon>(), false,
        false, {webapps::WebApkUpdateReason::PRIMARY_ICON_HASH_DIFFERS},
        base::BindOnce(&UpdateRequestStorer::OnComplete,
                       base::Unretained(this)));
    run_loop.Run();
  }

 private:
  void OnComplete(bool success) { std::move(quit_closure_).Run(); }

  base::OnceClosure quit_closure_;
};

// Builds a webapk::WebApkResponse with |token| as the token from the WebAPK
// server.
std::unique_ptr<net::test_server::HttpResponse> BuildValidWebApkResponse(
    const std::string& token) {
  std::unique_ptr<webapk::WebApkResponse> response_proto(
      new webapk::WebApkResponse);
  response_proto->set_package_name(kDownloadedWebApkPackageName);
  response_proto->set_token(token);
  std::string response_content;
  response_proto->SerializeToString(&response_content);

  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content(response_content);
  return std::move(response);
}

// Builds WebApk proto and blocks till done.
class BuildProtoRunner {
 public:
  BuildProtoRunner() {}

  BuildProtoRunner(const BuildProtoRunner&) = delete;
  BuildProtoRunner& operator=(const BuildProtoRunner&) = delete;

  ~BuildProtoRunner() {}

  void BuildSync(const GURL& best_primary_icon_url,
                 const GURL& splash_image_url,
                 std::map<std::string, webapps::WebApkIconHasher::Icon>
                     icon_url_to_murmur2_hash,
                 const std::string& primary_icon_data,
                 const std::string& splash_icon_data,
                 bool is_manifest_stale,
                 bool is_app_identity_update_supported,
                 const std::vector<GURL>& best_shortcut_icon_urls) {
    webapps::ShortcutInfo info(GURL::EmptyGURL());
    info.best_primary_icon_url = best_primary_icon_url;
    info.splash_image_url = splash_image_url;
    if (!best_primary_icon_url.is_empty())
      info.icon_urls.push_back(best_primary_icon_url.spec());
    if (!splash_image_url.is_empty())
      info.icon_urls.push_back(splash_image_url.spec());
    info.icon_urls.push_back(kUnusedIconPath);

    for (const GURL& shortcut_url : best_shortcut_icon_urls) {
      info.best_shortcut_icon_urls.push_back(shortcut_url);
      info.shortcut_items.emplace_back();
      info.shortcut_items.back().icons.emplace_back();
      info.shortcut_items.back().icons.back().src = shortcut_url;
    }

    WebApkInstaller::BuildProto(
        info, primary_icon_data, false /* is_primary_icon_maskable */,
        splash_icon_data, "" /* package_name */, "" /* version */,
        std::move(icon_url_to_murmur2_hash), is_manifest_stale,
        is_app_identity_update_supported,
        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  webapk::WebApk* GetWebApkRequest() { return webapk_request_.get(); }

  std::unique_ptr<std::string> TakeSerializedWebApk() {
    return std::move(serialized_webapk_);
  }

 private:
  // Called when the |webapk_request_| is populated.
  void OnBuiltWebApkProto(std::unique_ptr<std::string> serialized_proto) {
    webapk_request_ = std::make_unique<webapk::WebApk>();
    webapk_request_->ParseFromString(*serialized_proto);
    serialized_webapk_ = std::move(serialized_proto);
    std::move(on_completed_callback_).Run();
  }

  // The populated webapk::WebApk.
  std::unique_ptr<webapk::WebApk> webapk_request_;

  // The serialized webapk::WebApk.
  std::unique_ptr<std::string> serialized_webapk_;

  // Called after the |webapk_request_| is built.
  base::OnceClosure on_completed_callback_;
};

class ScopedTempFile {
 public:
  ScopedTempFile() { CHECK(base::CreateTemporaryFile(&file_path_)); }

  ScopedTempFile(const ScopedTempFile&) = delete;
  ScopedTempFile& operator=(const ScopedTempFile&) = delete;

  ~ScopedTempFile() { base::DeleteFile(file_path_); }

  const base::FilePath& GetFilePath() { return file_path_; }

 private:
  base::FilePath file_path_;
};

}  // anonymous namespace

class WebApkInstallerTest : public ::testing::Test {
 public:
  typedef base::RepeatingCallback<
      std::unique_ptr<net::test_server::HttpResponse>(void)>
      WebApkResponseBuilder;

  WebApkInstallerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  WebApkInstallerTest(const WebApkInstallerTest&) = delete;
  WebApkInstallerTest& operator=(const WebApkInstallerTest&) = delete;

  ~WebApkInstallerTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebApkInstallerTest::HandleWebApkRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);

    SetDefaults();
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<WebApkInstaller> CreateDefaultWebApkInstaller() {
    auto installer = std::unique_ptr<WebApkInstaller>(
        new TestWebApkInstaller(&profile_, SpaceStatus::ENOUGH_SPACE));
    installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
    return installer;
  }

  webapps::ShortcutInfo DefaultShortcutInfo() {
    webapps::ShortcutInfo info(test_server_.GetURL(kStartUrl));
    info.best_primary_icon_url = test_server_.GetURL(kBestPrimaryIconUrl);
    info.splash_image_url = test_server_.GetURL(kBestSplashIconUrl);
    info.best_shortcut_icon_urls.push_back(
        test_server_.GetURL(kBestShortcutIconUrl));
    return info;
  }

  std::unique_ptr<std::string> DefaultSerializedWebApk() {
    std::string icon_url_1 = test_server()->GetURL("/icon1.png").spec();
    std::string icon_url_2 = test_server()->GetURL("/icon2.png").spec();
    std::map<std::string, webapps::WebApkIconHasher::Icon>
        icon_url_to_murmur2_hash;
    icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
    icon_url_to_murmur2_hash[icon_url_2] = {"data2", "2"};

    std::string primary_icon_data = "data3";
    std::string splash_icon_data = "data4";

    std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
    runner->BuildSync(GURL(), GURL(), std::move(icon_url_to_murmur2_hash),
                      primary_icon_data, splash_icon_data,
                      true /* is_manifest_stale */,
                      true /* is_app_identity_update_supported */, {});

    return runner->TakeSerializedWebApk();
  }

  // Sets the URL to send the webapk::CreateWebApkRequest to. WebApkInstaller
  // should fail if the URL is not |kServerUrl|.
  void SetWebApkServerUrl(const GURL& server_url) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());
  }

  // Sets the function that should be used to build the response to the
  // WebAPK creation request.
  void SetWebApkResponseBuilder(WebApkResponseBuilder builder) {
    webapk_response_builder_ = builder;
  }

  std::unique_ptr<BuildProtoRunner> CreateBuildProtoRunner() {
    return std::make_unique<BuildProtoRunner>();
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }
  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    SetWebApkServerUrl(test_server_.GetURL(kServerUrl));
    SetWebApkResponseBuilder(
        base::BindRepeating(&BuildValidWebApkResponse, kToken));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request) {
    return (request.relative_url == kServerUrl)
               ? webapk_response_builder_.Run()
               : std::unique_ptr<net::test_server::HttpResponse>();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  net::EmbeddedTestServer test_server_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.

  // Builds response to the WebAPK creation request.
  WebApkResponseBuilder webapk_response_builder_;
};

// Test installation succeeding.
TEST_F(WebApkInstallerTest, Success) {
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if there is not enough space on device.
TEST_F(WebApkInstallerTest, FailOnLowSpace) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::NOT_ENOUGH_SPACE));
  installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation succeeds when the primary icon is guarded by
// a Cross-Origin-Resource-Policy: same-origin header and the icon is
// same-origin with the start URL.
TEST_F(WebApkInstallerTest, CrossOriginResourcePolicySameOriginIconSuccess) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconCorpUrl);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if fetching the bitmap at the best primary icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
TEST_F(WebApkInstallerTest, BestPrimaryIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url = test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation fails if fetching the bitmap at the best splash icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
TEST_F(WebApkInstallerTest, BestSplashIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.splash_image_url = test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation fails if the WebAPK creation request times out.
TEST_F(WebApkInstallerTest, CreateWebApkRequestTimesOut) {
  SetWebApkServerUrl(test_server()->GetURL("/slow?1000"));
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));
  installer->SetTimeoutMs(100);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// InstallForService tests

// Test installation for service succeeding
TEST_F(WebApkInstallerTest, ServiceSuccess) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));

  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();

  WebApkInstallerRunner runner;
  runner.RunInstallForService(std::move(installer), DefaultSerializedWebApk(),
                              shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test installation for service failing if not enough space
TEST_F(WebApkInstallerTest, ServiceFailOnLowSpace) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::NOT_ENOUGH_SPACE));

  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();

  WebApkInstallerRunner runner;
  runner.RunInstallForService(std::move(installer), DefaultSerializedWebApk(),
                              shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test installation for service failing if serialized apk invalid.
TEST_F(WebApkInstallerTest, ServiceFailOnInvalidSerializedWebApk) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));

  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  std::string invalid_serialized_webapk = "😀";

  WebApkInstallerRunner runner;
  runner.RunInstallForService(
      std::move(installer),
      std::make_unique<std::string>(invalid_serialized_webapk),
      shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

namespace {

// Returns an HttpResponse which cannot be parsed as a webapk::WebApkResponse.
std::unique_ptr<net::test_server::HttpResponse>
BuildUnparsableWebApkResponse() {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_code(net::HTTP_OK);
  response->set_content("😀");
  return std::move(response);
}

}  // anonymous namespace

// Test that an HTTP response which cannot be parsed as a webapk::WebApkResponse
// is handled properly.
TEST_F(WebApkInstallerTest, UnparsableCreateWebApkResponse) {
  SetWebApkResponseBuilder(base::BindRepeating(&BuildUnparsableWebApkResponse));

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test update succeeding.
TEST_F(WebApkInstallerTest, UpdateSuccess) {
  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  UpdateRequestStorer().StoreSync(update_request_path);
  ASSERT_TRUE(base::PathExists(update_request_path));

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that an update suceeds if the WebAPK server returns a HTTP response with
// an empty token. The WebAPK server sends an empty token when:
// - The server is unable to update the WebAPK in the way that the client
//   requested.
// AND
// - The most up to date version of the WebAPK on the server is identical to the
//   one installed on the client.
TEST_F(WebApkInstallerTest, UpdateSuccessWithEmptyTokenInResponse) {
  SetWebApkResponseBuilder(base::BindRepeating(&BuildValidWebApkResponse, ""));

  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  UpdateRequestStorer().StoreSync(update_request_path);
  ASSERT_TRUE(base::PathExists(update_request_path));
  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that an update fails if the "update request path" points to an update
// file with the incorrect format.
TEST_F(WebApkInstallerTest, UpdateFailsUpdateRequestWrongFormat) {
  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  base::WriteFile(update_request_path, "😀", 1);

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that an update fails if the "update request path" points to a
// non-existing file.
TEST_F(WebApkInstallerTest, UpdateFailsUpdateRequestFileDoesNotExist) {
  base::FilePath update_request_path;
  {
    ScopedTempFile scoped_file;
    update_request_path = scoped_file.GetFilePath();
  }
  ASSERT_FALSE(base::PathExists(update_request_path));

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that StoreUpdateRequestToFile() creates directories if needed when
// writing to the passed in |update_file_path|.
TEST_F(WebApkInstallerTest, StoreUpdateRequestToFileCreatesDirectories) {
  base::FilePath outer_file_path;
  ASSERT_TRUE(CreateNewTempDirectory("", &outer_file_path));
  base::FilePath update_request_path =
      outer_file_path.Append("deep").Append("deeper");
  UpdateRequestStorer().StoreSync(update_request_path);
  EXPECT_TRUE(base::PathExists(update_request_path));

  // Clean up
  base::DeletePathRecursively(outer_file_path);
}

// When there is no Web Manifest available for a site, an empty
// |best_primary_icon_url| and an empty |splash_image_url| is used to build a
// WebApk update request. Tests the request can be built properly.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsObsolete) {
  std::string icon_url_1 = test_server()->GetURL("/icon1.png").spec();
  std::string icon_url_2 = test_server()->GetURL("/icon2.png").spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[icon_url_2] = {"data2", "2"};

  std::string primary_icon_data = "data3";
  std::string splash_icon_data = "data4";
  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(), std::move(icon_url_to_murmur2_hash),
                    primary_icon_data, splash_icon_data,
                    true /* is_manifest_stale */,
                    true /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  EXPECT_EQ("", manifest.icons(0).src());
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(0).image_data(), primary_icon_data);

  EXPECT_EQ("", manifest.icons(1).src());
  EXPECT_FALSE(manifest.icons(1).has_hash());
  EXPECT_EQ(manifest.icons(1).image_data(), splash_icon_data);

  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsAvailable) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::string best_splash_icon_url =
      test_server()->GetURL(kBestSplashIconUrl).spec();
  std::string best_shortcut_icon_url =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data0", "0"};
  icon_url_to_murmur2_hash[best_primary_icon_url] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_splash_icon_url] = {"data2", "2"};
  icon_url_to_murmur2_hash[best_shortcut_icon_url] = {"data3", "3"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(best_primary_icon_url), GURL(best_splash_icon_url),
                    icon_url_to_murmur2_hash, "" /* primary_icon_data */,
                    "" /* splash_icon_data */, false /* is_manifest_stale*/,
                    false /* is_app_identity_update_supported */,
                    {GURL(best_shortcut_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for kBestPrimaryIconUrl.
  EXPECT_EQ(best_primary_icon_url, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[best_primary_icon_url].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[best_primary_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check protobuf fields for kBestSplashIconUrl.
  EXPECT_EQ(best_splash_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_splash_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_splash_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].unsafe_data);
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest, and
// primary icon and splash icon share the same URL.
TEST_F(WebApkInstallerTest, BuildWebApkProtoPrimaryIconAndSplashIconSameUrl) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_icon_url] = {"data0", "0"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(best_icon_url), GURL(best_icon_url),
                    icon_url_to_murmur2_hash, "" /* primary_icon_data */,
                    "" /* splash_icon_data */, false /* is_manifest_stale*/,
                    false /* is_app_identity_update_supported */,
                    {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for icons.
  EXPECT_EQ(best_icon_url, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON,
                                   webapk::Image::SPLASH_ICON));

  EXPECT_EQ(best_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON,
                                   webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
}

TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenWithMultipleShortcuts) {
  std::string best_shortcut_icon_url1 =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::string best_shortcut_icon_url2 =
      test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[best_shortcut_icon_url1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_shortcut_icon_url2] = {"data2", "2"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), icon_url_to_murmur2_hash, "" /* primary_icon_data */,
      "" /* splash_icon_data */, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */,
      {GURL(best_shortcut_icon_url1), GURL(best_shortcut_icon_url2)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(manifest.shortcuts_size(), 2);

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url1].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url1].unsafe_data);

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url2);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url2].hash);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url2].unsafe_data);
}

TEST_F(WebApkInstallerTest,
       BuildWebApkProtoWhenWithMultipleShortcutsAndSameIcons) {
  std::string best_shortcut_icon_url =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[best_shortcut_icon_url] = {"data1", "1"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), icon_url_to_murmur2_hash, "" /* primary_icon_data */,
      "" /* splash_icon_data */, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */,
      {GURL(best_shortcut_icon_url), GURL(best_shortcut_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(manifest.shortcuts_size(), 2);

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].unsafe_data);

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  // This is a duplicate icon, so the data won't be included again.
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(), "");
}

TEST_F(WebApkInstallerTest, BuildWebApkProtoSplashIconAndShortcutIconSameUrl) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_icon_url] = {"data0", "0"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(icon_url_1), GURL(best_icon_url),
                    icon_url_to_murmur2_hash, "" /* primary_icon_data */,
                    "" /* splash_icon_data */, false /* is_manifest_stale*/,
                    true /* is_app_identity_update_supported */,
                    {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());
  ASSERT_EQ(manifest.shortcuts_size(), 1);

  // Check primary icon fields.
  EXPECT_EQ(icon_url_1, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[icon_url_1].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[icon_url_1].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check splash icon fields
  EXPECT_EQ(best_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_TRUE(manifest.shortcuts(0).icons(0).has_image_data());
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
}
