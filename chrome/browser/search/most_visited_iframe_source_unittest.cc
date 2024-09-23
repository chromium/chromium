// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/most_visited_iframe_source.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/grit/new_tab_page_instant_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kNonInstantOrigin[] = "http://evil";
const char kInstantOrigin[] = "chrome-search://instant";

class TestMostVisitedIframeSource : public MostVisitedIframeSource {
 public:
  using MostVisitedIframeSource::GetMimeType;
  using MostVisitedIframeSource::SendJSWithOrigin;
  using MostVisitedIframeSource::SendResource;
  using MostVisitedIframeSource::ShouldServiceRequest;

  void set_origin(std::string origin) { origin_ = origin; }

 protected:
  std::string GetSource() override { return "test"; }

  bool ServesPath(const std::string& path) const override {
    return path == "/valid.html" || path == "/valid.js";
  }

  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override {}

  // RenderFrameHost is hard to mock in concert with everything else, so stub
  // this method out for testing.
  bool GetOrigin(const content::WebContents::Getter& wc_getter,
                 std::string* origin) const override {
    if (origin_.empty())
      return false;
    *origin = origin_;
    return true;
  }

 private:
  std::string origin_;
};

class MostVisitedIframeSourceTest : public testing::Test {
 public:
  // net::URLRequest wants to be executed with a message loop that has TYPE_IO.
  MostVisitedIframeSourceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        response_(nullptr) {}

  int GetInstantRendererPID() const { return mock_host_.GetID(); }
  int GetNonInstantRendererPID() const { return mock_host_.GetID() + 1; }
  int GetInvalidRendererPID() const { return mock_host_.GetID() + 2; }

  TestMostVisitedIframeSource* source() { return source_.get(); }

  std::string response_string() {
    if (response_.get()) {
      return std::string(base::as_string_view(*response_));
    }
    return "";
  }

  void SendResource(int resource_id) {
    source()->SendResource(
        resource_id, base::BindOnce(&MostVisitedIframeSourceTest::SaveResponse,
                                    base::Unretained(this)));
  }

  void SendJSWithOrigin(int resource_id) {
    source()->SendJSWithOrigin(
        resource_id, content::WebContents::Getter(),
        base::BindOnce(&MostVisitedIframeSourceTest::SaveResponse,
                       base::Unretained(this)));
  }

  bool ShouldService(const std::string& path, int process_id) {
    return source()->ShouldServiceRequest(GURL(path), &profile_, process_id);
  }

 private:
  void SetUp() override {
    source_ = std::make_unique<TestMostVisitedIframeSource>();
    source_->set_origin(kInstantOrigin);
    auto* instant_service = InstantServiceFactory::GetForProfile(&profile_);
    instant_service->AddInstantProcess(&mock_host_);
    response_ = nullptr;
  }

  void TearDown() override { source_.reset(); }

  void SaveResponse(scoped_refptr<base::RefCountedMemory> data) {
    response_ = data;
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  content::MockRenderProcessHost mock_host_{&profile_};
  std::unique_ptr<TestMostVisitedIframeSource> source_;
  scoped_refptr<base::RefCountedMemory> response_;
};

TEST_F(MostVisitedIframeSourceTest, ShouldServiceRequest) {
  source()->set_origin(kNonInstantOrigin);
  EXPECT_FALSE(
      ShouldService("http://test/loader.js", GetNonInstantRendererPID()));
  source()->set_origin(kInstantOrigin);
  EXPECT_FALSE(
      ShouldService("chrome-search://bogus/valid.js", GetInstantRendererPID()));
  source()->set_origin(kInstantOrigin);
  EXPECT_FALSE(
      ShouldService("chrome-search://test/bogus.js", GetInstantRendererPID()));
  source()->set_origin(kInstantOrigin);
  EXPECT_TRUE(
      ShouldService("chrome-search://test/valid.js", GetInstantRendererPID()));
  source()->set_origin(kNonInstantOrigin);
  EXPECT_FALSE(ShouldService("chrome-search://test/valid.js",
                             GetNonInstantRendererPID()));
  source()->set_origin(std::string());
  EXPECT_FALSE(
      ShouldService("chrome-search://test/valid.js", GetInvalidRendererPID()));
}

TEST_F(MostVisitedIframeSourceTest, GetMimeType) {
  // URLDataManagerBackend does not include / in path_and_query.
  EXPECT_EQ("text/html",
            source()->GetMimeType(GURL("chrome-search://test/foo.html")));
  EXPECT_EQ("application/javascript",
            source()->GetMimeType(GURL("chrome-search://test/foo.js")));
  EXPECT_EQ("text/css",
            source()->GetMimeType(GURL("chrome-search://test/foo.css")));
  EXPECT_EQ("", source()->GetMimeType(GURL("chrome-search://test/bogus")));
}

TEST_F(MostVisitedIframeSourceTest, SendResource) {
  SendResource(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_HTML);
  EXPECT_FALSE(response_string().empty());
}

TEST_F(MostVisitedIframeSourceTest, SendJSWithOrigin) {
  source()->set_origin(kInstantOrigin);
  SendJSWithOrigin(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_JS);
  EXPECT_FALSE(response_string().empty());
  source()->set_origin(kNonInstantOrigin);
  SendJSWithOrigin(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_JS);
  EXPECT_FALSE(response_string().empty());
  source()->set_origin(std::string());
  SendJSWithOrigin(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_JS);
  EXPECT_TRUE(response_string().empty());
}
