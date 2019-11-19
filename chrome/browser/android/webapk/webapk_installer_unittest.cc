// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
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
const char* kBestBadgeIconUrl = "/nostore.html";

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
// WebApkInstaller::StartInstallingDownloadedWebApk() and
// WebApkInstaller::StartUpdateUsingDownloadedWebApk() and
// WebApkInstaller::CanUseGooglePlayInstallService() and
// WebApkInstaller::InstallOrUpdateWebApkFromGooglePlay() are stubbed out.
class TestWebApkInstaller : public WebApkInstaller {
 public:
  explicit TestWebApkInstaller(content::BrowserContext* browser_context,
                               SpaceStatus status)
      : WebApkInstaller(browser_context), test_space_status_(status) {}

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

  DISALLOW_COPY_AND_ASSIGN(TestWebApkInstaller);
};

// Runs the WebApkInstaller installation process/update and blocks till done.
class WebApkInstallerRunner {
 public:
  WebApkInstallerRunner() {}

  ~WebApkInstallerRunner() {}

  void RunInstallWebApk(std::unique_ptr<WebApkInstaller> installer,
                        const ShortcutInfo& info) {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();

    // WebApkInstaller owns itself.
    WebApkInstaller::InstallAsyncForTesting(
        installer.release(), info, SkBitmap(), false, SkBitmap(),
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
    on_completed_callback_.Run();
  }

  // Called after the installation process has succeeded or failed.
  base::Closure on_completed_callback_;

  // The result of the installation process.
  WebApkInstallResult result_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerRunner);
};

// Helper class for calling WebApkInstaller::StoreUpdateRequestToFile()
// synchronously.
class UpdateRequestStorer {
 public:
  UpdateRequestStorer() {}

  void StoreSync(const base::FilePath& update_request_path) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    WebApkInstaller::StoreUpdateRequestToFile(
        update_request_path, ShortcutInfo((GURL())), SkBitmap(), false,
        SkBitmap(), "", "", std::map<std::string, std::string>(), false,
        WebApkUpdateReason::PRIMARY_ICON_HASH_DIFFERS,
        base::BindOnce(&UpdateRequestStorer::OnComplete,
                       base::Unretained(this)));
    run_loop.Run();
  }

 private:
  void OnComplete(bool success) { quit_closure_.Run(); }

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRequestStorer);
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
  ~BuildProtoRunner() {}

  void BuildSync(
      const GURL& best_primary_icon_url,
      const GURL& best_badge_icon_url,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale) {
    ShortcutInfo info(GURL::EmptyGURL());
    info.best_primary_icon_url = best_primary_icon_url;
    info.best_badge_icon_url = best_badge_icon_url;

    SkBitmap primary_icon(gfx::test::CreateBitmap(144, 144));
    SkBitmap badge_icon(gfx::test::CreateBitmap(72, 72));
    WebApkInstaller::BuildProto(
        info, primary_icon, false /* is_primary_icon_maskable */, badge_icon,
        "" /* package_name */, "" /* version */, icon_url_to_murmur2_hash,
        is_manifest_stale,
        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  webapk::WebApk* GetWebApkRequest() { return webapk_request_.get(); }

 private:
  // Called when the |webapk_request_| is populated.
  void OnBuiltWebApkProto(std::unique_ptr<std::string> serialized_proto) {
    webapk_request_ = std::make_unique<webapk::WebApk>();
    webapk_request_->ParseFromString(*serialized_proto);
    on_completed_callback_.Run();
  }

  // The populated webapk::WebApk.
  std::unique_ptr<webapk::WebApk> webapk_request_;

  // Called after the |webapk_request_| is built.
  base::Closure on_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(BuildProtoRunner);
};

class ScopedTempFile {
 public:
  ScopedTempFile() { CHECK(base::CreateTemporaryFile(&file_path_)); }

  ~ScopedTempFile() {
    base::DeleteFile(file_path_, false);
  }

  const base::FilePath& GetFilePath() { return file_path_; }

 private:
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTempFile);
};

}  // anonymous namespace

class WebApkInstallerTest : public ::testing::Test {
 public:
  typedef base::Callback<std::unique_ptr<net::test_server::HttpResponse>(void)>
      WebApkResponseBuilder;

  WebApkInstallerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~WebApkInstallerTest() override {}

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebApkInstallerTest::HandleWebApkRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    profile_.reset(new TestingProfile());

    SetDefaults();
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<WebApkInstaller> CreateDefaultWebApkInstaller() {
    auto installer = std::unique_ptr<WebApkInstaller>(
        new TestWebApkInstaller(profile_.get(), SpaceStatus::ENOUGH_SPACE));
    installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
    return installer;
  }

  ShortcutInfo DefaultShortcutInfo() {
    ShortcutInfo info(test_server_.GetURL(kStartUrl));
    info.best_primary_icon_url = test_server_.GetURL(kBestPrimaryIconUrl);
    info.best_badge_icon_url = test_server_.GetURL(kBestBadgeIconUrl);
    return info;
  }

  // Sets the URL to send the webapk::CreateWebApkRequest to. WebApkInstaller
  // should fail if the URL is not |kServerUrl|.
  void SetWebApkServerUrl(const GURL& server_url) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());
  }

  // Sets the function that should be used to build the response to the
  // WebAPK creation request.
  void SetWebApkResponseBuilder(const WebApkResponseBuilder& builder) {
    webapk_response_builder_ = builder;
  }

  std::unique_ptr<BuildProtoRunner> CreateBuildProtoRunner() {
    return std::unique_ptr<BuildProtoRunner>(new BuildProtoRunner());
  }

  Profile* profile() { return profile_.get(); }
  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  // Sets default configuration for running WebApkInstaller.
  void SetDefaults() {
    SetWebApkServerUrl(test_server_.GetURL(kServerUrl));
    SetWebApkResponseBuilder(base::Bind(&BuildValidWebApkResponse, kToken));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request) {
    return (request.relative_url == kServerUrl)
               ? webapk_response_builder_.Run()
               : std::unique_ptr<net::test_server::HttpResponse>();
  }

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;
  net::EmbeddedTestServer test_server_;

  // Builds response to the WebAPK creation request.
  WebApkResponseBuilder webapk_response_builder_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallerTest);
};

// Test installation succeeding.
TEST_F(WebApkInstallerTest, Success) {
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(),
                          DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if there is not enough space on device.
TEST_F(WebApkInstallerTest, FailOnLowSpace) {
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::NOT_ENOUGH_SPACE));
  installer->SetTimeoutMs(kWebApkServerRequestTimeoutMs);
  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), DefaultShortcutInfo());
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation succeeds when the primary icon is guarded by
// a Cross-Origin-Resource-Policy: same-origin header and the icon is
// same-origin with the start URL.
TEST_F(WebApkInstallerTest, CrossOriginResourcePolicySameOriginIconSuccess) {
  ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconCorpUrl);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), shortcut_info);
  EXPECT_EQ(WebApkInstallResult::SUCCESS, runner.result());
}

// Test that installation fails if fetching the bitmap at the best primary icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
TEST_F(WebApkInstallerTest, BestPrimaryIconUrlDownloadTimesOut) {
  ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url = test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), shortcut_info);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation fails if fetching the bitmap at the best badge icon
// URL returns no content. In a perfect world the fetch would always succeed
// because the fetch for the same icon succeeded recently.
TEST_F(WebApkInstallerTest, BestBadgeIconUrlDownloadTimesOut) {
  ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_badge_icon_url = test_server()->GetURL("/nocontent");

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(), shortcut_info);
  EXPECT_EQ(WebApkInstallResult::FAILURE, runner.result());
}

// Test that installation fails if the WebAPK creation request times out.
TEST_F(WebApkInstallerTest, CreateWebApkRequestTimesOut) {
  SetWebApkServerUrl(test_server()->GetURL("/slow?1000"));
  std::unique_ptr<WebApkInstaller> installer(
      new TestWebApkInstaller(profile(), SpaceStatus::ENOUGH_SPACE));
  installer->SetTimeoutMs(100);

  WebApkInstallerRunner runner;
  runner.RunInstallWebApk(std::move(installer), DefaultShortcutInfo());
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
  runner.RunInstallWebApk(CreateDefaultWebApkInstaller(),
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
  base::DeleteFile(outer_file_path, true /* recursive */);
}

// When there is no Web Manifest available for a site, an empty
// |best_primary_icon_url| and an empty |best_badge_icon_url| is used to build a
// WebApk update request. Tests the request can be built properly.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsObsolete) {
  std::string icon_url_1 = test_server()->GetURL("/icon1.png").spec();
  std::string icon_url_2 = test_server()->GetURL("/icon2.png").spec();
  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = "1";
  icon_url_to_murmur2_hash[icon_url_2] = "2";

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(), icon_url_to_murmur2_hash,
                    true /* is_manifest_stale*/);
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(4, manifest.icons_size());

  webapk::Image icons[4];
  for (int i = 0; i < 4; ++i)
    icons[i] = manifest.icons(i);

  EXPECT_EQ("", icons[0].src());
  EXPECT_FALSE(icons[0].has_hash());
  EXPECT_TRUE(icons[0].has_image_data());

  EXPECT_EQ("", icons[1].src());
  EXPECT_FALSE(icons[1].has_hash());
  EXPECT_TRUE(icons[1].has_image_data());

  EXPECT_EQ(icon_url_1, icons[2].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[icon_url_1], icons[2].hash());
  EXPECT_FALSE(icons[2].has_image_data());

  EXPECT_EQ(icon_url_2, icons[3].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[icon_url_2], icons[3].hash());
  EXPECT_FALSE(icons[3].has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest.
TEST_F(WebApkInstallerTest, BuildWebApkProtoWhenManifestIsAvailable) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::string best_badge_icon_url =
      test_server()->GetURL(kBestBadgeIconUrl).spec();
  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = "0";
  icon_url_to_murmur2_hash[best_primary_icon_url] = "1";
  icon_url_to_murmur2_hash[best_badge_icon_url] = "2";

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(best_primary_icon_url), GURL(best_badge_icon_url),
                    icon_url_to_murmur2_hash, false /* is_manifest_stale*/);
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  webapk::Image icons[3];
  for (int i = 0; i < 3; ++i)
    icons[i] = manifest.icons(i);

  // Check protobuf fields for /icon.png.
  EXPECT_EQ(icon_url_1, icons[0].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[icon_url_1], icons[0].hash());
  EXPECT_EQ(0, icons[0].usages_size());
  EXPECT_FALSE(icons[0].has_image_data());

  // Check protobuf fields for kBestBadgeIconUrl.
  EXPECT_EQ(best_badge_icon_url, icons[1].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[best_badge_icon_url], icons[1].hash());
  EXPECT_THAT(icons[1].usages(),
              testing::ElementsAre(webapk::Image::BADGE_ICON));
  EXPECT_TRUE(icons[1].has_image_data());

  // Check protobuf fields for kBestPrimaryIconUrl.
  EXPECT_EQ(best_primary_icon_url, icons[2].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[best_primary_icon_url], icons[2].hash());
  EXPECT_THAT(icons[2].usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
  EXPECT_TRUE(icons[2].has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest, and
// primary icon and badge icon share the same URL.
TEST_F(WebApkInstallerTest, BuildWebApkProtoPrimaryIconAndBadgeIconSameUrl) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = "1";
  icon_url_to_murmur2_hash[best_icon_url] = "0";

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(best_icon_url), GURL(best_icon_url),
                    icon_url_to_murmur2_hash, false /* is_manifest_stale*/);
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(2, manifest.icons_size());

  webapk::Image icons[2];
  for (int i = 0; i < 2; ++i)
    icons[i] = manifest.icons(i);

  // Check protobuf fields for /icon.png.
  EXPECT_EQ(icon_url_1, icons[0].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[icon_url_1], icons[0].hash());
  EXPECT_EQ(0, icons[0].usages_size());
  EXPECT_FALSE(icons[0].has_image_data());

  // Check protobuf fields for kBestPrimaryIconUrl.
  EXPECT_EQ(best_icon_url, icons[1].src());
  EXPECT_EQ(icon_url_to_murmur2_hash[best_icon_url], icons[1].hash());
  EXPECT_THAT(icons[1].usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON,
                                   webapk::Image::BADGE_ICON));
  EXPECT_TRUE(icons[1].has_image_data());
}
