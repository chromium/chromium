// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("chrome/test/data");

// URL of mock WebAPK server.
const char* kServerUrl = "/webapkserver/";

// Start URL for the WebAPK
const char* kStartUrl = "/index.html";

const char* kBestPrimaryIconUrl = "/banners/128x128-green.png";
const char* kBestSplashIconUrl = "/banners/128x128-red.png";
const char* kBestShortcutIconUrl = "/banners/96x96-red.png";

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

// WebApkInstaller subclass where
// WebApkInstaller::CheckFreeSpace() and
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestWebApkInstaller::OnResult, base::Unretained(this),
                       webapps::WebApkInstallResult::SUCCESS));
  }

 private:
  void CheckFreeSpace() override {
    OnGotSpaceStatus(nullptr, static_cast<int>(test_space_status_));
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
        installer.release(), web_contents, info, SkBitmap(),
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
    WebApkInstaller::InstallWithProtoAsyncForTesting(
        installer.release(), std::move(serialized_webapk), short_name, source,
        SkBitmap(), manifest_url,
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

  webapps::WebApkInstallResult result() { return result_; }

 private:
  void OnCompleted(webapps::WebApkInstallResult result,
                   std::unique_ptr<std::string> serialized_webapk,
                   bool relax_updates,
                   const std::string& webapk_package) {
    result_ = result;
    std::move(on_completed_callback_).Run();
  }

  // Called after the installation process has succeeded or failed.
  base::OnceClosure on_completed_callback_;

  // The result of the installation process.
  webapps::WebApkInstallResult result_;
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
        update_request_path, webapps::ShortcutInfo((GURL())), GURL(), "", "",
        "", "", std::map<std::string, webapps::WebApkIconHasher::Icon>(), false,
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

class WebApkInstallerBrowserTest : public AndroidBrowserTest {
 public:
  typedef base::RepeatingCallback<
      std::unique_ptr<net::test_server::HttpResponse>(void)>
      WebApkResponseBuilder;

  WebApkInstallerBrowserTest() {}

  WebApkInstallerBrowserTest(const WebApkInstallerBrowserTest&) = delete;
  WebApkInstallerBrowserTest& operator=(const WebApkInstallerBrowserTest&) =
      delete;

  ~WebApkInstallerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->AddDefaultHandlers(base::FilePath(kTestDataDir));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&WebApkInstallerBrowserTest::HandleWebApkRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    SetDefaults();
    AndroidBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<WebApkInstaller> CreateDefaultWebApkInstaller() {
    auto installer = std::unique_ptr<WebApkInstaller>(
        new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));
    installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
    return installer;
  }

  webapps::ShortcutInfo DefaultShortcutInfo() {
    webapps::ShortcutInfo info(embedded_test_server()->GetURL(kStartUrl));
    info.best_primary_icon_url =
        embedded_test_server()->GetURL(kBestPrimaryIconUrl);
    info.splash_image_url = embedded_test_server()->GetURL(kBestSplashIconUrl);
    info.best_shortcut_icon_urls.push_back(
        embedded_test_server()->GetURL(kBestShortcutIconUrl));
    return info;
  }

  std::unique_ptr<std::string> DefaultSerializedWebApk() {
    std::string icon_url_1 =
        embedded_test_server()->GetURL("/icon1.png").spec();
    std::string icon_url_2 =
        embedded_test_server()->GetURL("/icon2.png").spec();
    std::map<std::string, webapps::WebApkIconHasher::Icon>
        icon_url_to_murmur2_hash;
    icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
    icon_url_to_murmur2_hash[icon_url_2] = {"data2", "2"};

    std::string primary_icon_data = "data3";
    std::string splash_icon_data = "data4";

    webapps::ShortcutInfo info(GURL::EmptyGURL());

    return webapps::BuildProtoInBackground(
        info, info.manifest_id, primary_icon_data, splash_icon_data,
        /*package_name*/ "", /*version*/ "",
        std::move(icon_url_to_murmur2_hash), true /* is_manifest_stale */,
        true /* is_app_identity_update_supported */,
        std::vector<webapps::WebApkUpdateReason>());
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

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    SetWebApkServerUrl(embedded_test_server()->GetURL(kServerUrl));
    SetWebApkResponseBuilder(
        base::BindRepeating(&BuildValidWebApkResponse, kToken));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request) {
    return (request.relative_url == kServerUrl)
               ? webapk_response_builder_.Run()
               : std::unique_ptr<net::test_server::HttpResponse>();
  }

  // Builds response to the WebAPK creation request.
  WebApkResponseBuilder webapk_response_builder_;
};

// Test installation succeeding.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest, Success) {
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if there is not enough space on device.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest, FailOnLowSpace) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::NOT_ENOUGH_SPACE));
  installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(webapps::WebApkInstallResult::NOT_ENOUGH_SPACE, runner.result());
}

// Test that installation succeeds when the primary icon is guarded by
// a Cross-Origin-Resource-Policy: same-origin header and the icon is
// same-origin with the start URL.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       CrossOriginResourcePolicySameOriginIconSuccess) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url =
      embedded_test_server()->GetURL(kBestPrimaryIconCorpUrl);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if fetching the bitmap at the best primary icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       BestPrimaryIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url =
      embedded_test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(webapps::WebApkInstallResult::ICON_HASHER_ERROR, runner.result());
}

// Test that installation fails if fetching the bitmap at the best splash icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       BestSplashIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.splash_image_url = embedded_test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          shortcut_info);
  EXPECT_EQ(webapps::WebApkInstallResult::ICON_HASHER_ERROR, runner.result());
}

// Test that installation fails if the WebAPK server url is invalid.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       CreateWebApkInvalidServerUrl) {
  SetWebApkServerUrl(GURL());
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(webapps::WebApkInstallResult::SERVER_URL_INVALID, runner.result());
}

// Test that installation fails if the WebAPK creation request times out.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       CreateWebApkRequestTimesOut) {
  SetWebApkServerUrl(embedded_test_server()->GetURL("/slow?1000"));
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));
  installer->SetTimeoutMs(100);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(webapps::WebApkInstallResult::REQUEST_TIMEOUT, runner.result());
}

// InstallForService tests

// Test installation for service succeeding
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest, ServiceSuccess) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));

  std::unique_ptr<std::string> serialized_proto = DefaultSerializedWebApk();
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();

  WebApkInstallerRunner runner;
  runner.RunInstallForService(std::move(installer), std::move(serialized_proto),
                              shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test installation for service failing if not enough space
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest, ServiceFailOnLowSpace) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::NOT_ENOUGH_SPACE));

  std::unique_ptr<std::string> serialized_proto = DefaultSerializedWebApk();
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();

  WebApkInstallerRunner runner;
  runner.RunInstallForService(std::move(installer), std::move(serialized_proto),
                              shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(webapps::WebApkInstallResult::NOT_ENOUGH_SPACE, runner.result());
}

// Test installation for service failing if serialized apk invalid.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       ServiceFailOnInvalidSerializedWebApk) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));

  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  std::string invalid_serialized_webapk = "😀";

  WebApkInstallerRunner runner;
  runner.RunInstallForService(
      std::move(installer),
      std::make_unique<std::string>(invalid_serialized_webapk),
      shortcut_info.short_name, shortcut_info.source);

  EXPECT_EQ(webapps::WebApkInstallResult::REQUEST_INVALID, runner.result());
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
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       UnparsableCreateWebApkResponse) {
  SetWebApkResponseBuilder(base::BindRepeating(&BuildUnparsableWebApkResponse));

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), web_contents(),
                          DefaultShortcutInfo());
  EXPECT_EQ(webapps::WebApkInstallResult::SERVER_ERROR, runner.result());
}

// Test update succeeding.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest, UpdateSuccess) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  UpdateRequestStorer().StoreSync(update_request_path);
  ASSERT_TRUE(base::PathExists(update_request_path));

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that an update suceeds if the WebAPK server returns a HTTP response with
// an empty token. The WebAPK server sends an empty token when:
// - The server is unable to update the WebAPK in the way that the client
//   requested.
// AND
// - The most up to date version of the WebAPK on the server is identical to the
//   one installed on the client.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       UpdateSuccessWithEmptyTokenInResponse) {
  SetWebApkResponseBuilder(base::BindRepeating(&BuildValidWebApkResponse, ""));

  base::ScopedAllowBlockingForTesting allow_blocking;
  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  UpdateRequestStorer().StoreSync(update_request_path);
  ASSERT_TRUE(base::PathExists(update_request_path));
  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that an update fails if the "update request path" points to an update
// file with the incorrect format.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       UpdateFailsUpdateRequestWrongFormat) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ScopedTempFile scoped_file;
  base::FilePath update_request_path = scoped_file.GetFilePath();
  base::WriteFile(update_request_path, "😀");

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(webapps::WebApkInstallResult::REQUEST_INVALID, runner.result());
}

// Test that an update fails if the "update request path" points to a
// non-existing file.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       UpdateFailsUpdateRequestFileDoesNotExist) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath update_request_path;
  {
    ScopedTempFile scoped_file;
    update_request_path = scoped_file.GetFilePath();
  }
  ASSERT_FALSE(base::PathExists(update_request_path));

  WebApkInstallerRunner runner;
  runner.RunUpdateWebApk(CreateDefaultWebApkInstaller(), update_request_path);
  EXPECT_EQ(webapps::WebApkInstallResult::REQUEST_INVALID, runner.result());
}

// Test that StoreUpdateRequestToFile() creates directories if needed when
// writing to the passed in |update_file_path|.
IN_PROC_BROWSER_TEST_F(WebApkInstallerBrowserTest,
                       StoreUpdateRequestToFileCreatesDirectories) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath outer_file_path;
  ASSERT_TRUE(CreateNewTempDirectory("", &outer_file_path));
  base::FilePath update_request_path =
      outer_file_path.Append("deep").Append("deeper");
  UpdateRequestStorer().StoreSync(update_request_path);
  EXPECT_TRUE(base::PathExists(update_request_path));

  // Clean up
  base::DeletePathRecursively(outer_file_path);
}
