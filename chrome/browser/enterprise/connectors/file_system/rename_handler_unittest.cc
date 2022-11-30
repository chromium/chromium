// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using download::DownloadInterruptReason;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

const char kBox[] = "box";

using RenameHandler = FileSystemRenameHandler;

class RenameHandlerCreateTestBase : public testing::Test {
 protected:
  Profile* profile() { return profile_; }

  void Init(bool enable_feature_flag, const char* pref_value) {
    ASSERT_TRUE(pref_value);
    if (enable_feature_flag) {
      scoped_feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kFileSystemConnectorEnabled});
    }

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    // Set a dummy policy value to enable the rename handler.
    profile_->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(pref_value));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

class RenameHandlerCreateTest : public RenameHandlerCreateTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  RenameHandlerCreateTest() {
    Init(enable_feature_flag(), kWildcardSendDownloadToCloudPref);
  }

  bool enable_feature_flag() const { return GetParam(); }
};

TEST_P(RenameHandlerCreateTest, FeatureFlagTest) {
  // Check the precondition regardless of download item: feature is enabled.
  auto settings = GetFileSystemSettings(profile());
  ASSERT_EQ(enable_feature_flag(), settings.has_value());

  // Ensure a RenameHandler can be created with the profile in download item.
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://renameme.com"));
  item.SetTabUrl(GURL("https://renameme.com"));
  item.SetMimeType("text/plain");
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  auto handler = RenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(enable_feature_flag(), handler.get() != nullptr);
}

TEST_P(RenameHandlerCreateTest, Completed_LoadFromRerouteInfo) {
  DownloadItemForTest test_item_(FILE_PATH_LITERAL("handler_loaded_info.txt"));
  test_item_.SetState(DownloadItemForTest::DownloadState::COMPLETE);

  DownloadItemRerouteInfo rerouted_info;
  rerouted_info.set_service_provider(BoxUploader::kServiceProvider);
  rerouted_info.mutable_box()->set_file_id("12345");
  test_item_.SetRerouteInfo(rerouted_info);

  // Handler should be created regardless of settings/policies since item upload
  // was already completed.
  auto handler = RenameHandler::CreateIfNeeded(&test_item_);
  ASSERT_TRUE(handler.get());
}

TEST_P(RenameHandlerCreateTest, Completed_NoRerouteInfo) {
  DownloadItemForTest test_item_(FILE_PATH_LITERAL("handler_loaded_info.txt"));
  test_item_.SetState(DownloadItemForTest::DownloadState::COMPLETE);

  // Handler should NOT be created regardless of settings/policies since item
  // download was already completed without upload previously.
  auto handler = RenameHandler::CreateIfNeeded(&test_item_);
  ASSERT_FALSE(handler.get());
}

INSTANTIATE_TEST_SUITE_P(, RenameHandlerCreateTest, testing::Bool());

struct PolicyTestParam {
  const char* test_policy = nullptr;
  std::string item_url;
  std::string tab_url;
  std::string item_mime_type;
  bool expect_routing = false;
  std::string expect_logic_msg;

  // To print param value for debugging.
  std::string Print() const {
    std::string str;
    if (expect_logic_msg.size()) {
      str += "\nExpectation logic: ";
      str += expect_logic_msg;
    }
    str += "\nTest inputs:\n > Policy: ";
    str += test_policy;
    str += "\n > test item item url: " + item_url +
           "\n > test item tab url: " + tab_url +
           "\n > test item mime type: " + item_mime_type;
    return str;
  }
};

class RenameHandlerCreateTest_Policies
    : public RenameHandlerCreateTestBase,
      public testing::WithParamInterface<PolicyTestParam> {
  void SetUp() override { Init(true, GetParam().test_policy); }
};

TEST_P(RenameHandlerCreateTest_Policies, Test) {
  const auto& param = GetParam();

  content::FakeDownloadItem item;
  item.SetURL(GURL(param.item_url));
  item.SetTabUrl(GURL(param.tab_url));
  item.SetMimeType(param.item_mime_type);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  auto handler = RenameHandler::CreateIfNeeded(&item);
  bool rename_handler_created = handler.get() != nullptr;
  ASSERT_EQ(param.expect_routing, rename_handler_created) << param.Print();
}

namespace create_by_policies_tests {

constexpr char kFSCPref_NoDict[] = R"([])";

constexpr char kFSCPref_NoEnterpriseId[] = R"([
  {
    "service_provider": "box",
    "enable": [{
        "url_list": [ "yes.com" ],
        "mime_types": [ "text/plain", "image/png", "application/zip" ]
    }]
  }
])";

constexpr char kFSCPref_NoUrlList[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [{"mime_types": [ "text/plain", "image/png", "application/zip" ]}]
  }
])";

constexpr char kFSCPref_NoMimeTypes[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [{"url_list": [ "yes.com" ]}]
  }
])";

// All expecting NO routing.
std::vector<PolicyTestParam> incomplete_policies_tests = {
    {kFSCPref_NoDict},
    {R"([{ "service_provider": "box", "enterprise_id": "12345" }])"},
    {R"([{ "service_provider": "box" }])"},
    {R"([{ "enable": [{ "url_list": ["here.com"] }] }])"},
    {R"([{ "enable": [{ "mime_types": ["this/that"] }] }])"},
    {R"([{ "enable": [{ "url_list": ["here.com"],
                        "mime_types": ["this/that"]}]
        }])"},
    {R"([{ "enable": [{ "url_list": ["*"],
                        "mime_types": ["*"]}],
           "disable": [{ "url_list": ["here.com"]}]
        }])"},
    {R"([{ "enable": [{ "url_list": ["*"],
                        "mime_types": ["*"]}],
           "disable": [{ "mime_types": ["this/that"]}]
        }])"},
    {R"([{ "enable": [{ "url_list": ["*"],
                        "mime_types": ["*"] }],
           "disable": [{ "mime_types": ["this/that"],
                         "url_list": ["here.com"]}]
        }])"},
    {kFSCPref_NoEnterpriseId},
    {kFSCPref_NoUrlList},
    {kFSCPref_NoMimeTypes}};
INSTANTIATE_TEST_SUITE_P(PolicyValidation,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(incomplete_policies_tests));

constexpr char kFSCPref_EmptyEnable_OffByDefault[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [{ "url_list": [ ],
                 "mime_types": [ ] }]
  }
])";
// kWildcardSendDownloadToCloudPref is in test_helper.h. It is equivalent to
// on-by-default.
PolicyTestParam on_by_default_should_route_everything{
    kWildcardSendDownloadToCloudPref,
    "https://no.com/file.txt",
    "https://no.com",
    "text/plain",
    true,
    "Wildcard policy ===> Any URL/MIME should result in YES routing"};
PolicyTestParam empty_enable_equals_off_by_default{
    kFSCPref_EmptyEnable_OffByDefault,
    "https://yes.com/file.txt",
    "https://yes.com",
    "text/plain",
    false,
    "Empty enable ===> Everything should result in NO routing"};
std::vector<PolicyTestParam> simple_policies_tests = {
    on_by_default_should_route_everything, empty_enable_equals_off_by_default};

INSTANTIATE_TEST_SUITE_P(SimplePolicies,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(simple_policies_tests));

constexpr char kMimeType_JPG[] = "image/jpg";
constexpr char kMimeType_7Z[] = "application/x-7z-compressed";

constexpr char kFSCPref_OffByDefault_Except[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["yes.com"],
        "mime_types": ["image/jpg", "image/jpeg", "image/png"]
      }
    ]
  }
])";
PolicyTestParam no_url_matches_pattern{
    kFSCPref_OffByDefault_Except,
    "https://one.com/file.jpg",
    "https://two.com",
    kMimeType_JPG,
    false,
    "No URL matches pattern ===> NO routing"};
PolicyTestParam file_url_matches_pattern{
    kFSCPref_OffByDefault_Except,
    "https://yes.com/file.jpg",
    "https://two.com",
    kMimeType_JPG,
    true,
    "File URL matches pattern ===> YES routing"};
PolicyTestParam tab_url_matches_pattern{
    kFSCPref_OffByDefault_Except,
    "https://one.com/file.jpg",
    "https://yes.com",
    kMimeType_JPG,
    true,
    "Tab URL matches pattern ===> YES routing"};
PolicyTestParam disallowed_by_mime_type{
    kFSCPref_OffByDefault_Except,
    "https://yes.com/file.json",
    "https://yes.com",
    "application/json",
    false,
    "Disalloswed by MIME type  ===> NO routing"};

constexpr char kFSCPref_OffByDefault_Except_NoMimeTypes[] = R"([
  {
   "domain": "google.com",
   "enable": [ {
      "url_list": [ "yes.com" ]
   } ],
   "enterprise_id": "611447719",
   "service_provider": "box"
}
])";
PolicyTestParam no_enable_field_equals_disable{
    kFSCPref_OffByDefault_Except_NoMimeTypes,
    "https://yes.com/file.txt",
    "https://two.com",
    "text/plain",
    false,
    "No enable field in policy means no exception to enable ===> Everything "
    "should result in NO routing"};

std::vector<PolicyTestParam> off_by_default_tests = {
    no_url_matches_pattern, file_url_matches_pattern, tab_url_matches_pattern,
    disallowed_by_mime_type, no_enable_field_equals_disable};
INSTANTIATE_TEST_SUITE_P(OffByDefault_Except,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(off_by_default_tests));

// Off by default policy: all combos should result in NO routing except for
// yes.com + yes routing type
std::vector<PolicyTestParam> off_by_default_with_exception_tests = {
    {kFSCPref_OffByDefault_Except, "https://yes.com/file.jpg",
     "https://yes.com", kMimeType_JPG, true},
    {kFSCPref_OffByDefault_Except, "https://no.com/file.jpg", "https://no.com",
     kMimeType_JPG, false},
    {kFSCPref_OffByDefault_Except, "https://no.com/file.7z", "https://no.com",
     kMimeType_7Z, false},
    {kFSCPref_OffByDefault_Except, "https://yes.com/file.7z", "https://yes.com",
     kMimeType_7Z, false}};

INSTANTIATE_TEST_SUITE_P(
    OffByDefaultWithExceptions,
    RenameHandlerCreateTest_Policies,
    testing::ValuesIn(off_by_default_with_exception_tests));

constexpr char kFSCPref_OnByDefault_Except[] = R"([
  {
    "disable": [ {
      "mime_types": [ "application/x-bzip", "application/x-7z-compressed" ],
      "url_list": [ "no.com", "no1.com", "no2.com" ]
    } ],
    "domain": "google.com",
    "enable": [ {
      "mime_types": [ "*" ],
      "url_list": [ "*" ]
    } ],
    "enterprise_id": "123456789",
    "service_provider": "box"
  }
])";

// On by default policy: all combos should result in routing except for
// no.com + no routing type
std::vector<PolicyTestParam> on_by_default_with_exception_tests = {
    {kFSCPref_OnByDefault_Except, "https://no.com/file.7z", "https://no.com",
     kMimeType_7Z, false},
    {kFSCPref_OnByDefault_Except, "https://no.com/file.jpg", "https://no.com",
     kMimeType_JPG, true},
    {kFSCPref_OnByDefault_Except, "https://yes.com/file.jpg", "https://yes.com",
     kMimeType_JPG, true},
    {kFSCPref_OnByDefault_Except, "https://yes.com/file.7z", "https://yes.com",
     kMimeType_7Z, true}};

INSTANTIATE_TEST_SUITE_P(OnByDefault_Except,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(on_by_default_with_exception_tests));

constexpr char kFSCPref_OffByDefault_Except_SubtypeWildcard[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["yes.com"],
        "mime_types": ["image/*", "application/*"]
      }
    ]
  }
])";

// Off by default policy: all combos should result in routing except for
// no.com + no routing type
std::vector<PolicyTestParam>
    off_by_default_with_exception_subtype_wildcard_tests = {
        {kFSCPref_OffByDefault_Except_SubtypeWildcard, "https://no.com/file.7z",
         "https://no.com", kMimeType_7Z, false},
        {kFSCPref_OffByDefault_Except_SubtypeWildcard,
         "https://no.com/file.jpg", "https://no.com", kMimeType_JPG, false},
        {kFSCPref_OffByDefault_Except_SubtypeWildcard,
         "https://yes.com/file.jpg", "https://yes.com", kMimeType_JPG, true},
        {kFSCPref_OffByDefault_Except_SubtypeWildcard,
         "https://yes.com/file.7z", "https://yes.com", kMimeType_7Z, true}};

INSTANTIATE_TEST_SUITE_P(
    OffByDefault_Except_SubtypeWildcard,
    RenameHandlerCreateTest_Policies,
    testing::ValuesIn(off_by_default_with_exception_subtype_wildcard_tests));

constexpr char kFSCPref_OnByDefault_Except_SubtypeWildcard[] = R"([
  {
    "disable": [ {
      "mime_types": [ "application/*", "image/*" ],
      "url_list": [ "no.com", "no1.com", "no2.com" ]
    } ],
    "domain": "google.com",
    "enable": [ {
      "mime_types": [ "*" ],
      "url_list": [ "*" ]
    } ],
    "enterprise_id": "123456789",
    "service_provider": "box"
  }
])";

// On by default policy: all combos should result in routing except for
// no.com + no routing type
std::vector<PolicyTestParam>
    on_by_default_with_exception_subtype_wildcard_tests = {
        {kFSCPref_OnByDefault_Except_SubtypeWildcard, "https://no.com/file.7z",
         "https://no.com", kMimeType_7Z, false},
        {kFSCPref_OnByDefault_Except_SubtypeWildcard, "https://no.com/file.jpg",
         "https://no.com", kMimeType_JPG, false},
        {kFSCPref_OnByDefault_Except_SubtypeWildcard,
         "https://yes.com/file.jpg", "https://yes.com", kMimeType_JPG, true},
        {kFSCPref_OnByDefault_Except_SubtypeWildcard, "https://yes.com/file.7z",
         "https://yes.com", kMimeType_7Z, true}};

INSTANTIATE_TEST_SUITE_P(
    OnByDefault_Except_SubtypeWildcard,
    RenameHandlerCreateTest_Policies,
    testing::ValuesIn(on_by_default_with_exception_subtype_wildcard_tests));

constexpr char kFSCPref_DisEnableConflict[] = R"([
  {
    "disable": [ {
      "mime_types": [ "application/*", "image/*" ],
      "url_list": [ "*" ]
    } ],
    "domain": "google.com",
    "enable": [ {
      "mime_types": [ "*" ],
      "url_list": [ "no.com", "no1.com", "no2.com" ]
    } ],
    "enterprise_id": "123456789",
    "service_provider": "box"
  }
])";

std::vector<PolicyTestParam> conflicting_filter_tests = {
    {kFSCPref_DisEnableConflict, "https://no.com/file.7z", "https://no.com",
     kMimeType_7Z, false},
    {kFSCPref_DisEnableConflict, "https://no.com/file.jpg", "https://no.com",
     kMimeType_JPG, false},
    {kFSCPref_DisEnableConflict, "https://yes.com/file.jpg", "https://yes.com",
     kMimeType_JPG, false},
    {kFSCPref_DisEnableConflict, "https://yes.com/file.7z", "https://yes.com",
     kMimeType_7Z, false}};

INSTANTIATE_TEST_SUITE_P(DisEnableConflict,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(conflicting_filter_tests));

constexpr char kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": [ "*", "yes.com" ],
        "mime_types": ["image/jpg", "image/*"]
      }
    ]
  }
])";

constexpr char kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes[] =
    R"([
  {
    "disable": [ {
      "mime_types": [ "image/jpg", "*" ],
      "url_list": [ "*", "no.com" ]
    } ],
    "domain": "google.com",
    "enable": [ {
      "mime_types": [ "*" ],
      "url_list": [ "*" ]
    } ],
    "enterprise_id": "123456789",
    "service_provider": "box"
  }
])";

constexpr char kMimeType_PNG[] = "image/png";

std::vector<PolicyTestParam> excessive_filter_tests = {
    // Off by default policy with enable that has wildcard in URL and mime type
    // lists, plus some explicit types and URLs. Should behave the same as
    // off by default with with wildcard enable for all URLs + all image types.
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.7z", "https://no.com", kMimeType_7Z, false},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.jpg", "https://no.com", kMimeType_JPG, true},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.png", "https://no.com", kMimeType_PNG, true},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.jpg", "https://yes.com", kMimeType_JPG, true},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.png", "https://yes.com", kMimeType_PNG, true},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.7z", "https://yes.com", kMimeType_7Z, false},
    {kFSCPref_OffByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.7z", "https://no.com", kMimeType_7Z, false},
    // On by default policy with disable that has wildcard in URL and mime type
    // lists, plus some explicit types and URLs. Should behave the same as
    // wildcard disable for all URLs + all types.
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.7z", "https://no.com", kMimeType_7Z, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.jpg", "https://no.com", kMimeType_JPG, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.png", "https://no.com", kMimeType_PNG, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.jpg", "https://yes.com", kMimeType_JPG, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.png", "https://yes.com", kMimeType_PNG, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://yes.com/file.7z", "https://yes.com", kMimeType_7Z, false},
    {kFSCPref_OnByDefault_Except_WildcardWithExcessiveTypes,
     "https://no.com/file.7z", "https://no.com", kMimeType_7Z, false}};

INSTANTIATE_TEST_SUITE_P(WildcardWithExcessiveItems,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(excessive_filter_tests));

constexpr char kFSCPref_DisEnableLists[] = R"([
  {
    "disable": [ {
      "mime_types": [ "application/*", "audio/*" ],
      "url_list": [ "no-app.com, no-audio.com" ]
    }, {
      "mime_types": [ "text/*" ],
      "url_list": [ "no-text.com" ]
    } ],
    "domain": "google.com",
    "enable": [ {
      "mime_types": [ "video/*" ],
      "url_list": [ "yes-video.com" ]
    }, {
      "mime_types": [ "image/*" ],
      "url_list": [ "yes-image.com" ]
    } ],
    "enterprise_id": "123456789",
    "service_provider": "box"
  }
])";

constexpr char kMimeType_AudioMPEG[] = "audio/mpeg";
constexpr char kMimeType_TextPlain[] = "text/plain";
constexpr char kMimeType_VideoMP4[] = "video/mp4";

std::vector<PolicyTestParam> disenable_lists_tests = {
    {kFSCPref_DisEnableLists, "https://no-app.com/file.7z",
     "https://no-app.com", kMimeType_7Z, false},
    {kFSCPref_DisEnableLists, "https://no-app.com/file.jpg",
     "https://no-app.com", kMimeType_JPG, false},
    {kFSCPref_DisEnableLists, "https://no-audio.com/file.mpeg",
     "https://no-audio.com", kMimeType_AudioMPEG, false},
    {kFSCPref_DisEnableLists, "https://no-text.com/file.txt",
     "https://no-text.com", kMimeType_TextPlain, false},
    {kFSCPref_DisEnableLists, "https://yes-video.com/file.txt",
     "https://yes.com", kMimeType_TextPlain, false},
    {kFSCPref_DisEnableLists, "https://yes-video.com/file.mp4",
     "https://yes.com", kMimeType_VideoMP4, true},
    {kFSCPref_DisEnableLists, "https://yes-image.com/file.jpg",
     "https://yes-image.com", kMimeType_JPG, true},
    {kFSCPref_DisEnableLists, "https://yes-image.com/file.png",
     "https://yes-image.com", kMimeType_PNG, true}};

INSTANTIATE_TEST_SUITE_P(DisEnableLists,
                         RenameHandlerCreateTest_Policies,
                         testing::ValuesIn(disenable_lists_tests));
}  // namespace create_by_policies_tests

constexpr char ATokenBySignIn[] = "ATokenBySignIn";
constexpr char RTokenBySignIn[] = "RTokenBySignIn";
constexpr char ATokenByFetcher[] = "ATokenByFetcher";
constexpr char RTokenForFetcher[] = "RTokenForFetcher";

class RenameHandlerForTest : public FileSystemRenameHandler {
 public:
  using FileSystemRenameHandler::FileSystemRenameHandler;
  using FileSystemRenameHandler::OpenDownload;
  using FileSystemRenameHandler::SetUploaderForTesting;
  using FileSystemRenameHandler::ShowDownloadInContext;
  using AuthErr = GoogleServiceAuthError;

  MOCK_METHOD(void,
              PromptUserSignInForAuthorization,
              (content::WebContents * contents),
              (override));

  void ReturnSignInSuccess() {
    OnAuthorization(AuthErr::AuthErrorNone(), ATokenBySignIn, RTokenBySignIn);
  }

  void ReturnSignInFailure() {
    const auto fail_reason = AuthErr::FromInvalidGaiaCredentialsReason(
        AuthErr::InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_SERVER);
    OnAuthorization(fail_reason, std::string(), std::string());
  }

  void ReturnSignInCancellation() {
    OnAuthorization(AuthErr(AuthErr::State::REQUEST_CANCELED), std::string(),
                    std::string());
  }

  MOCK_METHOD(void,
              FetchAccessToken,
              (content::BrowserContext * context,
               const std::string& refresh_token),
              (override));

  void ReturnFetchSuccess() {
    OnAccessTokenFetched(AuthErr::AuthErrorNone(), ATokenByFetcher,
                         RTokenForFetcher);
  }

  void ReturnFetchFailure() {
    const auto fail_reason = AuthErr::FromInvalidGaiaCredentialsReason(
        AuthErr::InvalidGaiaCredentialsReason::CREDENTIALS_REJECTED_BY_SERVER);
    OnAccessTokenFetched(fail_reason, std::string(), std::string());
  }

  void ReturnFetchNetworkError() {
    const auto fail_reason = AuthErr::FromConnectionError(net::ERR_TIMED_OUT);
    OnAccessTokenFetched(fail_reason, std::string(), std::string());
  }
};

const base::FilePath kTargetFileName(FILE_PATH_LITERAL("rename_handler.txt"));
const char kUploadedFileId[] = "314159";  // Should match below.
const char kUploadedFileUrl[] = "https://example.com/file/314159";
const char kDestinationFolderUrl[] = "https://example.com/folder/1337";

class MockUploader : public BoxUploader {
 public:
  explicit MockUploader(download::DownloadItem* download_item)
      : BoxUploader(download_item) {}

  MOCK_METHOD(GURL, GetUploadedFileUrl, (), (const override));
  MOCK_METHOD(GURL, GetDestinationFolderUrl, (), (const override));

  MOCK_METHOD(
      void,
      TryTask,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       const std::string& access_token),
      (override));

  MOCK_METHOD(void, StartCurrentApiCall, (), (override));

  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override {
    return std::make_unique<MockApiCallFlow>();
  }

  void TryTaskSuccess() {
    // For invoking progress update in StartUpload().
    expected_reason_ = InterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
    EXPECT_CALL(*this, StartCurrentApiCall()).WillOnce(Invoke([this]() {
      SetUploadApiCallFlowDoneForTesting(expected_reason_, kUploadedFileId);
    }));

    EXPECT_CALL(*this, GetUploadedFileUrl())
        .WillOnce(Return(GURL(kUploadedFileUrl)));

    StartUpload();
  }

  void TryTaskFailure() {
    EXPECT_CALL(*this, StartCurrentApiCall()).Times(0);
    EXPECT_CALL(*this, GetUploadedFileUrl()).WillOnce(Return(GURL()));
    expected_reason_ = InterruptReason::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    SetUploadApiCallFlowDoneForTesting(expected_reason_, {});
  }

  void ExpectNoTryTask(InterruptReason reason) {
    expected_reason_ = reason;
    EXPECT_CALL(*this, GetUploadedFileUrl()).WillOnce(Return(GURL()));
    EXPECT_CALL(*this, TryTask(_, _)).Times(0);
  }

  InterruptReason expected_reason_ =
      InterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
};

class RenameHandlerTestBase {
 protected:
  RenameHandlerForTest* handler() { return handler_.get(); }
  MockUploader* uploader() { return uploader_; }

  void SetUp(TestingProfile* profile) {
    feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});

    // Make sure that from the connectors manager point of view the file system
    // connector should be enabled.  So that the only thing that controls
    // whether the rename handler is used or not is the feature flag.
    profile->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(kWildcardSendDownloadToCloudPref));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);

    item_.SetTargetFilePath(kTargetFileName);
    item_.SetURL(GURL("https://any.com"));
    content::DownloadItemUtils::AttachInfoForTesting(&item_, profile,
                                                     web_contents_.get());
    ASSERT_TRUE(base::WriteFile(item_.GetFullPath(), "RenameHandlerTest"))
        << "Failed to create " << item_.GetFullPath();

    handler_ = CreateHandler();
  }

  virtual std::unique_ptr<RenameHandlerForTest> CreateHandler() {
    ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(&item_));
    auto settings = service->GetFileSystemSettings(
        item_.GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
    settings->service_provider = kBox;
    auto handler = std::make_unique<RenameHandlerForTest>(
        &item_, std::move(settings.value()));

    auto uploader = std::make_unique<MockUploader>(&item_);
    uploader_ = uploader.get();
    handler->SetUploaderForTesting(std::move(uploader));
    return handler;
  }

  void TearDown() {
    handler_.reset();
    web_contents_.reset();
    ASSERT_FALSE(base::PathExists(item_.GetFullPath()) &&
                 base::PathExists(item_.GetTargetFilePath()))
        << "File should have been deleted regardless of rerouting success or "
           "failure";
  }

  DownloadItemForTest item_{FILE_PATH_LITERAL("rename_handler_test.txt")};

 private:
  std::unique_ptr<RenameHandlerForTest> handler_;
  raw_ptr<MockUploader> uploader_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
};

class RenameHandlerOAuth2Test : public testing::Test,
                                public RenameHandlerTestBase {
 public:
  RenameHandlerOAuth2Test()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    OSCryptMocker::SetUp();
    RenameHandlerTestBase::SetUp(profile_);
    EXPECT_CALL(*uploader(), GetDestinationFolderUrl()).Times(0);
  }

  void TearDown() override {
    ASSERT_EQ(download_cb_reason_, uploader()->expected_reason_);
    RenameHandlerTestBase::TearDown();
    OSCryptMocker::TearDown();
  }

 protected:
  void RunHandler() {
    run_loop_ = std::make_unique<base::RunLoop>();
    download::DownloadItemRenameHandler* download_handler_ = handler();
    download_handler_->Start(
        base::BindRepeating(&RenameHandlerOAuth2Test::OnProgressUpdate,
                            base::Unretained(this)),
        base::BindOnce(&RenameHandlerOAuth2Test::OnUploadComplete,
                       base::Unretained(this)));
    run_loop_->Run();
  }

  void VerifyBothTokensClear() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
    ASSERT_TRUE(atoken.empty());
    ASSERT_TRUE(rtoken.empty());
  }

  void VerifyBothTokensSetBySignIn() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenBySignIn);
    ASSERT_EQ(rtoken, RTokenBySignIn);
  }

  void VerifyBothTokensSetByFetcher() {
    std::string atoken, rtoken;
    ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken))
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    ASSERT_EQ(atoken, ATokenByFetcher);
    ASSERT_EQ(rtoken, RTokenForFetcher);
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

  int download_cb_count_ = 0;
  DownloadInterruptReason download_cb_reason_;
  GURL uploaded_file_url_;
  base::FilePath uploaded_file_name_;

 private:
  void OnProgressUpdate(
      const download::DownloadItemRenameProgressUpdate& update) {
    uploaded_file_name_ = update.target_file_name;
  }

  void OnUploadComplete(DownloadInterruptReason reason,
                        const base::FilePath& final_name) {
    ++download_cb_count_;
    download_cb_reason_ = reason;
    uploaded_file_url_ = uploader()->GetUploadedFileUrl();
    if (!uploaded_file_name_.empty()) {
      EXPECT_EQ(uploaded_file_name_, final_name);
    }
    run_loop_->Quit();
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(RenameHandlerOAuth2Test, NullPtrs) {
  ::testing::InSequence seq;
  constexpr char ATokenPlaceholder[] = "uselessAToken";
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenPlaceholder, RTokenForFetcher);
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);

  // Clear the map keyed in |download_item| to test this, since they are not
  // assumed to always be valid on the UI thread.
  item_.ClearAllUserData();

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_FALSE(uploaded_file_url_.is_valid());

  // Verify that the tokens stored are not updated.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_EQ(atoken, ATokenPlaceholder);
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

// Test cases are written according to The OAuth2 "Dance" in rename_handler.cc;
// all should be finished via 2a unless aborted via 1b.

////////////////////////////////////////////////////////////////////////////////
// Test cases for (1): Start with no valid token.
////////////////////////////////////////////////////////////////////////////////

// Case 1a->2a: Both tokens will be set; callback returned with success.
TEST_F(RenameHandlerOAuth2Test, SignInSuccessThenUploaderSuccess) {
  ::testing::InSequence seq;
  // 1a: PromptUserSignInForAuthorization() succeeds.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnSignInSuccess));
  // ->2a: TryUploaderTask() should be called after and succeed.
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetBySignIn();
}

// Case 1b: Both tokens will be clear; callback returned with failure.
TEST_F(RenameHandlerOAuth2Test, SignInCancellationSoAbort) {
  ::testing::InSequence seq;
  // 1b: PromptUserSignInForAuthorization() fails with Cancellation so abort.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(
          Invoke(handler(), &RenameHandlerForTest::ReturnSignInCancellation));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(
      download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  VerifyBothTokensClear();
}

// Case 1c->1a: Retry sign in, but terminate there without going through the
// uploader since 1a->2 is already covered in another test case.
TEST_F(RenameHandlerOAuth2Test, SignInFailureSoRetry) {
  ::testing::InSequence seq;
  // 1c->1a: PromptUserSignInForAuthorization() fails with other reasons than
  // Cancellation so should be called again.
  int authen_cb = 0;
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillRepeatedly(Invoke([this, &authen_cb](content::WebContents*) {
        ++authen_cb;
        if (authen_cb == 1) {
          handler()->ReturnSignInFailure();
        } else if (authen_cb == 2) {
          VerifyBothTokensClear();
          uploader()->TryTaskFailure();
          // Terminate here since 1a->2 is already covered.
        }
        ASSERT_LE(authen_cb, 2) << "Should've terminated above";
      }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(authen_cb, 2);
  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
  VerifyBothTokensClear();
}

// Case 1c but failed to clear token: Terminate upload flow with internal error.
TEST_F(RenameHandlerOAuth2Test, SignInFailureThenClearTokenFailure) {
  ::testing::InSequence seq;
  // 1c->1a: PromptUserSignInForAuthorization() fails with other reasons than
  // Cancellation so should be called again.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke([this](content::WebContents*) {
        item_.ClearAllUserData();  // To make GetPrefs() return nullptr.
        handler()->ReturnSignInFailure();
      }));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (3): Start with only refresh token.
////////////////////////////////////////////////////////////////////////////////

// Case 3a->2a: Fetch access token with refresh token succeeds, so should
// TryUploaderTask();
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenSuccess) {
  ::testing::InSequence seq;
  // Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  // 3a.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchSuccess));
  // ->2a.
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetByFetcher();
}

// Case 3b->1: Fetch access token with refresh token fails, so should clear
// tokens and PromptUserSignInForAuthorization().
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenFailureSoPromptForSignIn) {
  ::testing::InSequence seq;
  // Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  // 3b.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(handler(), &RenameHandlerForTest::ReturnFetchFailure));
  // ->1: Prompt user to sign in, but terminate because Case 1 is already
  // covered.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensClear();
}

// Case 3c: Fetch access token failed with network error, so don't clear the
// tokens, but do abort the upload with the network failure reason.
TEST_F(RenameHandlerOAuth2Test, FetchAccessTokenNetworkFailureSoAbort) {
  ::testing::InSequence seq;
  // 3c: Set a refresh token before starting, so should fetch access token.
  SetFileSystemOAuth2Tokens(prefs(), kBox, std::string(), RTokenForFetcher);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(
          Invoke(handler(), &RenameHandlerForTest::ReturnFetchNetworkError));

  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  uploader()->ExpectNoTryTask(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_FALSE(uploaded_file_url_.is_valid());
  // Verify that the tokens stored are not updated.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_TRUE(atoken.empty());
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

////////////////////////////////////////////////////////////////////////////////
// Test cases for (2): Start with both tokens.
////////////////////////////////////////////////////////////////////////////////

// Case 2a(failure): TryUploaderTask() with existing access token and fails,
// but both tokens stay.
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenThenUploaderFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  // Verify that uploader failure did not affect stored credentials.
  VerifyBothTokensSetByFetcher();
}

// Case 2a(success): TryUploaderTask() with existing access token and
// succeeds.
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenThenUploaderSuccess) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2a:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskSuccess));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_EQ(uploaded_file_url_, kUploadedFileUrl);
  ASSERT_EQ(uploaded_file_name_, kTargetFileName);
  VerifyBothTokensSetByFetcher();
}

// Case 2b->3: TryUploaderTask() with existing access token but fails with
// authentication error so FetchAccessToken().
TEST_F(RenameHandlerOAuth2Test, StartWithAccessTokenButUploaderOAuth2Error) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::NotifyOAuth2ErrorForTesting));
  // 3: Authentication error should lead to clearing access token stored, and
  // FetchAccessToken(). Just terminate here though because Case 3 is already
  // covered.
  EXPECT_CALL(*handler(), FetchAccessToken(_, _))
      .WillOnce(Invoke(uploader(), &MockUploader::TryTaskFailure));
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());
  // Verify that access token stored is cleared.
  std::string atoken, rtoken;
  ASSERT_TRUE(GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken));
  ASSERT_TRUE(atoken.empty());
  ASSERT_EQ(rtoken, RTokenForFetcher);
}

// Case 2b but failed to clear token: Terminate upload flow with internal error.
TEST_F(RenameHandlerOAuth2Test, UploaderOAuth2ErrorThenClearTokenFailure) {
  ::testing::InSequence seq;
  // 2: Set an access token before starting, so should TryUploaderTask().
  SetFileSystemOAuth2Tokens(prefs(), kBox, ATokenByFetcher, RTokenForFetcher);
  // 2b:
  EXPECT_CALL(*uploader(), TryTask(_, _))
      .WillOnce(Invoke([this](scoped_refptr<network::SharedURLLoaderFactory>,
                              const std::string&) {
        item_.ClearAllUserData();  // To make GetPrefs() return nullptr.
        uploader()->ExpectNoTryTask(download::DOWNLOAD_INTERRUPT_REASON_CRASH);
        uploader()->NotifyOAuth2ErrorForTesting();
      }));
  EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
  // These OAuth2 branches should not be called.
  EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);

  RunHandler();

  ASSERT_EQ(download_cb_count_, 1);
  ASSERT_TRUE(uploaded_file_url_.is_empty());  // Notified failure to terminate.
}

class RenameHandlerOpenDownloadTest : public BrowserWithTestWindowTest,
                                      public RenameHandlerTestBase {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    RenameHandlerTestBase::SetUp(profile());
    EXPECT_CALL(*handler(), PromptUserSignInForAuthorization(_)).Times(0);
    EXPECT_CALL(*handler(), FetchAccessToken(_, _)).Times(0);
    EXPECT_CALL(*uploader(), TryTask(_, _)).Times(0);
  }

  void TearDown() override {
    RenameHandlerTestBase::TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

  const GURL GetVisibleURL() {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    CHECK(tab_strip);
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    CHECK(active_contents);
    return active_contents->GetVisibleURL();
  }
};

TEST_F(RenameHandlerOpenDownloadTest, OpenDownloadItem) {
  EXPECT_CALL(*uploader(), GetUploadedFileUrl())
      .WillOnce(Return(GURL(kUploadedFileUrl)));
  EXPECT_CALL(*uploader(), GetDestinationFolderUrl()).Times(0);

  handler()->OpenDownload();
  // Verify that the active tab has the correct uploaded file URL.
  EXPECT_EQ(GetVisibleURL(), kUploadedFileUrl);
}

TEST_F(RenameHandlerOpenDownloadTest, ShowDownloadInContext) {
  EXPECT_CALL(*uploader(), GetDestinationFolderUrl())
      .WillOnce(Return(GURL(kDestinationFolderUrl)));
  EXPECT_CALL(*uploader(), GetUploadedFileUrl()).Times(0);

  handler()->ShowDownloadInContext();
  // Verify that the active tab has the correct destination folder URL.
  EXPECT_EQ(GetVisibleURL(), kDestinationFolderUrl);
}

}  // namespace enterprise_connectors
