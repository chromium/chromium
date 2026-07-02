// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/network_hints_handler_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/network_hints/common/network_hints.mojom.h"
#include "content/public/browser/preconnect_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace predictors {

using ::testing::_;
using ::testing::Pointee;
using ::testing::StrictMock;

namespace {

class MockPreconnectManager : public content::PreconnectManager {
 public:
  MockPreconnectManager() = default;
  ~MockPreconnectManager() override = default;

  MOCK_METHOD(void,
              Start,
              (const GURL&,
               std::vector<content::PreconnectRequest>,
               net::NetworkTrafficAnnotationTag),
              (override));
  MOCK_METHOD(void,
              StartPreresolveHost,
              (const GURL&,
               const net::NetworkAnonymizationKey&,
               net::NetworkTrafficAnnotationTag,
               const content::StoragePartitionConfig*,
               const base::UnguessableToken&),
              (override));
  MOCK_METHOD(void,
              StartPreresolveHosts,
              (const std::vector<GURL>&,
               const net::NetworkAnonymizationKey&,
               net::NetworkTrafficAnnotationTag,
               const content::StoragePartitionConfig*,
               const base::UnguessableToken&),
              (override));
  MOCK_METHOD(
      void,
      StartPreconnectUrl,
      (const GURL&,
       bool,
       net::NetworkAnonymizationKey,
       net::NetworkTrafficAnnotationTag,
       const content::StoragePartitionConfig*,
       const base::UnguessableToken&,
       std::optional<net::ConnectionKeepAliveConfig>,
       mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>),
      (override));
  MOCK_METHOD(void, Stop, (const GURL&), (override));
  MOCK_METHOD(void,
              SetNetworkContextForTesting,
              (network::mojom::NetworkContext*),
              (override));
  MOCK_METHOD(void, SetObserverForTesting, (Observer*), (override));

  base::WeakPtr<content::PreconnectManager> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<content::PreconnectManager> weak_ptr_factory_{this};
};

}  // namespace

class NetworkHintsHandlerImplTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto* loading_predictor = LoadingPredictorFactory::GetForProfile(profile());
    ASSERT_TRUE(loading_predictor);
    auto mock_preconnect_manager =
        std::make_unique<StrictMock<MockPreconnectManager>>();
    mock_preconnect_manager_ = mock_preconnect_manager.get();
    loading_predictor->set_mock_preconnect_manager(
        std::move(mock_preconnect_manager));
  }

  void TearDown() override {
    mock_preconnect_manager_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  mojo::Remote<network_hints::mojom::NetworkHintsHandler> CreateHandlerForFrame(
      content::RenderFrameHost* frame) {
    mojo::Remote<network_hints::mojom::NetworkHintsHandler> remote;
    NetworkHintsHandlerImpl::Create(frame, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  std::unique_ptr<content::WebContents> CreateGuestWebContents(
      const content::StoragePartitionConfig& config) {
    scoped_refptr<content::SiteInstance> guest_instance =
        content::SiteInstance::CreateForGuest(profile(), config);
    std::unique_ptr<content::WebContents> guest_contents =
        content::WebContentsTester::CreateTestWebContents(profile(),
                                                          guest_instance);
    content::WebContentsTester::For(guest_contents.get())
        ->NavigateAndCommit(GURL("https://example.com/"));
    return guest_contents;
  }

 protected:
  raw_ptr<StrictMock<MockPreconnectManager>> mock_preconnect_manager_ = nullptr;
};

TEST_F(NetworkHintsHandlerImplTest, PreconnectUsesFrameStoragePartition) {
  const content::StoragePartitionConfig kGuestConfig =
      content::StoragePartitionConfig::Create(profile(), "test_domain",
                                              "test_name", /*in_memory=*/true);
  std::unique_ptr<content::WebContents> guest_contents =
      CreateGuestWebContents(kGuestConfig);
  content::RenderFrameHost* frame = guest_contents->GetPrimaryMainFrame();
  ASSERT_EQ(kGuestConfig, frame->GetStoragePartition()->GetConfig());

  auto handler = CreateHandlerForFrame(frame);

  const url::SchemeHostPort kTarget(GURL("https://target.example/"));
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreconnectUrl(kTarget.GetURL(), true, _, _,
                                 Pointee(kGuestConfig), _, _, _));
  handler->Preconnect(kTarget, /*allow_credentials=*/true);
  handler.FlushForTesting();
}

TEST_F(NetworkHintsHandlerImplTest, PrefetchDNSUsesFrameStoragePartition) {
  const content::StoragePartitionConfig kGuestConfig =
      content::StoragePartitionConfig::Create(profile(), "test_domain",
                                              "test_name", /*in_memory=*/true);
  std::unique_ptr<content::WebContents> guest_contents =
      CreateGuestWebContents(kGuestConfig);
  content::RenderFrameHost* frame = guest_contents->GetPrimaryMainFrame();
  ASSERT_EQ(kGuestConfig, frame->GetStoragePartition()->GetConfig());

  auto handler = CreateHandlerForFrame(frame);

  const url::SchemeHostPort kTarget(GURL("https://target.example/"));
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreresolveHosts(std::vector<GURL>{kTarget.GetURL()}, _, _,
                                   Pointee(kGuestConfig), _));
  handler->PrefetchDNS({kTarget});
  handler.FlushForTesting();
}

TEST_F(NetworkHintsHandlerImplTest,
       PreconnectUsesDefaultStoragePartitionForDefaultFrame) {
  content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
  const content::StoragePartitionConfig& expected_config =
      frame->GetStoragePartition()->GetConfig();
  ASSERT_TRUE(expected_config.is_default());

  auto handler = CreateHandlerForFrame(frame);

  const url::SchemeHostPort kTarget(GURL("https://target.example/"));
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreconnectUrl(kTarget.GetURL(), false, _, _,
                                 Pointee(expected_config), _, _, _));
  handler->Preconnect(kTarget, /*allow_credentials=*/false);
  handler.FlushForTesting();
}

}  // namespace predictors
