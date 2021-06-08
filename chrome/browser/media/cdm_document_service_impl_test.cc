// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace content {

const char kTestOrigin[] = "https://foo.bar";
const char kTestOrigin2[] = "https://bar.foo";

using GetCdmOriginIdMockCB =
    base::MockOnceCallback<void(const base::UnguessableToken&)>;

class CdmDocumentServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  void NavigateToUrlAndCreateCdmDocumentService(GURL url) {
    // The lifetime of `cdm_document_service_` is tied to the lifetime of the
    // Frame. When changing URL we need to unbind `cdm_document_service_` before
    // we can bind it to the new frame.
    if (cdm_document_service_.is_bound())
      ASSERT_TRUE(cdm_document_service_.Unbind());
    NavigateAndCommit(url);
    CdmDocumentServiceImpl::Create(
        web_contents()->GetMainFrame(),
        cdm_document_service_.BindNewPipeAndPassReceiver());
  }

  const base::UnguessableToken GetCdmOriginId() {
    base::UnguessableToken origin_id;
    GetCdmOriginIdMockCB mock_cb;
    EXPECT_CALL(mock_cb, Run(_)).WillOnce(SaveArg<0>(&origin_id));

    cdm_document_service_->GetCdmOriginId(mock_cb.Get());
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(origin_id.is_empty());
    return origin_id;
  }

 protected:
  mojo::Remote<media::mojom::CdmDocumentService> cdm_document_service_;
};

// Verify that we get a non null origin id.
TEST_F(CdmDocumentServiceImplTest, GetOriginId) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  ignore_result(GetCdmOriginId());
}

// Verify that we can correctly get an existing origin id.
TEST_F(CdmDocumentServiceImplTest, GetSameOriginId) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));
  const auto kOtherOrigin = url::Origin::Create(GURL(kTestOrigin2));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id1 = GetCdmOriginId();

  // Create an unrelated origin id
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  base::UnguessableToken origin_id2 = GetCdmOriginId();

  // Get the origin id for the first origin
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id3 = GetCdmOriginId();

  ASSERT_NE(origin_id2, origin_id1);
  ASSERT_EQ(origin_id1, origin_id3);
}

}  // namespace content
