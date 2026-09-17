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

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>

#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "sandbox/policy/win/lpac_capability.h"
#endif  // BUILDFLAG(IS_WIN)

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

  void CorruptCdmPreference() {
    PrefService* user_prefs = profile()->GetPrefs();

    // Create (or overwrite) an entry with only an origin id to simulate some
    // kind of corruption or simply an update to the preference format.
    auto entry = base::DictValue().Set(
        kOriginId,
        base::UnguessableTokenToValue(base::UnguessableToken::Create()));

    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    base::DictValue& dict = update.Get();
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

// Check that obsolete client tokens are cleanly stripped by
// MigrateObsoleteProfilePrefs() while other origin data is preserved.
TEST_F(CdmDocumentServiceImplTest, MigrateObsoleteClientToken) {
  PrefService* user_prefs = profile()->GetPrefs();

  const auto origin_id = base::UnguessableToken::Create();
  const auto creation_time = base::Time::Now();
  const std::string origin_str = "https://example.com";

  const auto clean_origin_id = base::UnguessableToken::Create();
  const std::string clean_origin_str = "https://other.com";

  // Populate an entry containing valid origin data alongside obsolete client
  // token data, and another entry that has no client token data.
  {
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    base::DictValue entry;
    entry.Set(kOriginId, base::UnguessableTokenToValue(origin_id));
    entry.Set("origin_id_creation_time", base::TimeToValue(creation_time));
    entry.Set(prefs::kHardwareSecureDecryptionDisabledTimes, base::ListValue());
    entry.Set("client_token", "dGVzdF90b2tlbg==");
    entry.Set("client_token_creation_time", base::TimeToValue(creation_time));
    update->Set(origin_str, std::move(entry));

    base::DictValue clean_entry;
    clean_entry.Set(kOriginId, base::UnguessableTokenToValue(clean_origin_id));
    clean_entry.Set("origin_id_creation_time",
                    base::TimeToValue(creation_time));
    clean_entry.Set(prefs::kHardwareSecureDecryptionDisabledTimes,
                    base::ListValue());
    update->Set(clean_origin_str, std::move(clean_entry));
  }

  // Verify the client token keys are present before migration.
  {
    const base::DictValue& dict =
        user_prefs->GetDict(prefs::kMediaCdmOriginData);
    const base::DictValue* origin_dict = dict.FindDict(origin_str);
    ASSERT_TRUE(origin_dict);
    EXPECT_TRUE(origin_dict->Find("client_token"));
    EXPECT_TRUE(origin_dict->Find("client_token_creation_time"));
  }

  // Run the migration.
  CdmPrefServiceHelper::MigrateObsoleteProfilePrefs(user_prefs);

  // Verify client token keys are removed, but origin_id and creation time
  // remain for the migrated origin.
  {
    const base::DictValue& dict =
        user_prefs->GetDict(prefs::kMediaCdmOriginData);
    const base::DictValue* origin_dict = dict.FindDict(origin_str);
    ASSERT_TRUE(origin_dict);
    EXPECT_FALSE(origin_dict->Find("client_token"));
    EXPECT_FALSE(origin_dict->Find("client_token_creation_time"));
    EXPECT_EQ(base::ValueToUnguessableToken(*origin_dict->Find(kOriginId)),
              origin_id);
    EXPECT_EQ(base::ValueToTime(origin_dict->Find("origin_id_creation_time")),
              creation_time);

    // The clean origin remains intact.
    const base::DictValue* clean_dict = dict.FindDict(clean_origin_str);
    ASSERT_TRUE(clean_dict);
    EXPECT_EQ(base::ValueToUnguessableToken(*clean_dict->Find(kOriginId)),
              clean_origin_id);
  }

  // Verify that CdmDocumentService continues to read the preserved origin_id.
  NavigateToUrlAndCreateCdmDocumentService(GURL(origin_str));
  auto data = GetMediaFoundationCdmData();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->origin_id, origin_id);
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

#if BUILDFLAG(IS_WIN)
bool HasListDirectoryPermission(const base::FilePath& path,
                                const std::vector<base::win::Sid>& sids) {
  PACL dacl = nullptr;
  PSECURITY_DESCRIPTOR sd = nullptr;
  // Manually retrieve the Discretionary Access Control List (DACL) to inspect
  // its entries directly.
  if (::GetNamedSecurityInfo(path.value().c_str(), SE_FILE_OBJECT,
                             DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl,
                             nullptr, &sd) != ERROR_SUCCESS) {
    return false;
  }
  bool has_list_directory = false;
  if (dacl) {
    // Iterate over each Access Control Entry (ACE) in the DACL.
    for (DWORD i = 0; i < dacl->AceCount; ++i) {
      PVOID ace_ptr = nullptr;
      if (::GetAce(dacl, i, &ace_ptr)) {
        PACE_HEADER ace_header = static_cast<PACE_HEADER>(ace_ptr);
        // We only care about ACEs that grant access and are not marked as
        // "inherit only". INHERIT_ONLY_ACEs apply to child objects, not the
        // directory itself.
        if (ace_header->AceType == ACCESS_ALLOWED_ACE_TYPE &&
            !(ace_header->AceFlags & INHERIT_ONLY_ACE)) {
          PACCESS_ALLOWED_ACE allowed_ace =
              static_cast<PACCESS_ALLOWED_ACE>(ace_ptr);
          // Check if this ACE grants the FILE_LIST_DIRECTORY permission.
          if (allowed_ace->Mask & FILE_LIST_DIRECTORY) {
            PSID ace_sid = reinterpret_cast<PSID>(&allowed_ace->SidStart);
            // Check if the SID in the ACE matches our LPAC SID.
            for (const auto& sid : sids) {
              if (::EqualSid(ace_sid, sid.GetPSID())) {
                has_list_directory = true;
                break;
              }
            }
          }
        }
      }
      if (has_list_directory) {
        break;
      }
    }
  }
  ::LocalFree(sd);
  return has_list_directory;
}

TEST_F(CdmDocumentServiceImplTest, VerifyCdmStorePathRootAcl) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto data = GetMediaFoundationCdmData();

  auto sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox::policy::kMediaFoundationCdmData});
  ASSERT_FALSE(sids.empty());

  // The root path should have traverse permissions.
  EXPECT_TRUE(base::win::HasAccessToPath(data->cdm_store_path_root, sids,
                                         FILE_TRAVERSE, NO_INHERITANCE));

  // The root path should NOT have list directory permissions to prevent
  // cross-origin enumeration. base::win::HasAccessToPath cannot be used here
  // because it matches inherit-only ACEs when querying with NO_INHERITANCE.
  EXPECT_FALSE(HasListDirectoryPermission(data->cdm_store_path_root, sids));

  // And the inherited permissions should grant full access to subdirectories.
  // (Note: HasAccessToPath requires the exact inheritance flags to match the
  // ACE).
  EXPECT_TRUE(base::win::HasAccessToPath(
      data->cdm_store_path_root, sids,
      FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE | INHERIT_ONLY_ACE));
}

TEST_F(CdmDocumentServiceImplTest, MigrateCdmStorePathRootAcl) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));
  auto data = GetMediaFoundationCdmData();

  auto sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox::policy::kMediaFoundationCdmData});
  ASSERT_FALSE(sids.empty());

  // Manually apply the old vulnerable ACL to simulate a pre-existing root.
  ASSERT_TRUE(base::win::GrantAccessToPath(
      data->cdm_store_path_root, sids,
      FILE_GENERIC_READ | FILE_GENERIC_WRITE | GENERIC_EXECUTE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE));

  // Verify the vulnerable ACL is actually applied.
  EXPECT_TRUE(HasListDirectoryPermission(data->cdm_store_path_root, sids));

  // Clear the processed paths cache so the migration logic runs again.
  CdmDocumentServiceImpl::ClearCdmStoreProcessedPathsForTesting();

  // Trigger the creation/migration logic again.
  auto data2 = GetMediaFoundationCdmData();

  // Verify the vulnerable ACL is removed.
  EXPECT_FALSE(HasListDirectoryPermission(data2->cdm_store_path_root, sids));

  // And the intended ACLs are restored.
  EXPECT_TRUE(base::win::HasAccessToPath(data2->cdm_store_path_root, sids,
                                         FILE_TRAVERSE, NO_INHERITANCE));
  EXPECT_TRUE(base::win::HasAccessToPath(
      data2->cdm_store_path_root, sids,
      FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE | INHERIT_ONLY_ACE));
}

// Verifies that an existing per-origin subdirectory still grants the
// lpacMediaFoundationCdmData SID effective read/write/delete access after the
// CDM store root's ACL is re-applied on a subsequent call to
// GetMediaFoundationCdmData().
//
// The first call creates the root with the inherit-only broad ACE and
// pre-creates <origin_id>/, which inherits (I)(OI)(CI)(R,W,D) for the SID at
// creation time. The second call sees the root already exists and runs the
// migration path, which revokes the SID's ACEs on the root (cascading the
// removal through NTFS auto-inheritance onto existing children) and then
// re-applies the inherit-only broad ACE. The final re-apply must propagate
// the new ACE onto existing <origin_id>/ subdirectories so the LPAC retains
// access to per-origin CDM state created in an earlier session.
TEST_F(CdmDocumentServiceImplTest, MigrationPreservesAccessOnExistingSubdirs) {
  NavigateToUrlAndCreateCdmDocumentService(GURL(kTestOrigin));

  // Session 1: creates the root and pre-creates the per-origin subdir.
  auto data1 = GetMediaFoundationCdmData();
  ASSERT_TRUE(data1);
  base::FilePath origin_subdir =
      data1->cdm_store_path_root.AppendASCII(data1->origin_id.ToString());
  ASSERT_TRUE(base::PathExists(origin_subdir));

  auto sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox::policy::kMediaFoundationCdmData});
  ASSERT_FALSE(sids.empty());

  // After session 1, the per-origin subdir should have inherited the broad
  // ACE from the root (effective on the subdir itself, not just inherit-only).
  EXPECT_TRUE(base::win::HasAccessToPath(
      origin_subdir, sids, FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE))
      << "Newly-created per-origin subdir should inherit the LPAC ACE.";

  // Session 2: same origin, root already exists -> migration path runs.
  auto data2 = GetMediaFoundationCdmData();
  ASSERT_TRUE(data2);
  ASSERT_EQ(data1->origin_id, data2->origin_id);
  ASSERT_TRUE(base::PathExists(origin_subdir));

  // After migration, the same per-origin subdir must still grant the LPAC SID
  // effective FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE.
  EXPECT_TRUE(base::win::HasAccessToPath(
      origin_subdir, sids, FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE))
      << "After migration, per-origin subdir lost effective LPAC access; the "
         "inherit-only ACE on the root did not propagate to existing children.";
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
