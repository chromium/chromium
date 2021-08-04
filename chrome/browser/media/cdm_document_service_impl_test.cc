// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace {
// copied from cdm_pref_service_helper.cc for testing
const char kOriginId[] = "origin_id";
}  // namespace

namespace content {

const char kTestOrigin[] = "https://foo.bar";
const char kTestOrigin2[] = "https://bar.foo";

using GetCdmPreferenceDataMockCB =
    base::MockOnceCallback<void(std::unique_ptr<media::CdmPreferenceData>)>;

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

  std::unique_ptr<media::CdmPreferenceData> GetCdmPreferenceData() {
    std::unique_ptr<media::CdmPreferenceData> cdm_preference_data;
    GetCdmPreferenceDataMockCB mock_cb;
    EXPECT_CALL(mock_cb, Run(_))
        .WillOnce([&cdm_preference_data](
                      std::unique_ptr<media::CdmPreferenceData> ptr) {
          cdm_preference_data = std::move(ptr);
        });

    cdm_document_service_->GetCdmPreferenceData(mock_cb.Get());
    base::RunLoop().RunUntilIdle();

    return cdm_preference_data;
  }

  void SetCdmClientToken(const std::vector<uint8_t>& client_token) {
    cdm_document_service_->SetCdmClientToken(client_token);
    base::RunLoop().RunUntilIdle();
  }

  void CorruptCdmPreference() {
    PrefService* user_prefs = user_prefs::UserPrefs::Get(
        web_contents()->GetMainFrame()->GetBrowserContext());

    // Create (or overwrite) an entry with only an origin id to simulate some
    // kind of corruption or simply an update to the preference format.
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetKey(kOriginId, base::UnguessableTokenToValue(
                                base::UnguessableToken::Create()));

    DictionaryPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    base::DictionaryValue* dict = update.Get();
    const std::string serialized_origin =
        web_contents()->GetMainFrame()->GetLastCommittedOrigin().Serialize();
    dict->SetKey(serialized_origin, std::move(entry));
  }

 protected:
  mojo::Remote<media::mojom::CdmDocumentService> cdm_document_service_;
};

// Verify that we get a non null origin id.
TEST_F(CdmDocumentServiceImplTest, GetOriginId) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto pref_data = GetCdmPreferenceData();
  ASSERT_FALSE(pref_data->origin_id.is_empty());
}

// Verify that we get a non null and different origin id if the preference gets
// corrupted.
TEST_F(CdmDocumentServiceImplTest, GetOriginIdAfterCorruption) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto pref_data_before = GetCdmPreferenceData();

  CorruptCdmPreference();
  auto pref_data_after = GetCdmPreferenceData();
  ASSERT_FALSE(pref_data_after->origin_id.is_empty());
  ASSERT_NE(pref_data_before->origin_id, pref_data_after->origin_id);
}

// Verify that we can correctly get an existing origin id.
TEST_F(CdmDocumentServiceImplTest, GetSameOriginId) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id1 = GetCdmPreferenceData()->origin_id;

  // Create an unrelated origin id
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  base::UnguessableToken origin_id2 = GetCdmPreferenceData()->origin_id;

  // Get the origin id for the first origin
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id3 = GetCdmPreferenceData()->origin_id;

  ASSERT_NE(origin_id2, origin_id1);
  ASSERT_EQ(origin_id1, origin_id3);
}

TEST_F(CdmDocumentServiceImplTest, GetNullClientToken) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto cdm_preference_data = GetCdmPreferenceData();

  ASSERT_FALSE(cdm_preference_data->client_token);
}

TEST_F(CdmDocumentServiceImplTest, SetClientToken) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  // Call GetCdmPreferenceData to create the origin id first, otherwise
  // `SetCdmClientToken()` will assume the preference data associated with the
  // origin was recently cleared and will not save the client token.
  ignore_result(GetCdmPreferenceData());

  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  auto cdm_preference_data = GetCdmPreferenceData();

  ASSERT_EQ(cdm_preference_data->client_token, expected_client_token);
}

// Sets a client token for one origin and check that we get the same
// client token after navigating back to that origin.
TEST_F(CdmDocumentServiceImplTest, GetSameClientToken) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));
  const auto kOtherOrigin = url::Origin::Create(GURL(kTestOrigin2));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  // Call GetCdmPreferenceData to create the origin id first, otherwise
  // `SetCdmClientToken()` will assume the preference data associated with the
  // origin was recently cleared and will not save the client token.
  ignore_result(GetCdmPreferenceData());
  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  ignore_result(GetCdmPreferenceData());
  SetCdmClientToken({1, 2, 3, 4, 5});

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto cdm_preference_data = GetCdmPreferenceData();

  ASSERT_EQ(cdm_preference_data->client_token, expected_client_token);
}

// If an entry cannot be parsed correctly, `SetCdmClientToken` should simply
// remove that entry and return without saving the client token.
TEST_F(CdmDocumentServiceImplTest, SetClientTokenAfterCorruption) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  ignore_result(GetCdmPreferenceData());
  CorruptCdmPreference();

  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  auto cdm_preference_data = GetCdmPreferenceData();
  ASSERT_FALSE(cdm_preference_data->client_token.has_value());
}

}  // namespace content
