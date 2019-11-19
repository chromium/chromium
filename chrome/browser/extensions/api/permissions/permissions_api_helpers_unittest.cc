// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/common/extensions/api/permissions.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api::permissions::Permissions;
using extensions::permissions_api_helpers::PackPermissionSet;
using extensions::permissions_api_helpers::UnpackPermissionSet;
using extensions::permissions_api_helpers::UnpackPermissionSetResult;
using extensions::permissions_test_util::GetPatternsAsStrings;

namespace extensions {

// Tests that we can convert PermissionSets to the generated types.
TEST(ExtensionPermissionsAPIHelpers, Pack) {
  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  apis.insert(APIPermission::kFileBrowserHandler);
  // Note: kFileBrowserHandler implies kFileBrowserHandlerInternal.

  URLPatternSet explicit_hosts(
      {URLPattern(Extension::kValidHostPermissionSchemes, "http://a.com/*"),
       URLPattern(Extension::kValidHostPermissionSchemes, "http://b.com/*")});
  URLPatternSet scriptable_hosts(
      {URLPattern(UserScript::ValidUserScriptSchemes(), "http://c.com/*"),
       URLPattern(UserScript::ValidUserScriptSchemes(), "http://d.com/*")});

  // Pack the permission set to value and verify its contents.
  std::unique_ptr<Permissions> pack_result(PackPermissionSet(
      PermissionSet(std::move(apis), ManifestPermissionSet(),
                    std::move(explicit_hosts), std::move(scriptable_hosts))));
  ASSERT_TRUE(pack_result);
  ASSERT_TRUE(pack_result->permissions);
  EXPECT_THAT(*pack_result->permissions,
              testing::UnorderedElementsAre("tabs", "fileBrowserHandler",
                                            "fileBrowserHandlerInternal"));

  ASSERT_TRUE(pack_result->origins);
  EXPECT_THAT(*pack_result->origins, testing::UnorderedElementsAre(
                                         "http://a.com/*", "http://b.com/*",
                                         "http://c.com/*", "http://d.com/*"));
}

// Tests various error conditions and edge cases when unpacking values
// into PermissionSets.
TEST(ExtensionPermissionsAPIHelpers, Unpack_Basic) {
  std::unique_ptr<base::ListValue> apis(new base::ListValue());
  apis->AppendString("tabs");
  std::unique_ptr<base::ListValue> origins(new base::ListValue());
  origins->AppendString("http://a.com/*");

  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  std::unique_ptr<const PermissionSet> permissions;
  std::string error;

  APIPermissionSet optional_apis;
  optional_apis.insert(APIPermission::kTab);
  URLPatternSet optional_explicit_hosts(
      {URLPattern(Extension::kValidHostPermissionSchemes, "http://a.com/*")});
  PermissionSet optional_permissions(
      std::move(optional_apis), ManifestPermissionSet(),
      std::move(optional_explicit_hosts), URLPatternSet());

  // Origins shouldn't have to be present.
  {
    Permissions permissions_object;
    value->Set("permissions", apis->CreateDeepCopy());
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));

    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, PermissionSet(),
                            optional_permissions, true, &error);

    ASSERT_TRUE(unpack_result);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(1u, unpack_result->optional_apis.size());
    EXPECT_TRUE(unpack_result->optional_apis.count(APIPermission::kTab));
  }

  // The api permissions don't need to be present either.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", origins->CreateDeepCopy());
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));

    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, PermissionSet(),
                            optional_permissions, true, &error);
    ASSERT_TRUE(unpack_result);
    EXPECT_TRUE(error.empty());
    EXPECT_THAT(permissions_test_util::GetPatternsAsStrings(
                    unpack_result->optional_explicit_hosts),
                testing::UnorderedElementsAre("http://a.com/*"));
  }

  // Throw errors for non-string API permissions.
  {
    Permissions permissions_object;
    value->Clear();
    std::unique_ptr<base::ListValue> invalid_apis = apis->CreateDeepCopy();
    invalid_apis->AppendInteger(3);
    value->Set("permissions", std::move(invalid_apis));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Throw errors for non-string origins.
  {
    Permissions permissions_object;
    value->Clear();
    std::unique_ptr<base::ListValue> invalid_origins =
        origins->CreateDeepCopy();
    invalid_origins->AppendInteger(3);
    value->Set("origins", std::move(invalid_origins));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Throw errors when "origins" or "permissions" are not list values.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", std::make_unique<base::Value>(2));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  {
    Permissions permissions_object;
    value->Clear();
    value->Set("permissions", std::make_unique<base::Value>(2));
    EXPECT_FALSE(Permissions::Populate(*value, &permissions_object));
  }

  // Additional fields should be allowed.
  {
    Permissions permissions_object;
    value->Clear();
    value->Set("origins", origins->CreateDeepCopy());
    value->Set("random", std::make_unique<base::Value>(3));
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));

    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, PermissionSet(),
                            optional_permissions, true, &error);
    ASSERT_TRUE(unpack_result);
    EXPECT_TRUE(error.empty());
    EXPECT_THAT(permissions_test_util::GetPatternsAsStrings(
                    unpack_result->optional_explicit_hosts),
                testing::UnorderedElementsAre("http://a.com/*"));
  }

  // Unknown permissions should throw an error.
  {
    Permissions permissions_object;
    value->Clear();
    std::unique_ptr<base::ListValue> invalid_apis = apis->CreateDeepCopy();
    invalid_apis->AppendString("unknown_permission");
    value->Set("permissions", std::move(invalid_apis));
    EXPECT_TRUE(Permissions::Populate(*value, &permissions_object));

    EXPECT_FALSE(UnpackPermissionSet(permissions_object, PermissionSet(),
                                     optional_permissions, true, &error));
    EXPECT_EQ(error, "'unknown_permission' is not a recognized permission.");
  }
}

// Tests that host permissions are properly partitioned according to the
// required/optional permission sets.
TEST(ExtensionPermissionsAPIHelpers, Unpack_HostSeparation) {
  auto explicit_url_pattern = [](const char* pattern) {
    return URLPattern(Extension::kValidHostPermissionSchemes, pattern);
  };
  auto scriptable_url_pattern = [](const char* pattern) {
    return URLPattern(UserScript::ValidUserScriptSchemes(), pattern);
  };

  constexpr char kRequiredExplicit1[] = "https://required_explicit1.com/*";
  constexpr char kRequiredExplicit2[] = "https://required_explicit2.com/*";
  constexpr char kOptionalExplicit1[] = "https://optional_explicit1.com/*";
  constexpr char kOptionalExplicit2[] = "https://optional_explicit2.com/*";
  constexpr char kRequiredScriptable1[] = "https://required_scriptable1.com/*";
  constexpr char kRequiredScriptable2[] = "https://required_scriptable2.com/*";
  constexpr char kRequiredExplicitAndScriptable1[] =
      "https://required_explicit_and_scriptable1.com/*";
  constexpr char kRequiredExplicitAndScriptable2[] =
      "https://required_explicit_and_scriptable2.com/*";
  constexpr char kOptionalExplicitAndRequiredScriptable1[] =
      "https://optional_explicit_and_scriptable1.com/*";
  constexpr char kOptionalExplicitAndRequiredScriptable2[] =
      "https://optional_explicit_and_scriptable2.com/*";
  constexpr char kUnlisted1[] = "https://unlisted1.com/*";

  URLPatternSet required_explicit_hosts({
      explicit_url_pattern(kRequiredExplicit1),
      explicit_url_pattern(kRequiredExplicit2),
      explicit_url_pattern(kRequiredExplicitAndScriptable1),
      explicit_url_pattern(kRequiredExplicitAndScriptable2),
  });
  URLPatternSet required_scriptable_hosts({
      scriptable_url_pattern(kRequiredScriptable1),
      scriptable_url_pattern(kRequiredScriptable2),
      scriptable_url_pattern(kRequiredExplicitAndScriptable1),
      scriptable_url_pattern(kRequiredExplicitAndScriptable2),
      scriptable_url_pattern(kOptionalExplicitAndRequiredScriptable1),
      scriptable_url_pattern(kOptionalExplicitAndRequiredScriptable2),
  });
  URLPatternSet optional_explicit_hosts({
      explicit_url_pattern(kOptionalExplicit1),
      explicit_url_pattern(kOptionalExplicit2),
      explicit_url_pattern(kOptionalExplicitAndRequiredScriptable1),
      explicit_url_pattern(kOptionalExplicitAndRequiredScriptable2),
  });

  PermissionSet required_permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      std::move(required_explicit_hosts), std::move(required_scriptable_hosts));
  PermissionSet optional_permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      std::move(optional_explicit_hosts), URLPatternSet());

  Permissions permissions_object;
  permissions_object.origins =
      std::make_unique<std::vector<std::string>>(std::vector<std::string>(
          {kRequiredExplicit1, kOptionalExplicit1, kRequiredScriptable1,
           kRequiredExplicitAndScriptable1,
           kOptionalExplicitAndRequiredScriptable1, kUnlisted1}));

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      UnpackPermissionSet(permissions_object, required_permissions,
                          optional_permissions, true, &error);
  ASSERT_TRUE(unpack_result);
  EXPECT_TRUE(error.empty()) << error;

  EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_explicit_hosts),
              testing::UnorderedElementsAre(kRequiredExplicit1,
                                            kRequiredExplicitAndScriptable1));
  EXPECT_THAT(GetPatternsAsStrings(unpack_result->optional_explicit_hosts),
              testing::UnorderedElementsAre(
                  kOptionalExplicit1, kOptionalExplicitAndRequiredScriptable1));
  EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_scriptable_hosts),
              testing::UnorderedElementsAre(
                  kRequiredScriptable1, kRequiredExplicitAndScriptable1,
                  kOptionalExplicitAndRequiredScriptable1));
  EXPECT_THAT(GetPatternsAsStrings(unpack_result->unlisted_hosts),
              testing::UnorderedElementsAre(kUnlisted1));
}

// Tests that host permissions are properly partitioned according to the
// required/optional permission sets.
TEST(ExtensionPermissionsAPIHelpers, Unpack_APISeparation) {
  constexpr APIPermission::ID kRequired1 = APIPermission::kTab;
  constexpr APIPermission::ID kRequired2 = APIPermission::kStorage;
  constexpr APIPermission::ID kOptional1 = APIPermission::kCookie;
  constexpr APIPermission::ID kOptional2 = APIPermission::kAlarms;
  constexpr APIPermission::ID kUnlisted1 = APIPermission::kIdle;

  APIPermissionSet required_apis;
  required_apis.insert(kRequired1);
  required_apis.insert(kRequired2);

  APIPermissionSet optional_apis;
  optional_apis.insert(kOptional1);
  optional_apis.insert(kOptional2);

  PermissionSet required_permissions(std::move(required_apis),
                                     ManifestPermissionSet(), URLPatternSet(),
                                     URLPatternSet());
  PermissionSet optional_permissions(std::move(optional_apis),
                                     ManifestPermissionSet(), URLPatternSet(),
                                     URLPatternSet());

  Permissions permissions_object;
  permissions_object.permissions = std::make_unique<std::vector<std::string>>(
      std::vector<std::string>({"tabs", "cookies", "idle"}));

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      UnpackPermissionSet(permissions_object, required_permissions,
                          optional_permissions, true, &error);
  ASSERT_TRUE(unpack_result);
  EXPECT_TRUE(error.empty()) << error;

  EXPECT_EQ(1u, unpack_result->required_apis.size());
  EXPECT_TRUE(unpack_result->required_apis.count(kRequired1));
  EXPECT_EQ(1u, unpack_result->optional_apis.size());
  EXPECT_TRUE(unpack_result->optional_apis.count(kOptional1));
  EXPECT_EQ(1u, unpack_result->unlisted_apis.size());
  EXPECT_TRUE(unpack_result->unlisted_apis.count(kUnlisted1));
}

// Tests that an error is thrown for permissions that cannot be optional, when
// requested as an optional permission.
TEST(ExtensionPermissionsAPIHelpers, Unpack_UnsupportedAPIPermission) {
  APIPermissionSet optional_apis;
  optional_apis.insert(APIPermission::kWallpaper);
  EXPECT_FALSE((*optional_apis.begin())->info()->supports_optional());
  PermissionSet optional_permissions(std::move(optional_apis),
                                     ManifestPermissionSet(), URLPatternSet(),
                                     URLPatternSet());

  Permissions permissions_object;
  permissions_object.permissions = std::make_unique<std::vector<std::string>>(
      std::vector<std::string>({"wallpaper"}));

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      UnpackPermissionSet(permissions_object, PermissionSet(),
                          optional_permissions, true, &error);
  ASSERT_TRUE(unpack_result) << error;
  EXPECT_EQ(1u, unpack_result->unsupported_optional_apis.size());
  EXPECT_TRUE(unpack_result->unsupported_optional_apis.count(
      APIPermission::kWallpaper));
}

// Tests that unpacking works correctly with wildcard schemes (which are
// interesting, because they only match http | https, and not all schemes).
TEST(ExtensionPermissionsAPIHelpers, Unpack_WildcardSchemes) {
  constexpr char kWildcardSchemePattern[] = "*://*/*";

  PermissionSet optional_permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      URLPatternSet({URLPattern(Extension::kValidHostPermissionSchemes,
                                kWildcardSchemePattern)}),
      URLPatternSet());

  Permissions permissions_object;
  permissions_object.origins = std::make_unique<std::vector<std::string>>(
      std::vector<std::string>({kWildcardSchemePattern}));

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      UnpackPermissionSet(permissions_object, PermissionSet(),
                          optional_permissions, true, &error);
  ASSERT_TRUE(unpack_result) << error;
  EXPECT_THAT(GetPatternsAsStrings(unpack_result->optional_explicit_hosts),
              testing::ElementsAre(kWildcardSchemePattern));
}

// Tests that unpacking <all_urls> correctly includes or omits the file:-scheme.
TEST(ExtensionPermissionsAPIHelpers, Unpack_FileSchemes_AllUrls) {
  // Without file access, <all_urls> should be parsed, but the resulting pattern
  // should not include file:-scheme access.
  {
    const int kNonFileSchemes =
        Extension::kValidHostPermissionSchemes & ~URLPattern::SCHEME_FILE;
    URLPattern all_urls(kNonFileSchemes, URLPattern::kAllUrlsPattern);
    PermissionSet required_permissions(
        APIPermissionSet(), ManifestPermissionSet(), URLPatternSet({all_urls}),
        URLPatternSet());

    Permissions permissions_object;
    permissions_object.origins = std::make_unique<std::vector<std::string>>(
        std::vector<std::string>({URLPattern::kAllUrlsPattern}));

    constexpr bool kHasFileAccess = false;
    std::string error;
    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, required_permissions,
                            PermissionSet(), kHasFileAccess, &error);
    ASSERT_TRUE(unpack_result) << error;

    EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_explicit_hosts),
                testing::ElementsAre(URLPattern::kAllUrlsPattern));
    EXPECT_FALSE(
        unpack_result->required_explicit_hosts.begin()->valid_schemes() &
        URLPattern::SCHEME_FILE);
    EXPECT_THAT(
        GetPatternsAsStrings(unpack_result->restricted_file_scheme_patterns),
        testing::IsEmpty());
  }

  // With file access, <all_urls> should be parsed and include file:-scheme
  // access.
  {
    URLPattern all_urls(Extension::kValidHostPermissionSchemes,
                        URLPattern::kAllUrlsPattern);
    PermissionSet required_permissions(
        APIPermissionSet(), ManifestPermissionSet(), URLPatternSet({all_urls}),
        URLPatternSet());

    Permissions permissions_object;
    permissions_object.origins = std::make_unique<std::vector<std::string>>(
        std::vector<std::string>({URLPattern::kAllUrlsPattern}));

    std::string error;
    constexpr bool kHasFileAccess = true;
    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, required_permissions,
                            PermissionSet(), kHasFileAccess, &error);
    ASSERT_TRUE(unpack_result) << error;

    EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_explicit_hosts),
                testing::ElementsAre(URLPattern::kAllUrlsPattern));
    EXPECT_TRUE(
        unpack_result->required_explicit_hosts.begin()->valid_schemes() &
        URLPattern::SCHEME_FILE);
    EXPECT_THAT(
        GetPatternsAsStrings(unpack_result->restricted_file_scheme_patterns),
        testing::IsEmpty());
  }
}

// Tests that unpacking a pattern that explicitly specifies the file:-scheme is
// properly placed into the |restricted_file_scheme_patterns| set.
TEST(ExtensionPermissionsAPIHelpers, Unpack_FileSchemes_Specific) {
  constexpr char kFilePattern[] = "file:///*";

  // Without file access, the file:-scheme pattern should be populated into
  // |restricted_file_scheme_patterns|.
  {
    URLPattern file_pattern(Extension::kValidHostPermissionSchemes,
                            kFilePattern);
    PermissionSet required_permissions(
        APIPermissionSet(), ManifestPermissionSet(),
        URLPatternSet({file_pattern}), URLPatternSet());

    Permissions permissions_object;
    permissions_object.origins = std::make_unique<std::vector<std::string>>(
        std::vector<std::string>({kFilePattern}));

    std::string error;
    constexpr bool kHasFileAccess = false;
    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, required_permissions,
                            PermissionSet(), kHasFileAccess, &error);
    ASSERT_TRUE(unpack_result) << error;

    EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_explicit_hosts),
                testing::IsEmpty());
    EXPECT_THAT(
        GetPatternsAsStrings(unpack_result->restricted_file_scheme_patterns),
        testing::ElementsAre(kFilePattern));
    EXPECT_TRUE(unpack_result->restricted_file_scheme_patterns.begin()
                    ->valid_schemes() &
                URLPattern::SCHEME_FILE);
  }

  // With file access, the file:-scheme pattern should be populated into
  // |required_explicit_hosts| (since it's not restricted).
  {
    URLPattern file_pattern(Extension::kValidHostPermissionSchemes,
                            kFilePattern);
    PermissionSet required_permissions(
        APIPermissionSet(), ManifestPermissionSet(),
        URLPatternSet({file_pattern}), URLPatternSet());

    Permissions permissions_object;
    permissions_object.origins = std::make_unique<std::vector<std::string>>(
        std::vector<std::string>({kFilePattern}));

    std::string error;
    constexpr bool kHasFileAccess = true;
    std::unique_ptr<UnpackPermissionSetResult> unpack_result =
        UnpackPermissionSet(permissions_object, required_permissions,
                            PermissionSet(), kHasFileAccess, &error);
    ASSERT_TRUE(unpack_result) << error;

    EXPECT_THAT(GetPatternsAsStrings(unpack_result->required_explicit_hosts),
                testing::ElementsAre(kFilePattern));
    EXPECT_TRUE(
        unpack_result->required_explicit_hosts.begin()->valid_schemes() &
        URLPattern::SCHEME_FILE);
    EXPECT_THAT(
        GetPatternsAsStrings(unpack_result->restricted_file_scheme_patterns),
        testing::IsEmpty());
  }
}

// Tests that unpacking a UsbDevicePermission with a list of USB device IDs
// preserves the device list in the result object.
TEST(ExtensionPermissionsAPIHelpers, Unpack_UsbDevicePermission) {
  constexpr char kDeviceListJson[] = R"([{"productId":2,"vendorId":1}])";
  constexpr char kUsbDevicesPermissionJson[] =
      R"(usbDevices|[{"productId":2,"vendorId":1}])";

  auto device_list = base::JSONReader::Read(kDeviceListJson);
  ASSERT_TRUE(device_list) << "Failed to parse device list JSON.";

  auto usb_device_permission = std::make_unique<UsbDevicePermission>(
      PermissionsInfo::GetInstance()->GetByID(APIPermission::ID::kUsbDevice));
  std::string error;
  std::vector<std::string> unhandled_permissions;
  bool from_value_result = usb_device_permission->FromValue(
      &device_list.value(), &error, &unhandled_permissions);
  ASSERT_TRUE(from_value_result);
  EXPECT_TRUE(unhandled_permissions.empty());

  APIPermissionSet api_permission_set;
  api_permission_set.insert(usb_device_permission->Clone());
  PermissionSet optional_permissions(std::move(api_permission_set),
                                     ManifestPermissionSet(), URLPatternSet(),
                                     URLPatternSet());

  Permissions permissions_object;
  permissions_object.permissions = std::make_unique<std::vector<std::string>>(
      std::vector<std::string>({kUsbDevicesPermissionJson}));
  constexpr bool kHasFileAccess = false;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      UnpackPermissionSet(permissions_object, PermissionSet(),
                          optional_permissions, kHasFileAccess, &error);

  ASSERT_TRUE(unpack_result) << error;

  ASSERT_EQ(1U, unpack_result->optional_apis.size());
  EXPECT_EQ(APIPermission::ID::kUsbDevice,
            unpack_result->optional_apis.begin()->id());
  EXPECT_TRUE(unpack_result->optional_apis.begin()->Contains(
      usb_device_permission.get()));
}

}  // namespace extensions
