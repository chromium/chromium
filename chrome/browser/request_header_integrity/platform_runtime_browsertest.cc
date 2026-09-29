// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/request_header_integrity/platform_runtime_host.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "chrome/common/request_header_integrity/platform_runtime_headers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/google_header_names.h"
#endif

#if !defined(VALIDATE_HEADER_NAME)
#define VALIDATE_HEADER_NAME "X-Placeholder-3"
#endif

namespace request_header_integrity {
namespace {

net::HttpRequestHeaders MakeHeaders(std::string_view name,
                                    std::string_view value) {
  net::HttpRequestHeaders headers;
  headers.SetHeader(name, value);
  return headers;
}

class FakeService : public mojom::PlatformRuntimeService,
                    public mojom::PlatformRuntime {
 public:
  explicit FakeService(
      mojo::PendingReceiver<mojom::PlatformRuntimeService> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~FakeService() override = default;

#if BUILDFLAG(IS_WIN)
// <windows.h>, pulled in by headers included after platform_runtime.mojom.h,
// #defines LoadLibrary as LoadLibraryW, which would otherwise rename this
// override so that it no longer matches the mojom method.
#pragma push_macro("LoadLibrary")
#undef LoadLibrary
#endif
  void LoadLibrary(
      const base::FilePath& library_path,
      mojo::PendingReceiver<mojom::PlatformRuntime> runtime) override {
    last_library_path_ = library_path;
    runtime_receiver_.Bind(std::move(runtime));
  }
#if BUILDFLAG(IS_WIN)
#pragma pop_macro("LoadLibrary")
#endif

  void ProcessHeaders(const net::HttpRequestHeaders& input_headers,
                      ProcessHeadersCallback callback) override {
    ++call_count_;
    last_input_headers_ = input_headers;
    if (status_ == mojom::PlatformRuntimeStatus::kSuccess) {
      std::move(callback).Run(base::ok(output_));
    } else {
      std::move(callback).Run(base::unexpected(status_));
    }
  }

  void SetReply(mojom::PlatformRuntimeStatus status,
                net::HttpRequestHeaders output) {
    status_ = status;
    output_ = std::move(output);
  }

  int call_count() const { return call_count_; }
  const base::FilePath& last_library_path() const { return last_library_path_; }

 private:
  mojo::Receiver<mojom::PlatformRuntimeService> receiver_;
  mojo::Receiver<mojom::PlatformRuntime> runtime_receiver_{this};
  mojom::PlatformRuntimeStatus status_ = mojom::PlatformRuntimeStatus::kSuccess;
  net::HttpRequestHeaders output_;
  int call_count_ = 0;
  base::FilePath last_library_path_;
  net::HttpRequestHeaders last_input_headers_;
};

class PlatformRuntimeBrowserTest : public InProcessBrowserTest {
 public:
  PlatformRuntimeBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PlatformRuntimeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    main_thread_task_runner_ =
        base::SingleThreadTaskRunner::GetCurrentDefault();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.RegisterRequestHandler(base::BindRepeating(
        &PlatformRuntimeBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_.SetCertHostnames({"www.google.com"});
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    host()->SetServiceLauncherForTesting({});
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  PlatformRuntimeHost* host() {
    auto* host = g_browser_process->GetFeatures()->platform_runtime_host();
    CHECK(host);
    return host;
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  GURL GetGoogleUrl(const std::string& path) const {
    return https_server_.GetURL("www.google.com", path);
  }

  void WaitForRequest(const GURL& url) {
    ASSERT_TRUE(main_thread_task_runner_->RunsTasksInCurrentSequence());
    base::RunLoop loop;
    {
      base::AutoLock lock(lock_);
      if (received_headers_.contains(url)) {
        return;
      }
      done_callbacks_.emplace(url, loop.QuitClosure());
    }
    loop.Run();
  }

  std::optional<std::string> GetReceivedHeader(const GURL& url,
                                               std::string_view header_name) {
    EXPECT_TRUE(main_thread_task_runner_->RunsTasksInCurrentSequence());
    base::AutoLock lock(lock_);
    auto it = received_headers_.find(url);
    if (it == received_headers_.end()) {
      return std::nullopt;
    }
    auto header_it = it->second.find(header_name);
    if (header_it == it->second.end()) {
      return std::nullopt;
    }
    return header_it->second;
  }

  void ClearReceivedHeaders() {
    EXPECT_TRUE(main_thread_task_runner_->RunsTasksInCurrentSequence());
    base::AutoLock lock(lock_);
    received_headers_.clear();
    done_callbacks_.clear();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_FALSE(main_thread_task_runner_->RunsTasksInCurrentSequence());
    base::AutoLock lock(lock_);

    std::string host;
    auto host_it = request.headers.find("Host");
    if (host_it != request.headers.end()) {
      host = host_it->second;
    }
    auto components = base::SplitStringOnce(host, ':');
    if (components) {
      host = components->first;
    }

    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    GURL original_url = request.GetURL().ReplaceComponents(replacements);

    received_headers_[original_url] = request.headers;
    auto it = done_callbacks_.find(original_url);
    if (it != done_callbacks_.end()) {
      std::move(it->second).Run();
      done_callbacks_.erase(it);
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    if (request.relative_url == "/page.html") {
      response->set_content_type("text/html");
      response->set_content(
          "<!DOCTYPE html><html><head><title>Test</title></head>"
          "<body><p>Test Page</p></body></html>");
    } else {
      response->set_content_type("application/json");
      response->set_content("{\"status\":\"ok\"}");
    }
    return response;
  }

  net::EmbeddedTestServer https_server_;
  base::Lock lock_;
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_
      GUARDED_BY(lock_);
  std::map<GURL, base::OnceClosure> done_callbacks_ GUARDED_BY(lock_);
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

// Verifies that the platform runtime header is applied to both main frame
// navigation requests (browser process) and subresource requests (renderer
// process).
IN_PROC_BROWSER_TEST_F(PlatformRuntimeBrowserTest,
                       HeadersAppliedToMainAndSubresourceRequests) {
  constexpr char kTestToken[] = "test_platform_runtime_token_alpha";
  host()->SetHeadersForTesting(MakeHeaders(VALIDATE_HEADER_NAME, kTestToken));

  // Main frame navigation request (Browser process).
  const GURL page_url = GetGoogleUrl("/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(GetReceivedHeader(page_url, VALIDATE_HEADER_NAME), kTestToken);

  // Subresource fetch request (Renderer process).
  const GURL subresource_url = GetGoogleUrl("/subresource");
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1).catch(() => {});", subresource_url)));
  WaitForRequest(subresource_url);
  EXPECT_EQ(GetReceivedHeader(subresource_url, VALIDATE_HEADER_NAME),
            kTestToken);
}

// Verifies that dynamic header updates published by the host propagate to both
// the browser process and existing live renderer processes without page reload.
IN_PROC_BROWSER_TEST_F(PlatformRuntimeBrowserTest,
                       DynamicHeaderUpdatesPropagateToRenderer) {
  constexpr char kTokenV1[] = "test_token_v1";
  constexpr char kTokenV2[] = "test_token_v2";

  // Publish initial token.
  host()->SetHeadersForTesting(MakeHeaders(VALIDATE_HEADER_NAME, kTokenV1));

  const GURL page_url = GetGoogleUrl("/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(GetReceivedHeader(page_url, VALIDATE_HEADER_NAME), kTokenV1);

  const GURL subresource_url1 = GetGoogleUrl("/subresource_1");
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1).catch(() => {});", subresource_url1)));
  WaitForRequest(subresource_url1);
  EXPECT_EQ(GetReceivedHeader(subresource_url1, VALIDATE_HEADER_NAME),
            kTokenV1);

  // Rotate to token V2 while the page remains loaded in the renderer.
  host()->SetHeadersForTesting(MakeHeaders(VALIDATE_HEADER_NAME, kTokenV2));

  // A subsequent subresource fetch from the same renderer page must now
  // carry token V2.
  const GURL subresource_url2 = GetGoogleUrl("/subresource_2");
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1).catch(() => {});", subresource_url2)));
  WaitForRequest(subresource_url2);
  EXPECT_EQ(GetReceivedHeader(subresource_url2, VALIDATE_HEADER_NAME),
            kTokenV2);

  // A subsequent main frame navigation in the browser must also carry token V2.
  ClearReceivedHeaders();
  const GURL page_url2 = GetGoogleUrl("/page.html?v=2");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url2));
  EXPECT_EQ(GetReceivedHeader(page_url2, VALIDATE_HEADER_NAME), kTokenV2);
}

// Verifies the end-to-end flow with the service launcher: OnComponentReady
// verifies the library, connects to the service via Mojo, receives computed
// headers, and stamps them on main and subresource requests.
IN_PROC_BROWSER_TEST_F(PlatformRuntimeBrowserTest,
                       EndToEndServiceLaunchAndHeaderStamping) {
  constexpr char kComputedToken[] = "service_computed_integrity_token";

  std::unique_ptr<FakeService> fake_service;
  base::RunLoop service_ready_loop;

  host()->SetServiceLauncherForTesting(base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<mojom::PlatformRuntimeService> receiver) {
        fake_service = std::make_unique<FakeService>(std::move(receiver));
        fake_service->SetReply(
            mojom::PlatformRuntimeStatus::kSuccess,
            MakeHeaders(VALIDATE_HEADER_NAME, kComputedToken));
        service_ready_loop.Quit();
      }));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath library_path = temp_dir.GetPath().AppendASCII("test_lib.dll");
  ASSERT_TRUE(base::WriteFile(library_path, "dummy"));

  base::RunLoop headers_published_loop;
  auto subscription = host()->RegisterHeadersChangedCallback(
      headers_published_loop.QuitClosure());

  host()->OnComponentReady(library_path);
  service_ready_loop.Run();
  headers_published_loop.Run();

  ASSERT_TRUE(fake_service);
  EXPECT_EQ(1, fake_service->call_count());
  EXPECT_EQ(library_path, fake_service->last_library_path());

  // Navigation request (Browser process).
  const GURL page_url = GetGoogleUrl("/page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  EXPECT_EQ(GetReceivedHeader(page_url, VALIDATE_HEADER_NAME), kComputedToken);

  // Subresource fetch (Renderer process).
  const GURL subresource_url = GetGoogleUrl("/subresource");
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("fetch($1).catch(() => {});", subresource_url)));
  WaitForRequest(subresource_url);
  EXPECT_EQ(GetReceivedHeader(subresource_url, VALIDATE_HEADER_NAME),
            kComputedToken);
}

}  // namespace
}  // namespace request_header_integrity
