// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <memory>
#include <tuple>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
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

base::FilePath CreateDummyCdmDataFile(const base::FilePath& cdm_store_path_root,
                                      const base::UnguessableToken& origin_id) {
  // Create a fake CDM file
  auto cdm_store_path = cdm_store_path_root.AppendASCII(origin_id.ToString());
  base::CreateDirectory(cdm_store_path);
  auto cdm_data_file_path = cdm_store_path.AppendASCII("cdm_data_file.txt");
  base::File file(cdm_data_file_path,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  return cdm_data_file_path;
}
}  // namespace

namespace content {

const char kTestOrigin[] = "https://foo.bar";
const char kTestOrigin2[] = "https://bar.foo";

using GetMediaFoundationCdmDataMockCB = base::MockOnceCallback<void(
    std::unique_ptr<media::MediaFoundationCdmData>)>;

class CdmDocumentServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // The Media Foundation CDM depends on functionalities only available in
    // Windows 10 and newer versions.
    if (!media::MediaFoundationCdm::IsAvailable()) {
      GTEST_SKIP() << "skipping all test for this fixture when not running on "
                      "Windows 10.";
    }

    // Set up a testing profile manager.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
  }

  void NavigateToUrlAndCreateCdmDocumentService(GURL url) {
    // The lifetime of `cdm_document_service_` is tied to the lifetime of the
    // Frame. When changing URL we need to unbind `cdm_document_service_` before
    // we can bind it to the new frame.
    if (cdm_document_service_.is_bound())
      ASSERT_TRUE(cdm_document_service_.Unbind());
    NavigateAndCommit(url);
    CdmDocumentServiceImpl::Create(
        web_contents()->GetPrimaryMainFrame(),
        cdm_document_service_.BindNewPipeAndPassReceiver());
  }

  std::unique_ptr<media::MediaFoundationCdmData> GetMediaFoundationCdmData() {
    std::unique_ptr<media::MediaFoundationCdmData> media_foundation_cdm_data;
    GetMediaFoundationCdmDataMockCB mock_cb;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_cb, Run(_))
        .WillOnce([&media_foundation_cdm_data, &run_loop](
                      std::unique_ptr<media::MediaFoundationCdmData> ptr) {
          media_foundation_cdm_data = std::move(ptr);
          run_loop.Quit();
        });

    cdm_document_service_->GetMediaFoundationCdmData(mock_cb.Get());
    run_loop.Run();

    return media_foundation_cdm_data;
  }

  void SetCdmClientToken(const std::vector<uint8_t>& client_token) {
    cdm_document_service_->SetCdmClientToken(client_token);
    base::RunLoop().RunUntilIdle();
  }

  void CorruptCdmPreference() {
    PrefService* user_prefs = profile()->GetPrefs();

    // Create (or overwrite) an entry with only an origin id to simulate some
    // kind of corruption or simply an update to the preference format.
    auto entry = base::Value::Dict().Set(
        kOriginId,
        base::UnguessableTokenToValue(base::UnguessableToken::Create()));

    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    base::Value::Dict& dict = update.Get();
    const std::string serialized_origin = web_contents()
                                              ->GetPrimaryMainFrame()
                                              ->GetLastCommittedOrigin()
                                              .Serialize();
    dict.Set(serialized_origin, std::move(entry));
  }

 protected:
  mojo::Remote<media::mojom::CdmDocumentService> cdm_document_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

// Verify that we get a non null origin id.
TEST_F(CdmDocumentServiceImplTest, GetOriginId) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto data = GetMediaFoundationCdmData();
  ASSERT_FALSE(data->origin_id.is_empty());
}

// Verify that we get a non null and different origin id if the preference gets
// corrupted.
TEST_F(CdmDocumentServiceImplTest, GetOriginIdAfterCorruption) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto data_before = GetMediaFoundationCdmData();

  CorruptCdmPreference();
  auto data_after = GetMediaFoundationCdmData();
  ASSERT_FALSE(data_after->origin_id.is_empty());
  ASSERT_NE(data_before->origin_id, data_after->origin_id);
}

// Verify that we can correctly get an existing origin id.
TEST_F(CdmDocumentServiceImplTest, GetSameOriginId) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id1 = GetMediaFoundationCdmData()->origin_id;

  // Create an unrelated origin id
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  base::UnguessableToken origin_id2 = GetMediaFoundationCdmData()->origin_id;

  // Get the origin id for the first origin
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id3 = GetMediaFoundationCdmData()->origin_id;

  ASSERT_NE(origin_id2, origin_id1);
  ASSERT_EQ(origin_id1, origin_id3);
}

TEST_F(CdmDocumentServiceImplTest, GetNullClientToken) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto media_foundation_cdm_data = GetMediaFoundationCdmData();

  ASSERT_FALSE(media_foundation_cdm_data->client_token);
}

TEST_F(CdmDocumentServiceImplTest, SetClientToken) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  // Call GetMediaFoundationCdmData to create the origin id first, otherwise
  // `SetCdmClientToken()` will assume the preference data associated with the
  // origin was recently cleared and will not save the client token.
  std::ignore = GetMediaFoundationCdmData();

  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  auto media_foundation_cdm_data = GetMediaFoundationCdmData();

  ASSERT_EQ(media_foundation_cdm_data->client_token, expected_client_token);
}

// Sets a client token for one origin and check that we get the same
// client token after navigating back to that origin.
TEST_F(CdmDocumentServiceImplTest, GetSameClientToken) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));
  const auto kOtherOrigin = url::Origin::Create(GURL(kTestOrigin2));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  // Call GetMediaFoundationCdmData to create the origin id first, otherwise
  // `SetCdmClientToken()` will assume the preference data associated with the
  // origin was recently cleared and will not save the client token.
  std::ignore = GetMediaFoundationCdmData();
  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  std::ignore = GetMediaFoundationCdmData();
  SetCdmClientToken({1, 2, 3, 4, 5});

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto media_foundation_cdm_data = GetMediaFoundationCdmData();

  ASSERT_EQ(media_foundation_cdm_data->client_token, expected_client_token);
}

// If an entry cannot be parsed correctly, `SetCdmClientToken` should simply
// remove that entry and return without saving the client token.
TEST_F(CdmDocumentServiceImplTest, SetClientTokenAfterCorruption) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  std::ignore = GetMediaFoundationCdmData();
  CorruptCdmPreference();

  std::vector<uint8_t> expected_client_token = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  SetCdmClientToken(expected_client_token);

  auto media_foundation_cdm_data = GetMediaFoundationCdmData();
  ASSERT_FALSE(media_foundation_cdm_data->client_token.has_value());
}

// Check that we can clear the CDM preferences. `GetMediaFoundationCdmData()`
// should return a new origin_id after the clearing operation.
TEST_F(CdmDocumentServiceImplTest, ClearCdmPreferenceData) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto cdm_data = GetMediaFoundationCdmData();
  base::UnguessableToken origin_id = cdm_data->origin_id;

  base::FilePath cdm_data_file_path = CreateDummyCdmDataFile(
      cdm_data->cdm_store_path_root, cdm_data->origin_id);

  base::Time start = base::Time::Now() - base::Hours(1);
  base::Time end;  // null time

  base::RunLoop loop1;

  // With the filter returning false, the origin id should not be destroyed.
  CdmDocumentServiceImpl::ClearCdmData(
      profile(), start, end,
      base::BindRepeating([](const GURL& url) { return false; }),
      loop1.QuitClosure());

  loop1.Run();
  base::UnguessableToken same_origin_id =
      GetMediaFoundationCdmData()->origin_id;
  ASSERT_EQ(origin_id, same_origin_id);
  ASSERT_TRUE(base::PathExists(cdm_data_file_path));

  base::RunLoop loop2;

  CdmDocumentServiceImpl::ClearCdmData(
      profile(), start, end,
      base::BindRepeating([](const GURL& url) { return true; }),
      loop2.QuitClosure());

  loop2.Run();

  base::UnguessableToken new_origin_id = GetMediaFoundationCdmData()->origin_id;
  ASSERT_NE(origin_id, new_origin_id);
  ASSERT_FALSE(base::PathExists(cdm_data_file_path));
}

TEST_F(CdmDocumentServiceImplTest, ClearCdmPreferenceDataAfterCorruption) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto cdm_data = GetMediaFoundationCdmData();
  base::UnguessableToken origin_id = cdm_data->origin_id;

  base::FilePath cdm_data_file_path = CreateDummyCdmDataFile(
      cdm_data->cdm_store_path_root, cdm_data->origin_id);

  CorruptCdmPreference();

  base::UnguessableToken new_origin_id = GetMediaFoundationCdmData()->origin_id;
  ASSERT_NE(origin_id, new_origin_id);

  // Path should still exist even though prefs were corrupted.
  ASSERT_TRUE(base::PathExists(cdm_data_file_path));

  base::Time start = base::Time::Now() - base::Hours(1);
  base::Time end;  // null time

  base::RunLoop loop1;

  // With the filter returning true, the path should no longer exist.
  CdmDocumentServiceImpl::ClearCdmData(
      profile(), start, end,
      base::BindRepeating([](const GURL& url) { return true; }),
      loop1.QuitClosure());

  loop1.Run();

  // Path should no longer exist
  ASSERT_FALSE(base::PathExists(cdm_data_file_path));
}

// Check that we only clear the CDM preference that were set between start and
// end.
TEST_F(CdmDocumentServiceImplTest, ClearCdmPreferenceDataWrongTime) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto cdm_data = GetMediaFoundationCdmData();
  base::UnguessableToken origin_id = cdm_data->origin_id;
  base::FilePath cdm_data_file_path =
      CreateDummyCdmDataFile(cdm_data->cdm_store_path_root, origin_id);

  base::Time start = base::Time::Now() - base::Hours(4);
  base::Time end = start - base::Hours(2);

  auto null_filter = base::RepeatingCallback<bool(const GURL&)>();

  base::RunLoop loop;

  CdmDocumentServiceImpl::ClearCdmData(profile(), start, end, null_filter,
                                       loop.QuitClosure());

  loop.Run();

  base::UnguessableToken new_origin_id = GetMediaFoundationCdmData()->origin_id;
  ASSERT_EQ(origin_id, new_origin_id);
  ASSERT_TRUE(base::PathExists(cdm_data_file_path));
}

TEST_F(CdmDocumentServiceImplTest, ClearCdmPreferenceDataNullFilter) {
  const auto kOrigin = url::Origin::Create(GURL(kTestOrigin));

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  base::UnguessableToken origin_id_1 = GetMediaFoundationCdmData()->origin_id;

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin2));
  base::UnguessableToken origin_id_2 = GetMediaFoundationCdmData()->origin_id;

  base::Time start = base::Time::Now() - base::Hours(1);
  base::Time end;  // null time

  auto null_filter = base::RepeatingCallback<bool(const GURL&)>();

  base::RunLoop loop;

  CdmDocumentServiceImpl::ClearCdmData(profile(), start, end, null_filter,
                                       loop.QuitClosure());

  loop.Run();

  base::UnguessableToken new_origin_id = GetMediaFoundationCdmData()->origin_id;
  ASSERT_NE(origin_id_2, new_origin_id);

  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  new_origin_id = GetMediaFoundationCdmData()->origin_id;
  ASSERT_NE(origin_id_1, new_origin_id);
}

}  // namespace content
