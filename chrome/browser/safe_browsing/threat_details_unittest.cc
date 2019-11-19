// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/browser/referrer_chain_provider.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/browser/threat_details_history.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using content::WebContents;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedPointwise;

namespace safe_browsing {

namespace {

// Mixture of HTTP and HTTPS.  No special treatment for HTTPS.
static const char* kOriginalLandingURL =
    "http://www.originallandingpage.com/with/path";
static const char* kDOMChildURL = "https://www.domchild.com/with/path";
static const char* kDOMChildUrl2 = "https://www.domchild2.com/path";
static const char* kDOMParentURL = "https://www.domparent.com/with/path";
static const char* kFirstRedirectURL = "http://redirectone.com/with/path";
static const char* kSecondRedirectURL = "https://redirecttwo.com/with/path";
static const char* kReferrerURL = "http://www.referrer.com/with/path";
static const char* kDataURL = "data:text/html;charset=utf-8;base64,PCFET0";
static const char* kBlankURL = "about:blank";

static const char* kThreatURL = "http://www.threat.com/with/path";
static const char* kThreatURLHttps = "https://www.threat.com/with/path";
static const char* kThreatHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: image/jpeg\n"
    "Some-Other-Header: foo\n";  // Persisted for http, stripped for https
static const char* kThreatData = "exploit();";

static const char* kLandingURL = "http://www.landingpage.com/with/path";
static const char* kLandingHeaders =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Content-Length: 1024\n"
    "Set-Cookie: tastycookie\n";  // This header is stripped.
static const char* kLandingData =
    "<iframe src='http://www.threat.com/with/path'>";

using content::BrowserThread;
using content::WebContents;

// Lets us control synchronization of the done callback for ThreatDetails.
// Also exposes the constructor.
class ThreatDetailsWrap : public ThreatDetails {
 public:
  ThreatDetailsWrap(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider)
      : ThreatDetails(ui_manager,
                      web_contents,
                      unsafe_resource,
                      url_loader_factory,
                      history_service,
                      referrer_chain_provider,
                      /*trim_to_ad_tags=*/false,
                      base::Bind(&ThreatDetailsWrap::ThreatDetailsDone,
                                 base::Unretained(this))),
        run_loop_(nullptr),
        done_callback_count_(0) {}

  ThreatDetailsWrap(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      bool trim_to_ad_tags)
      : ThreatDetails(ui_manager,
                      web_contents,
                      unsafe_resource,
                      url_loader_factory,
                      history_service,
                      referrer_chain_provider,
                      trim_to_ad_tags,
                      base::Bind(&ThreatDetailsWrap::ThreatDetailsDone,
                                 base::Unretained(this))),
        run_loop_(nullptr),
        done_callback_count_(0) {}

  ~ThreatDetailsWrap() override {}

  void ThreatDetailsDone(content::WebContents* web_contents) {
    ++done_callback_count_;
    run_loop_->Quit();
    run_loop_ = nullptr;
  }

  // Used to synchronize ThreatDetailsDone() with WaitForThreatDetailsDone().
  // RunLoop::RunUntilIdle() is not sufficient because the MessageLoop task
  // queue completely drains at some point between the send and the wait.
  void SetRunLoopToQuit(base::RunLoop* run_loop) {
    DCHECK(run_loop_ == nullptr);
    run_loop_ = run_loop;
  }

  size_t done_callback_count() { return done_callback_count_; }

  void StartCollection() { ThreatDetails::StartCollection(); }

 private:

  base::RunLoop* run_loop_;
  size_t done_callback_count_;
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  // The safe browsing UI manager does not need a service for this test.
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(nullptr), report_sent_(false) {}

  // When the serialized report is sent, this is called.
  void SendSerializedThreatDetails(const std::string& serialized) override {
    report_sent_ = true;
    serialized_ = serialized;
  }

  const std::string& GetSerialized() { return serialized_; }
  bool ReportWasSent() { return report_sent_; }

 private:
  ~MockSafeBrowsingUIManager() override {}

  std::string serialized_;
  bool report_sent_;
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingUIManager);
};

class MockReferrerChainProvider : public ReferrerChainProvider {
 public:
  virtual ~MockReferrerChainProvider() {}
  MOCK_METHOD3(IdentifyReferrerChainByWebContents,
               AttributionResult(content::WebContents* web_contents,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
  MOCK_METHOD4(IdentifyReferrerChainByEventURL,
               AttributionResult(const GURL& event_url,
                                 SessionID event_tab_id,
                                 int user_gesture_count_limit,
                                 ReferrerChain* out_referrer_chain));
};

}  // namespace

class ThreatDetailsTest : public ChromeRenderViewHostTestHarness {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  ThreatDetailsTest()
      : referrer_chain_provider_(new MockReferrerChainProvider()),
        ui_manager_(new MockSafeBrowsingUIManager()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(true /* delete_file */,
                                                false /* no_db */));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  std::string WaitForThreatDetailsDone(ThreatDetailsWrap* report,
                                       bool did_proceed,
                                       int num_visit) {
    report->FinishCollection(did_proceed, num_visit);
    // Wait for the callback (ThreatDetailsDone).
    base::RunLoop run_loop;
    report->SetRunLoopToQuit(&run_loop);
    run_loop.Run();
    // Make sure the done callback was run exactly once.
    EXPECT_EQ(1u, report->done_callback_count());
    return ui_manager_->GetSerialized();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  bool ReportWasSent() { return ui_manager_->ReportWasSent(); }

 protected:
  void InitResource(SBThreatType threat_type,
                    ThreatSource threat_source,
                    bool is_subresource,
                    const GURL& url,
                    UnsafeResource* resource) {
    resource->url = url;
    resource->is_subresource = is_subresource;
    resource->threat_type = threat_type;
    resource->threat_source = threat_source;
    resource->web_contents_getter =
        SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
            web_contents()->GetMainFrame()->GetProcess()->GetID(),
            web_contents()->GetMainFrame()->GetRoutingID());
  }

  void VerifyResults(const ClientSafeBrowsingReportRequest& report_pb,
                     const ClientSafeBrowsingReportRequest& expected_pb) {
    EXPECT_EQ(expected_pb.type(), report_pb.type());
    EXPECT_EQ(expected_pb.url(), report_pb.url());
    EXPECT_EQ(expected_pb.page_url(), report_pb.page_url());
    EXPECT_EQ(expected_pb.referrer_url(), report_pb.referrer_url());
    EXPECT_EQ(expected_pb.did_proceed(), report_pb.did_proceed());
    EXPECT_EQ(expected_pb.has_repeat_visit(), report_pb.has_repeat_visit());
    if (expected_pb.has_repeat_visit() && report_pb.has_repeat_visit()) {
      EXPECT_EQ(expected_pb.repeat_visit(), report_pb.repeat_visit());
    }

    ASSERT_EQ(expected_pb.resources_size(), report_pb.resources_size());
    // Put the actual resources in a map, then iterate over the expected
    // resources, making sure each exists and is equal.
    std::map<int, const ClientSafeBrowsingReportRequest::Resource*>
        actual_resource_map;
    for (const ClientSafeBrowsingReportRequest::Resource& resource :
         report_pb.resources()) {
      actual_resource_map[resource.id()] = &resource;
    }
    // Make sure no resources disappeared when moving them to a map (IDs should
    // be unique).
    ASSERT_EQ(expected_pb.resources_size(),
              static_cast<int>(actual_resource_map.size()));
    for (const ClientSafeBrowsingReportRequest::Resource& expected_resource :
         expected_pb.resources()) {
      ASSERT_GT(actual_resource_map.count(expected_resource.id()), 0u);
      VerifyResource(*actual_resource_map[expected_resource.id()],
                     expected_resource);
    }

    ASSERT_EQ(expected_pb.dom_size(), report_pb.dom_size());
    // Put the actual elements in a map, then iterate over the expected
    // elements, making sure each exists and is equal.
    std::map<int, const HTMLElement*> actual_dom_map;
    for (const HTMLElement& element : report_pb.dom()) {
      actual_dom_map[element.id()] = &element;
    }
    // Make sure no elements disappeared when moving them to a map (IDs should
    // be unique).
    ASSERT_EQ(expected_pb.dom_size(), static_cast<int>(actual_dom_map.size()));
    for (const HTMLElement& expected_element : expected_pb.dom()) {
      ASSERT_GT(actual_dom_map.count(expected_element.id()), 0u);
      VerifyElement(*actual_dom_map[expected_element.id()], expected_element);
    }

    EXPECT_TRUE(report_pb.client_properties().has_url_api_type());
    EXPECT_EQ(expected_pb.client_properties().url_api_type(),
              report_pb.client_properties().url_api_type());
    EXPECT_EQ(expected_pb.complete(), report_pb.complete());

    EXPECT_EQ(expected_pb.referrer_chain_size(),
              report_pb.referrer_chain_size());
    // TODO: check each elem url
  }

  void VerifyResource(
      const ClientSafeBrowsingReportRequest::Resource& resource,
      const ClientSafeBrowsingReportRequest::Resource& expected) {
    EXPECT_EQ(expected.id(), resource.id());
    EXPECT_EQ(expected.url(), resource.url());
    EXPECT_EQ(expected.parent_id(), resource.parent_id());
    ASSERT_EQ(expected.child_ids_size(), resource.child_ids_size());
    for (int i = 0; i < expected.child_ids_size(); i++) {
      EXPECT_EQ(expected.child_ids(i), resource.child_ids(i));
    }

    // Verify HTTP Responses
    if (expected.has_response()) {
      ASSERT_TRUE(resource.has_response());
      EXPECT_EQ(expected.response().firstline().code(),
                resource.response().firstline().code());

      ASSERT_EQ(expected.response().headers_size(),
                resource.response().headers_size());
      for (int i = 0; i < expected.response().headers_size(); ++i) {
        EXPECT_EQ(expected.response().headers(i).name(),
                  resource.response().headers(i).name());
        EXPECT_EQ(expected.response().headers(i).value(),
                  resource.response().headers(i).value());
      }

      EXPECT_EQ(expected.response().body(), resource.response().body());
      EXPECT_EQ(expected.response().bodylength(),
                resource.response().bodylength());
      EXPECT_EQ(expected.response().bodydigest(),
                resource.response().bodydigest());
    }

    // Verify IP:port pair
    EXPECT_EQ(expected.response().remote_ip(), resource.response().remote_ip());
  }

  void VerifyElement(const HTMLElement& element, const HTMLElement& expected) {
    EXPECT_EQ(expected.id(), element.id());
    EXPECT_EQ(expected.tag(), element.tag());
    EXPECT_EQ(expected.resource_id(), element.resource_id());
    EXPECT_THAT(element.child_ids(),
                UnorderedPointwise(Eq(), expected.child_ids()));
    ASSERT_EQ(expected.attribute_size(), element.attribute_size());
    std::map<std::string, std::string> actual_attributes_map;
    for (const HTMLElement::Attribute& attribute : element.attribute()) {
      actual_attributes_map[attribute.name()] = attribute.value();
    }
    ASSERT_EQ(expected.attribute_size(),
              static_cast<int>(actual_attributes_map.size()));
    for (const HTMLElement::Attribute& expected_attribute :
         expected.attribute()) {
      ASSERT_GT(actual_attributes_map.count(expected_attribute.name()), 0u);
      EXPECT_EQ(expected_attribute.value(),
                actual_attributes_map[expected_attribute.name()]);
    }
  }
  // Adds a page to history.
  // The redirects is the redirect url chain leading to the url.
  void AddPageToHistory(const GURL& url, history::RedirectList* redirects) {
    // The last item of the redirect chain has to be the final url when adding
    // to history backend.
    redirects->push_back(url);
    history_service()->AddPage(url, base::Time::Now(),
                               reinterpret_cast<history::ContextID>(1), 0,
                               GURL(), *redirects, ui::PAGE_TRANSITION_TYPED,
                               history::SOURCE_BROWSED, false);
  }

  void WriteCacheEntry(const std::string& url,
                       const std::string& headers,
                       const std::string& content) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    head->remote_endpoint = net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80);
    head->mime_type = "text/html";
    network::URLLoaderCompletionStatus status;
    status.decoded_body_length = content.size();

    test_url_loader_factory_.AddResponse(GURL(url), std::move(head), content,
                                         status);
  }

  void SimulateFillCache(const std::string& url) {
    WriteCacheEntry(url, kThreatHeaders, kThreatData);
    WriteCacheEntry(kLandingURL, kLandingHeaders, kLandingData);
  }

  std::unique_ptr<MockReferrerChainProvider> referrer_chain_provider_;
  scoped_refptr<MockSafeBrowsingUIManager> ui_manager_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

// Tests creating a simple threat report of a malware URL.
TEST_F(ThreatDetailsTest, ThreatSubResource) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kLandingURL), web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      GURL(kReferrerURL), network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::CLIENT_SIDE_DETECTION,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests creating a simple threat report of a suspicious site that contains
// the referrer chain.
TEST_F(ThreatDetailsTest, SuspiciousSiteWithReferrerChain) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kLandingURL), web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      GURL(kReferrerURL), network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_SUSPICIOUS_SITE, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  ReferrerChain returned_referrer_chain;
  returned_referrer_chain.Add()->set_url(kReferrerURL);
  returned_referrer_chain.Add()->set_url(kSecondRedirectURL);
  EXPECT_CALL(*referrer_chain_provider_,
              IdentifyReferrerChainByWebContents(web_contents(), _, _))
      .WillOnce(DoAll(SetArgPointee<2>(returned_referrer_chain),
                      Return(ReferrerChainProvider::SUCCESS)));

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_SUSPICIOUS);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  // Make sure the referrer chain returned by the provider is copied into the
  // resulting proto.
  expected.mutable_referrer_chain()->CopyFrom(returned_referrer_chain);

  VerifyResults(actual, expected);
}
// Tests creating a simple threat report of a phishing page where the
// subresource has a different original_url.
TEST_F(ThreatDetailsTest, ThreatSubResourceWithOriginalUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_PHISHING, ThreatSource::DATA_SAVER,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  resource.original_url = GURL(kOriginalLandingURL);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::FLYWHEEL);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
  // The Resource for kThreatURL should have the Resource for
  // kOriginalLandingURL (with id 1) as parent.
  pb_resource->set_parent_id(1);

  VerifyResults(actual, expected);
}

// Tests creating a threat report of a UwS page with data from the renderer.
TEST_F(ThreatDetailsTest, ThreatDOMDetails) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER3,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  // Send a message from the DOM, with 2 nodes, a parent and a child.
  std::vector<mojom::ThreatDOMDetailsNodePtr> params;
  mojom::ThreatDOMDetailsNodePtr child_node =
      mojom::ThreatDOMDetailsNode::New();
  child_node->url = GURL(kDOMChildURL);
  child_node->tag_name = "iframe";
  child_node->parent = GURL(kDOMParentURL);
  child_node->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildURL));
  params.push_back(std::move(child_node));
  mojom::ThreatDOMDetailsNodePtr parent_node =
      mojom::ThreatDOMDetailsNode::New();
  parent_node->url = GURL(kDOMParentURL);
  parent_node->children.push_back(GURL(kDOMChildURL));
  params.push_back(std::move(parent_node));
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(params));

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMChildURL);
  pb_resource->set_parent_id(3);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(2);
  expected.set_complete(false);  // Since the cache was missing.

  HTMLElement* pb_element = expected.add_dom();
  pb_element->set_id(0);
  pb_element->set_tag("IFRAME");
  pb_element->set_resource_id(2);
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDOMChildURL);

  VerifyResults(actual, expected);
}

// Tests creating a threat report when receiving data from multiple renderers.
// We use three layers in this test:
// kDOMParentURL
//  \- <div id=outer>
//    \- <iframe src=kDOMChildURL foo=bar>
//      \- <div id=inner bar=baz/> - div and script are at the same level.
//      \- <script src=kDOMChildURL2>
TEST_F(ThreatDetailsTest, ThreatDOMDetails_MultipleFrames) {
  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  // Define two sets of DOM nodes - one for an outer page containing an iframe,
  // and then another for the inner page containing the contents of that iframe.
  std::vector<mojom::ThreatDOMDetailsNodePtr> outer_params;
  mojom::ThreatDOMDetailsNodePtr outer_child_div =
      mojom::ThreatDOMDetailsNode::New();
  outer_child_div->node_id = 1;
  outer_child_div->child_node_ids.push_back(2);
  outer_child_div->tag_name = "div";
  outer_child_div->parent = GURL(kDOMParentURL);
  outer_child_div->attributes.push_back(
      mojom::AttributeNameValue::New("id", "outer"));
  outer_params.push_back(std::move(outer_child_div));

  mojom::ThreatDOMDetailsNodePtr outer_child_iframe =
      mojom::ThreatDOMDetailsNode::New();
  outer_child_iframe->node_id = 2;
  outer_child_iframe->parent_node_id = 1;
  outer_child_iframe->url = GURL(kDOMChildURL);
  outer_child_iframe->tag_name = "iframe";
  outer_child_iframe->parent = GURL(kDOMParentURL);
  outer_child_iframe->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildURL));
  outer_child_iframe->attributes.push_back(
      mojom::AttributeNameValue::New("foo", "bar"));
  outer_child_iframe->child_frame_routing_id = child_rfh->GetRoutingID();
  outer_params.push_back(std::move(outer_child_iframe));

  mojom::ThreatDOMDetailsNodePtr outer_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_summary_node->url = GURL(kDOMParentURL);
  outer_summary_node->children.push_back(GURL(kDOMChildURL));
  outer_params.push_back(std::move(outer_summary_node));

  // Now define some more nodes for the body of the iframe.
  std::vector<mojom::ThreatDOMDetailsNodePtr> inner_params;
  mojom::ThreatDOMDetailsNodePtr inner_child_div =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_div->node_id = 1;
  inner_child_div->tag_name = "div";
  inner_child_div->parent = GURL(kDOMChildURL);
  inner_child_div->attributes.push_back(
      mojom::AttributeNameValue::New("id", "inner"));
  inner_child_div->attributes.push_back(
      mojom::AttributeNameValue::New("bar", "baz"));
  inner_params.push_back(std::move(inner_child_div));

  mojom::ThreatDOMDetailsNodePtr inner_child_script =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_script->node_id = 2;
  inner_child_script->url = GURL(kDOMChildUrl2);
  inner_child_script->tag_name = "script";
  inner_child_script->parent = GURL(kDOMChildURL);
  inner_child_script->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildUrl2));
  inner_params.push_back(std::move(inner_child_script));

  mojom::ThreatDOMDetailsNodePtr inner_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_summary_node->url = GURL(kDOMChildURL);
  inner_summary_node->children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(std::move(inner_summary_node));

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  ClientSafeBrowsingReportRequest::Resource* res_dom_child =
      expected.add_resources();
  res_dom_child->set_id(2);
  res_dom_child->set_url(kDOMChildURL);
  res_dom_child->set_parent_id(3);
  res_dom_child->add_child_ids(4);

  ClientSafeBrowsingReportRequest::Resource* res_dom_parent =
      expected.add_resources();
  res_dom_parent->set_id(3);
  res_dom_parent->set_url(kDOMParentURL);
  res_dom_parent->add_child_ids(2);

  ClientSafeBrowsingReportRequest::Resource* res_dom_child2 =
      expected.add_resources();
  res_dom_child2->set_id(4);
  res_dom_child2->set_url(kDOMChildUrl2);
  res_dom_child2->set_parent_id(2);

  expected.set_complete(false);  // Since the cache was missing.

  HTMLElement* elem_dom_outer_div = expected.add_dom();
  elem_dom_outer_div->set_id(0);
  elem_dom_outer_div->set_tag("DIV");
  elem_dom_outer_div->add_attribute()->set_name("id");
  elem_dom_outer_div->mutable_attribute(0)->set_value("outer");
  elem_dom_outer_div->add_child_ids(1);

  HTMLElement* elem_dom_outer_iframe = expected.add_dom();
  elem_dom_outer_iframe->set_id(1);
  elem_dom_outer_iframe->set_tag("IFRAME");
  elem_dom_outer_iframe->set_resource_id(res_dom_child->id());
  elem_dom_outer_iframe->add_attribute()->set_name("src");
  elem_dom_outer_iframe->mutable_attribute(0)->set_value(kDOMChildURL);
  elem_dom_outer_iframe->add_attribute()->set_name("foo");
  elem_dom_outer_iframe->mutable_attribute(1)->set_value("bar");
  elem_dom_outer_iframe->add_child_ids(2);
  elem_dom_outer_iframe->add_child_ids(3);

  HTMLElement* elem_dom_inner_div = expected.add_dom();
  elem_dom_inner_div->set_id(2);
  elem_dom_inner_div->set_tag("DIV");
  elem_dom_inner_div->add_attribute()->set_name("id");
  elem_dom_inner_div->mutable_attribute(0)->set_value("inner");
  elem_dom_inner_div->add_attribute()->set_name("bar");
  elem_dom_inner_div->mutable_attribute(1)->set_value("baz");

  HTMLElement* elem_dom_inner_script = expected.add_dom();
  elem_dom_inner_script->set_id(3);
  elem_dom_inner_script->set_tag("SCRIPT");
  elem_dom_inner_script->set_resource_id(res_dom_child2->id());
  elem_dom_inner_script->add_attribute()->set_name("src");
  elem_dom_inner_script->mutable_attribute(0)->set_value(kDOMChildUrl2);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Send both sets of nodes, from different render frames.
  {
    auto report = std::make_unique<ThreatDetailsWrap>(
        ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
        referrer_chain_provider_.get());
    report->StartCollection();

    std::vector<mojom::ThreatDOMDetailsNodePtr> outer_params_copy;
    for (auto& node : outer_params) {
      outer_params_copy.push_back(node.Clone());
    }
    std::vector<mojom::ThreatDOMDetailsNodePtr> inner_params_copy;
    for (auto& node : inner_params) {
      inner_params_copy.push_back(node.Clone());
    }

    // Send both sets of nodes from different render frames.
    report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                       main_rfh(),
                                       std::move(outer_params_copy));
    report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                       child_rfh, std::move(inner_params_copy));

    std::string serialized = WaitForThreatDetailsDone(
        report.get(), false /* did_proceed*/, 0 /* num_visit */);
    ClientSafeBrowsingReportRequest actual;
    actual.ParseFromString(serialized);
    VerifyResults(actual, expected);
  }

  // Try again but with the messages coming in a different order. The IDs change
  // slightly, but everything else remains the same.
  {
    // Adjust the expected IDs: the inner params come first, so InnerScript
    // and InnerDiv appear before DomParent
    res_dom_child2->set_id(2);
    res_dom_child2->set_parent_id(3);
    res_dom_child->set_id(3);
    res_dom_child->set_parent_id(4);
    res_dom_child->clear_child_ids();
    res_dom_child->add_child_ids(2);
    res_dom_parent->set_id(4);
    res_dom_parent->clear_child_ids();
    res_dom_parent->add_child_ids(3);

    // Also adjust the elements - they change order since InnerDiv and
    // InnerScript come in first.
    elem_dom_inner_div->set_id(0);
    elem_dom_inner_script->set_id(1);
    elem_dom_inner_script->set_resource_id(res_dom_child2->id());

    elem_dom_outer_div->set_id(2);
    elem_dom_outer_div->clear_child_ids();
    elem_dom_outer_div->add_child_ids(3);
    elem_dom_outer_iframe->set_id(3);
    elem_dom_outer_iframe->set_resource_id(res_dom_child->id());
    elem_dom_outer_iframe->clear_child_ids();
    elem_dom_outer_iframe->add_child_ids(0);
    elem_dom_outer_iframe->add_child_ids(1);

    auto report = std::make_unique<ThreatDetailsWrap>(
        ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
        referrer_chain_provider_.get());
    report->StartCollection();

    // Send both sets of nodes from different render frames.
    report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                       child_rfh, std::move(inner_params));
    report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                       main_rfh(), std::move(outer_params));

    std::string serialized = WaitForThreatDetailsDone(
        report.get(), false /* did_proceed*/, 0 /* num_visit */);
    ClientSafeBrowsingReportRequest actual;
    actual.ParseFromString(serialized);
    VerifyResults(actual, expected);
  }
}

// Tests an ambiguous DOM, meaning that an inner render frame can not be mapped
// to an iframe element in the parent frame, which is a failure to lookup the
// frames in the frame tree and should not happen.
// We use three layers in this test:
// kDOMParentURL
//   \- <frame src=kDataURL>
//        \- <script src=kDOMChildURL2>
TEST_F(ThreatDetailsTest, ThreatDOMDetails_AmbiguousDOM) {
  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  // Define two sets of DOM nodes - one for an outer page containing a frame,
  // and then another for the inner page containing the contents of that frame.
  std::vector<mojom::ThreatDOMDetailsNodePtr> outer_params;
  mojom::ThreatDOMDetailsNodePtr outer_child_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_child_node->url = GURL(kDataURL);
  outer_child_node->tag_name = "frame";
  outer_child_node->parent = GURL(kDOMParentURL);
  outer_child_node->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDataURL));
  outer_params.push_back(std::move(outer_child_node));
  mojom::ThreatDOMDetailsNodePtr outer_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_summary_node->url = GURL(kDOMParentURL);
  outer_summary_node->children.push_back(GURL(kDataURL));
  // Set |child_frame_routing_id| for this node to something non-sensical so
  // that the child frame lookup fails.
  outer_summary_node->child_frame_routing_id = -100;
  outer_params.push_back(std::move(outer_summary_node));

  // Now define some more nodes for the body of the frame. The URL of this
  // inner frame is "about:blank".
  std::vector<mojom::ThreatDOMDetailsNodePtr> inner_params;
  mojom::ThreatDOMDetailsNodePtr inner_child_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_node->url = GURL(kDOMChildUrl2);
  inner_child_node->tag_name = "script";
  inner_child_node->parent = GURL(kBlankURL);
  inner_child_node->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildUrl2));
  inner_params.push_back(std::move(inner_child_node));
  mojom::ThreatDOMDetailsNodePtr inner_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_summary_node->url = GURL(kBlankURL);
  inner_summary_node->children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(std::move(inner_summary_node));

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kDOMParentURL);
  pb_resource->add_child_ids(3);

  // TODO(lpz): The data URL is added, despite being unreportable, because it
  // is a child of the top-level page. Consider if this should happen.
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kDataURL);

  // This child can't be mapped to its containing iframe so its parent is unset.
  pb_resource = expected.add_resources();
  pb_resource->set_id(4);
  pb_resource->set_url(kDOMChildUrl2);

  expected.set_complete(false);  // Since the cache was missing.

  // This Element represents the Frame with the data URL. It has no resource or
  // children since it couldn't be mapped to anything. It does still contain the
  // src attribute with the data URL set.
  HTMLElement* pb_element = expected.add_dom();
  pb_element->set_id(0);
  pb_element->set_tag("FRAME");
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDataURL);

  pb_element = expected.add_dom();
  pb_element->set_id(1);
  pb_element->set_tag("SCRIPT");
  pb_element->set_resource_id(4);
  pb_element->add_attribute()->set_name("src");
  pb_element->mutable_attribute(0)->set_value(kDOMChildUrl2);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED,
               ThreatSource::PASSWORD_PROTECTION_SERVICE,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  base::HistogramTester histograms;

  // Send both sets of nodes from different render frames.
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(outer_params));
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     child_rfh, std::move(inner_params));
  std::string serialized = WaitForThreatDetailsDone(
      report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);
  VerifyResults(actual, expected);
}

// Tests creating a threat report when receiving data from multiple renderers
// that gets trimmed to just the ad and its contents.
// This test uses the following structure.
// kDOMParentURL
//  \- <div id=outer>  # Trimmed
//  \- <script id=shared-resource src=kFirstRedirectURL>  # Trimmed
//  \- <script id=outer-sibling src=kReferrerURL>  # Reported (parent of ad ID)
//   \- <script id=sibling src=kFirstRedirectURL>  # Reported (sibling of ad ID)
//   \- <div data-google-query-id=ad-tag>  # Reported (ad ID)
//     \- <iframe src=kDOMChildURL foo=bar>  # Reported (child of ad ID)
//       \- <div id=inner bar=baz/>  # Reported (child of ad ID)
//       \- <script src=kDOMChildURL2>  # Reported (child of ad ID)
//
// *Note: the best way to match the inputs and expectations in the body of the
// test with the structure above, is to use URLs for resources, and the ID
// attributes for DOM elements.
TEST_F(ThreatDetailsTest, ThreatDOMDetails_TrimToAdTags) {
  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  // Define two sets of DOM nodes - one for an outer page containing an iframe,
  // and then another for the inner page containing the contents of that iframe.
  std::vector<mojom::ThreatDOMDetailsNodePtr> outer_params;
  mojom::ThreatDOMDetailsNodePtr outer_div = mojom::ThreatDOMDetailsNode::New();
  outer_div->node_id = 1;
  outer_div->tag_name = "div";
  outer_div->parent = GURL(kDOMParentURL);
  outer_div->attributes.push_back(
      mojom::AttributeNameValue::New("id", "outer"));
  outer_params.push_back(std::move(outer_div));

  mojom::ThreatDOMDetailsNodePtr shared_resource_script =
      mojom::ThreatDOMDetailsNode::New();
  shared_resource_script->node_id = 2;
  shared_resource_script->tag_name = "script";
  shared_resource_script->url = GURL(kFirstRedirectURL);
  shared_resource_script->parent = GURL(kDOMParentURL);
  shared_resource_script->attributes.push_back(
      mojom::AttributeNameValue::New("id", "shared-resource"));
  outer_params.push_back(std::move(shared_resource_script));

  mojom::ThreatDOMDetailsNodePtr outer_sibling_script =
      mojom::ThreatDOMDetailsNode::New();
  outer_sibling_script->node_id = 3;
  outer_sibling_script->url = GURL(kReferrerURL);
  outer_sibling_script->child_node_ids.push_back(4);
  outer_sibling_script->child_node_ids.push_back(5);
  outer_sibling_script->tag_name = "script";
  outer_sibling_script->parent = GURL(kDOMParentURL);
  outer_sibling_script->attributes.push_back(
      mojom::AttributeNameValue::New("src", kReferrerURL));
  outer_sibling_script->attributes.push_back(
      mojom::AttributeNameValue::New("id", "outer-sibling"));
  outer_params.push_back(std::move(outer_sibling_script));

  mojom::ThreatDOMDetailsNodePtr sibling_script =
      mojom::ThreatDOMDetailsNode::New();
  sibling_script->node_id = 4;
  sibling_script->url = GURL(kFirstRedirectURL);
  sibling_script->tag_name = "script";
  sibling_script->parent = GURL(kDOMParentURL);
  sibling_script->parent_node_id = 3;
  sibling_script->attributes.push_back(
      mojom::AttributeNameValue::New("src", kFirstRedirectURL));
  sibling_script->attributes.push_back(
      mojom::AttributeNameValue::New("id", "sibling"));
  outer_params.push_back(std::move(sibling_script));

  mojom::ThreatDOMDetailsNodePtr outer_ad_tag_div =
      mojom::ThreatDOMDetailsNode::New();
  outer_ad_tag_div->node_id = 5;
  outer_ad_tag_div->parent_node_id = 3;
  outer_ad_tag_div->child_node_ids.push_back(6);
  outer_ad_tag_div->tag_name = "div";
  outer_ad_tag_div->parent = GURL(kDOMParentURL);
  outer_ad_tag_div->attributes.push_back(
      mojom::AttributeNameValue::New("data-google-query-id", "ad-tag"));
  outer_params.push_back(std::move(outer_ad_tag_div));

  mojom::ThreatDOMDetailsNodePtr outer_child_iframe =
      mojom::ThreatDOMDetailsNode::New();
  outer_child_iframe->node_id = 6;
  outer_child_iframe->parent_node_id = 5;
  outer_child_iframe->url = GURL(kDOMChildURL);
  outer_child_iframe->tag_name = "iframe";
  outer_child_iframe->parent = GURL(kDOMParentURL);
  outer_child_iframe->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildURL));
  outer_child_iframe->attributes.push_back(
      mojom::AttributeNameValue::New("foo", "bar"));
  outer_child_iframe->child_frame_routing_id = child_rfh->GetRoutingID();
  outer_params.push_back(std::move(outer_child_iframe));

  mojom::ThreatDOMDetailsNodePtr outer_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_summary_node->url = GURL(kDOMParentURL);
  outer_summary_node->children.push_back(GURL(kDOMChildURL));
  outer_summary_node->children.push_back(GURL(kReferrerURL));
  outer_summary_node->children.push_back(GURL(kFirstRedirectURL));
  outer_params.push_back(std::move(outer_summary_node));

  // Now define some more nodes for the body of the iframe.
  std::vector<mojom::ThreatDOMDetailsNodePtr> inner_params;
  mojom::ThreatDOMDetailsNodePtr inner_child_div =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_div->node_id = 1;
  inner_child_div->tag_name = "div";
  inner_child_div->parent = GURL(kDOMChildURL);
  inner_child_div->attributes.push_back(
      mojom::AttributeNameValue::New("id", "inner"));
  inner_child_div->attributes.push_back(
      mojom::AttributeNameValue::New("bar", "baz"));
  inner_params.push_back(std::move(inner_child_div));

  mojom::ThreatDOMDetailsNodePtr inner_child_script =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_script->node_id = 2;
  inner_child_script->url = GURL(kDOMChildUrl2);
  inner_child_script->tag_name = "script";
  inner_child_script->parent = GURL(kDOMChildURL);
  inner_child_script->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildUrl2));
  inner_params.push_back(std::move(inner_child_script));

  mojom::ThreatDOMDetailsNodePtr inner_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_summary_node->url = GURL(kDOMChildURL);
  inner_summary_node->children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(std::move(inner_summary_node));

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_UNWANTED);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);
  expected.set_repeat_visit(false);
  expected.set_complete(false);  // Since the cache was missing.

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);

  ClientSafeBrowsingReportRequest::Resource* res_dom_child2 =
      expected.add_resources();
  res_dom_child2->set_id(2);
  res_dom_child2->set_url(kDOMChildUrl2);
  res_dom_child2->set_parent_id(3);
  res_dom_child2->set_tag_name("script");

  ClientSafeBrowsingReportRequest::Resource* res_dom_child =
      expected.add_resources();
  res_dom_child->set_id(3);
  res_dom_child->set_url(kDOMChildURL);
  res_dom_child->set_parent_id(5);
  res_dom_child->add_child_ids(2);
  res_dom_child->set_tag_name("iframe");

  ClientSafeBrowsingReportRequest::Resource* res_ad_parent =
      expected.add_resources();
  res_ad_parent->set_id(6);
  res_ad_parent->set_url(kReferrerURL);
  res_ad_parent->set_parent_id(5);
  res_ad_parent->set_tag_name("script");

  ClientSafeBrowsingReportRequest::Resource* res_sibling =
      expected.add_resources();
  res_sibling->set_id(4);
  res_sibling->set_url(kFirstRedirectURL);
  res_sibling->set_parent_id(5);
  res_sibling->set_tag_name("script");

  ClientSafeBrowsingReportRequest::Resource* res_dom_parent =
      expected.add_resources();
  res_dom_parent->set_id(5);
  res_dom_parent->set_url(kDOMParentURL);
  res_dom_parent->add_child_ids(3);
  res_dom_parent->add_child_ids(6);
  res_dom_parent->add_child_ids(4);

  HTMLElement* elem_dom_parent_script = expected.add_dom();
  elem_dom_parent_script->set_id(4);
  elem_dom_parent_script->set_tag("SCRIPT");
  elem_dom_parent_script->set_resource_id(res_ad_parent->id());
  elem_dom_parent_script->add_attribute()->set_name("src");
  elem_dom_parent_script->mutable_attribute(0)->set_value(kReferrerURL);
  elem_dom_parent_script->add_attribute()->set_name("id");
  elem_dom_parent_script->mutable_attribute(1)->set_value("outer-sibling");
  elem_dom_parent_script->add_child_ids(5);
  elem_dom_parent_script->add_child_ids(6);

  HTMLElement* elem_dom_sibling_script = expected.add_dom();
  elem_dom_sibling_script->set_id(5);
  elem_dom_sibling_script->set_tag("SCRIPT");
  elem_dom_sibling_script->set_resource_id(res_sibling->id());
  elem_dom_sibling_script->add_attribute()->set_name("src");
  elem_dom_sibling_script->mutable_attribute(0)->set_value(kFirstRedirectURL);
  elem_dom_sibling_script->add_attribute()->set_name("id");
  elem_dom_sibling_script->mutable_attribute(1)->set_value("sibling");

  HTMLElement* elem_dom_ad_tag_div = expected.add_dom();
  elem_dom_ad_tag_div->set_id(6);
  elem_dom_ad_tag_div->set_tag("DIV");
  elem_dom_ad_tag_div->add_attribute()->set_name("data-google-query-id");
  elem_dom_ad_tag_div->mutable_attribute(0)->set_value("ad-tag");
  elem_dom_ad_tag_div->add_child_ids(7);

  HTMLElement* elem_dom_outer_iframe = expected.add_dom();
  elem_dom_outer_iframe->set_id(7);
  elem_dom_outer_iframe->set_tag("IFRAME");
  elem_dom_outer_iframe->set_resource_id(res_dom_child->id());
  elem_dom_outer_iframe->add_attribute()->set_name("src");
  elem_dom_outer_iframe->mutable_attribute(0)->set_value(kDOMChildURL);
  elem_dom_outer_iframe->add_attribute()->set_name("foo");
  elem_dom_outer_iframe->mutable_attribute(1)->set_value("bar");
  elem_dom_outer_iframe->add_child_ids(0);
  elem_dom_outer_iframe->add_child_ids(1);

  HTMLElement* elem_dom_inner_div = expected.add_dom();
  elem_dom_inner_div->set_id(0);
  elem_dom_inner_div->set_tag("DIV");
  elem_dom_inner_div->add_attribute()->set_name("id");
  elem_dom_inner_div->mutable_attribute(0)->set_value("inner");
  elem_dom_inner_div->add_attribute()->set_name("bar");
  elem_dom_inner_div->mutable_attribute(1)->set_value("baz");

  HTMLElement* elem_dom_inner_script = expected.add_dom();
  elem_dom_inner_script->set_id(1);
  elem_dom_inner_script->set_tag("SCRIPT");
  elem_dom_inner_script->set_resource_id(res_dom_child2->id());
  elem_dom_inner_script->add_attribute()->set_name("src");
  elem_dom_inner_script->mutable_attribute(0)->set_value(kDOMChildUrl2);

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Send both sets of nodes, from different render frames.
  auto trimmed_report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get(),
      /*trim_to_ad_tags=*/true);
  trimmed_report->StartCollection();

  // Send both sets of nodes from different render frames.
  trimmed_report->OnReceivedThreatDOMDetails(
      mojo::Remote<mojom::ThreatReporter>(), child_rfh,
      std::move(inner_params));
  trimmed_report->OnReceivedThreatDOMDetails(
      mojo::Remote<mojom::ThreatReporter>(), main_rfh(),
      std::move(outer_params));

  std::string serialized = WaitForThreatDetailsDone(
      trimmed_report.get(), false /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);
  VerifyResults(actual, expected);
}

// Tests that an empty report does not get sent. Empty reports can result from
// trying to trim a report to ad tags when no ad tags are present.
// This test uses the following structure.
// kDOMParentURL
//   \- <frame src=kDataURL>
//        \- <script src=kDOMChildURL2>
TEST_F(ThreatDetailsTest, ThreatDOMDetails_EmptyReportNotSent) {
  // Create a child renderer inside the main frame to house the inner iframe.
  // Perform the navigation first in order to manipulate the frame tree.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  // Define two sets of DOM nodes - one for an outer page containing a frame,
  // and then another for the inner page containing the contents of that frame.
  std::vector<mojom::ThreatDOMDetailsNodePtr> outer_params;
  mojom::ThreatDOMDetailsNodePtr outer_child_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_child_node->url = GURL(kDataURL);
  outer_child_node->tag_name = "frame";
  outer_child_node->parent = GURL(kDOMParentURL);
  outer_child_node->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDataURL));
  outer_params.push_back(std::move(outer_child_node));
  mojom::ThreatDOMDetailsNodePtr outer_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  outer_summary_node->url = GURL(kDOMParentURL);
  outer_summary_node->children.push_back(GURL(kDataURL));
  // Set |child_frame_routing_id| for this node to something non-sensical so
  // that the child frame lookup fails.
  outer_summary_node->child_frame_routing_id = -100;
  outer_params.push_back(std::move(outer_summary_node));

  // Now define some more nodes for the body of the frame. The URL of this
  // inner frame is "about:blank".
  std::vector<mojom::ThreatDOMDetailsNodePtr> inner_params;
  mojom::ThreatDOMDetailsNodePtr inner_child_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_child_node->url = GURL(kDOMChildUrl2);
  inner_child_node->tag_name = "script";
  inner_child_node->parent = GURL(kBlankURL);
  inner_child_node->attributes.push_back(
      mojom::AttributeNameValue::New("src", kDOMChildUrl2));
  inner_params.push_back(std::move(inner_child_node));
  mojom::ThreatDOMDetailsNodePtr inner_summary_node =
      mojom::ThreatDOMDetailsNode::New();
  inner_summary_node->url = GURL(kBlankURL);
  inner_summary_node->children.push_back(GURL(kDOMChildUrl2));
  inner_params.push_back(std::move(inner_summary_node));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_UNWANTED, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Send both sets of nodes, from different render frames.
  auto trimmed_report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get(),
      /*trim_to_ad_tags=*/true);
  trimmed_report->StartCollection();

  // Send both sets of nodes from different render frames.
  trimmed_report->OnReceivedThreatDOMDetails(
      mojo::Remote<mojom::ThreatReporter>(), child_rfh,
      std::move(inner_params));
  trimmed_report->OnReceivedThreatDOMDetails(
      mojo::Remote<mojom::ThreatReporter>(), main_rfh(),
      std::move(outer_params));

  std::string serialized = WaitForThreatDetailsDone(
      trimmed_report.get(), false /* did_proceed*/, 0 /* num_visit */);
  EXPECT_FALSE(ReportWasSent());
}

// Tests creating a threat report of a malware page where there are redirect
// urls to an unsafe resource url.
TEST_F(ThreatDetailsTest, ThreatWithRedirectUrl) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::REMOTE,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  resource.original_url = GURL(kOriginalLandingURL);

  // add some redirect urls
  resource.redirect_urls.push_back(GURL(kFirstRedirectURL));
  resource.redirect_urls.push_back(GURL(kSecondRedirectURL));
  resource.redirect_urls.push_back(GURL(kThreatURL));

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 0 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::ANDROID_SAFETYNET);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kOriginalLandingURL);

  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kThreatURL);
  pb_resource->set_parent_id(4);

  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);
  pb_resource->set_parent_id(1);

  pb_resource = expected.add_resources();
  pb_resource->set_id(4);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource->set_parent_id(3);

  VerifyResults(actual, expected);
}

// Test collecting threat details for a blocked main frame load.
TEST_F(ThreatDetailsTest, ThreatOnMainPageLoadBlocked) {
  const char* kUnrelatedReferrerURL =
      "http://www.unrelatedreferrer.com/some/path";
  const char* kUnrelatedURL = "http://www.unrelated.com/some/path";

  // Load and commit an unrelated URL. The ThreatDetails should not use this
  // navigation entry.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kUnrelatedURL), web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      GURL(kUnrelatedReferrerURL), network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  // Start a pending load with a referrer.
  controller().LoadURL(
      GURL(kLandingURL),
      content::Referrer(GURL(kReferrerURL),
                        network::mojom::ReferrerPolicy::kDefault),
      ui::PAGE_TRANSITION_TYPED, std::string());

  // Create UnsafeResource for the pending main page load.
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::UNKNOWN,
               false /* is_subresource */, GURL(kLandingURL), &resource);

  // Start ThreatDetails collection.
  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  // Simulate clicking don't proceed.
  controller().DiscardNonCommittedEntries();

  // Finish ThreatDetails collection.
  std::string serialized = WaitForThreatDetailsDone(
      report.get(), false /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kLandingURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(false);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

// Tests that a pending load does not interfere with collecting threat details
// for the committed page.
TEST_F(ThreatDetailsTest, ThreatWithPendingLoad) {
  const char* kPendingReferrerURL = "http://www.pendingreferrer.com/some/path";
  const char* kPendingURL = "http://www.pending.com/some/path";

  // Load and commit the landing URL with a referrer.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kLandingURL), web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      GURL(kReferrerURL), network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  // Create UnsafeResource for fake sub-resource of landing page.
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::LOCAL_PVER4,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Start a pending load before creating ThreatDetails.
  controller().LoadURL(
      GURL(kPendingURL),
      content::Referrer(GURL(kPendingReferrerURL),
                        network::mojom::ReferrerPolicy::kDefault),
      ui::PAGE_TRANSITION_TYPED, std::string());

  // Do ThreatDetails collection.
  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER4_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  // Note that the referrer policy is not actually enacted here, since that's
  // done in Blink.
  expected.set_referrer_url(kReferrerURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_url(kReferrerURL);

  VerifyResults(actual, expected);
}

TEST_F(ThreatDetailsTest, ThreatOnFreshTab) {
  // A fresh WebContents should not have any NavigationEntries yet. (See
  // https://crbug.com/524208.)
  EXPECT_EQ(nullptr, controller().GetLastCommittedEntry());
  EXPECT_EQ(nullptr, controller().GetPendingEntry());

  // Simulate a subresource malware hit (this could happen if the WebContents
  // was created with window.open, and had content injected into it).
  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::CLIENT_SIDE_DETECTION,
               true /* is_subresource */, GURL(kThreatURL), &resource);

  // Do ThreatDetails collection.
  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);

  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.set_url(kThreatURL);
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kThreatURL);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache.
TEST_F(ThreatDetailsTest, HTTPCache) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
               ThreatSource::CLIENT_SIDE_DETECTION, true /* is_subresource */,
               GURL(kThreatURL), &resource);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, test_shared_loader_factory_,
      history_service(), referrer_chain_provider_.get());
  report->StartCollection();

  SimulateFillCache(kThreatURL);

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<mojom::ThreatDOMDetailsNodePtr> params;
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(params));

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
      pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("text/html");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Length");
  pb_header->set_value("1024");
  pb_response->set_body(kLandingData);
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  pb_response = pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Some-Other-Header");
  pb_header->set_value("foo");
  pb_response->set_body(kThreatData);
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test that only some fields of the HTTPS resource (eg: whitelisted headers)
// are reported.
TEST_F(ThreatDetailsTest, HttpsResourceSanitization) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
               ThreatSource::CLIENT_SIDE_DETECTION, true /* is_subresource */,
               GURL(kThreatURLHttps), &resource);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, test_shared_loader_factory_,
      history_service(), referrer_chain_provider_.get());
  report->StartCollection();

  SimulateFillCache(kThreatURLHttps);

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<mojom::ThreatDOMDetailsNodePtr> params;
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(params));

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  expected.set_url(kThreatURLHttps);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(200);
  ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
      pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("text/html");
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Length");
  pb_header->set_value("1024");
  pb_response->set_body(kLandingData);
  std::string landing_data(kLandingData);
  pb_response->set_bodylength(landing_data.size());
  pb_response->set_bodydigest(base::MD5String(landing_data));
  pb_response->set_remote_ip("1.2.3.4:80");

  // The threat URL is HTTP so the request and response are cleared (except for
  // whitelisted headers and certain safe fields). Namely the firstline and body
  // are missing.
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURLHttps);
  pb_response = pb_resource->mutable_response();
  pb_header = pb_response->add_headers();
  pb_header->set_name("Content-Type");
  pb_header->set_value("image/jpeg");
  std::string threat_data(kThreatData);
  pb_response->set_bodylength(threat_data.size());
  pb_response->set_bodydigest(base::MD5String(threat_data));
  pb_response->set_remote_ip("1.2.3.4:80");
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Tests the interaction with the HTTP cache (where the cache is empty).
TEST_F(ThreatDetailsTest, HTTPCacheNoEntries) {
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE,
               ThreatSource::LOCAL_PVER3, true /* is_subresource */,
               GURL(kThreatURL), &resource);

  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, test_shared_loader_factory_,
      history_service(), referrer_chain_provider_.get());
  report->StartCollection();

  // Simulate no cache entry found.
  test_url_loader_factory_.AddResponse(
      GURL(kThreatURL), network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_CACHE_MISS));
  test_url_loader_factory_.AddResponse(
      GURL(kLandingURL), network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_CACHE_MISS));

  // The cache collection starts after the IPC from the DOM is fired.
  std::vector<mojom::ThreatDOMDetailsNodePtr> params;
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(params));

  // Let the cache callbacks complete.
  base::RunLoop().RunUntilIdle();

  DVLOG(1) << "Getting serialized report";
  std::string serialized = WaitForThreatDetailsDone(
      report.get(), false /* did_proceed*/, -1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(false);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_url(kThreatURL);
  expected.set_complete(true);

  VerifyResults(actual, expected);
}

// Test getting redirects from history service.
TEST_F(ThreatDetailsTest, HistoryServiceUrls) {
  // Add content to history service.
  // There are two redirect urls before reacing malware url:
  // kFirstRedirectURL -> kSecondRedirectURL -> kThreatURL
  GURL baseurl(kThreatURL);
  history::RedirectList redirects;
  redirects.push_back(GURL(kFirstRedirectURL));
  redirects.push_back(GURL(kSecondRedirectURL));
  AddPageToHistory(baseurl, &redirects);
  // Wait for history service operation finished.
  profile()->BlockUntilHistoryProcessesPendingRequests();

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kLandingURL));

  UnsafeResource resource;
  InitResource(SB_THREAT_TYPE_URL_MALWARE, ThreatSource::LOCAL_PVER3,
               true /* is_subresource */, GURL(kThreatURL), &resource);
  auto report = std::make_unique<ThreatDetailsWrap>(
      ui_manager_.get(), web_contents(), resource, nullptr, history_service(),
      referrer_chain_provider_.get());
  report->StartCollection();

  // The redirects collection starts after the IPC from the DOM is fired.
  std::vector<mojom::ThreatDOMDetailsNodePtr> params;
  report->OnReceivedThreatDOMDetails(mojo::Remote<mojom::ThreatReporter>(),
                                     main_rfh(), std::move(params));

  // Let the redirects callbacks complete.
  base::RunLoop().RunUntilIdle();

  std::string serialized = WaitForThreatDetailsDone(
      report.get(), true /* did_proceed*/, 1 /* num_visit */);
  ClientSafeBrowsingReportRequest actual;
  actual.ParseFromString(serialized);

  ClientSafeBrowsingReportRequest expected;
  expected.set_type(ClientSafeBrowsingReportRequest::URL_MALWARE);
  expected.mutable_client_properties()->set_url_api_type(
      ClientSafeBrowsingReportRequest::PVER3_NATIVE);
  expected.set_url(kThreatURL);
  expected.set_page_url(kLandingURL);
  expected.set_referrer_url("");
  expected.set_did_proceed(true);
  expected.set_repeat_visit(true);

  ClientSafeBrowsingReportRequest::Resource* pb_resource =
      expected.add_resources();
  pb_resource->set_id(0);
  pb_resource->set_url(kLandingURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(1);
  pb_resource->set_parent_id(2);
  pb_resource->set_url(kThreatURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(2);
  pb_resource->set_parent_id(3);
  pb_resource->set_url(kSecondRedirectURL);
  pb_resource = expected.add_resources();
  pb_resource->set_id(3);
  pb_resource->set_url(kFirstRedirectURL);

  VerifyResults(actual, expected);
}

}  // namespace safe_browsing
