// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifestUnchecked;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::Manifest;
using extensions::PermissionsData;
using extensions::WebRequestInfo;
using extensions::WebRequestInfoInitParams;
using extensions::mojom::ManifestLocation;

class ExtensionWebRequestHelpersTestWithThreadsTest
    : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override;

  raw_ptr<extensions::PermissionHelper> permission_helper_ = nullptr;
  // This extension has Web Request permissions, but no host permission.
  scoped_refptr<Extension> permissionless_extension_;
  // This extension has Web Request permissions, and *.com a host permission.
  scoped_refptr<Extension> com_extension_;
  // This extension is the same as com_extension, except it's installed from
  // ManifestLocation::kExternalPolicyDownload.
  scoped_refptr<Extension> com_policy_extension_;
};

void ExtensionWebRequestHelpersTestWithThreadsTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  permission_helper_ = extensions::PermissionHelper::Get(browser_context());

  std::string error;
  permissionless_extension_ =
      LoadManifestUnchecked("permissions", "web_request_no_host.json",
                            ManifestLocation::kInvalidLocation,
                            Extension::NO_FLAGS, "ext_id_1", &error);
  ASSERT_TRUE(permissionless_extension_.get()) << error;
  com_extension_ = LoadManifestUnchecked(
      "permissions", "web_request_com_host_permissions.json",
      ManifestLocation::kInvalidLocation, Extension::NO_FLAGS, "ext_id_2",
      &error);
  ASSERT_TRUE(com_extension_.get()) << error;
  com_policy_extension_ = LoadManifestUnchecked(
      "permissions", "web_request_com_host_permissions.json",
      ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS,
      "ext_id_3", &error);
  ASSERT_TRUE(com_policy_extension_.get()) << error;
  ExtensionRegistry::Get(browser_context())
      ->AddEnabled(permissionless_extension_);
  ExtensionRegistry::Get(browser_context())->AddEnabled(com_extension_);
  ExtensionRegistry::Get(browser_context())->AddEnabled(com_policy_extension_);
}

// Ensures that requests to extension blocklist urls can't be intercepted by
// extensions.
TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       BlocklistUpdateUrlsHidden) {
  auto create_request_params = [](const std::string& url) {
    const int kRendererProcessId = 2;
    WebRequestInfoInitParams request;
    request.url = GURL(url);
    request.render_process_id = kRendererProcessId;
    return request;
  };

  WebRequestInfo request_1(create_request_params(
      "http://www.gstatic.com/chrome/extensions/blocklist"));
  EXPECT_TRUE(
      WebRequestPermissions::HideRequest(permission_helper_, request_1));

  WebRequestInfo request_2(create_request_params(
      "https://www.gstatic.com/chrome/extensions/blocklist"));
  EXPECT_TRUE(
      WebRequestPermissions::HideRequest(permission_helper_, request_2));
}

// Ensure requests made by the local WebUINTP are hidden from extensions.
// Regression test for crbug.com/931013.
TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest, LocalWebUINTPRequests) {
  const GURL example_com("http://example.com");

  auto create_request_params =
      [&example_com](const url::Origin& initiator,
                     extensions::WebRequestResourceType web_request_type,
                     bool is_navigation_request) {
        WebRequestInfoInitParams info_params;
        info_params.url = example_com;
        info_params.initiator = initiator;
        info_params.render_process_id = -1;
        info_params.web_request_type = web_request_type;
        info_params.is_navigation_request = is_navigation_request;
        return info_params;
      };

  url::Origin ntp_origin =
      url::Origin::Create(GURL(chrome::kChromeUINewTabPageURL));

  // Sub-resource browser initiated requests are hidden from extensions.
  WebRequestInfoInitParams info_params_1 = create_request_params(
      ntp_origin, extensions::WebRequestResourceType::OTHER, false);
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper_, WebRequestInfo(std::move(info_params_1))));

  // Sub-frame navigations initiated from the local ntp should be hidden.
  WebRequestInfoInitParams info_params_2 = create_request_params(
      ntp_origin, extensions::WebRequestResourceType::SUB_FRAME, true);
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper_, WebRequestInfo(std::move(info_params_2))));

  // Sub-frame navigations initiated from a non-sensitive domain should not be
  // hidden.
  WebRequestInfoInitParams info_params_3 = create_request_params(
      url::Origin::Create(example_com),
      extensions::WebRequestResourceType::SUB_FRAME, true);
  EXPECT_FALSE(WebRequestPermissions::HideRequest(
      permission_helper_, WebRequestInfo(std::move(info_params_3))));
}

TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       TestCanExtensionAccessURL_HostPermissions) {
  const GURL url("http://example.com");
  const extensions::WebRequestResourceType kWebRequestType =
      extensions::WebRequestResourceType::OTHER;

  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, permissionless_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::DO_NOT_CHECK_HOST, std::nullopt,
                kWebRequestType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, permissionless_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                std::nullopt, kWebRequestType));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, com_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                std::nullopt, kWebRequestType));
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      WebRequestPermissions::CanExtensionAccessURL(
          permission_helper_, com_extension_->id(), url,
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          std::nullopt, kWebRequestType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, com_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_ALL_URLS, std::nullopt,
                kWebRequestType));

  std::optional<url::Origin> initiator(
      url::Origin::Create(GURL("http://www.example.org")));

  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, permissionless_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::DO_NOT_CHECK_HOST, initiator,
                kWebRequestType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, permissionless_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                initiator, kWebRequestType));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, com_extension_->id(), url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                initiator, kWebRequestType));
  // Doesn't have access to the initiator.
  EXPECT_EQ(
      PermissionsData::PageAccess::kDenied,
      WebRequestPermissions::CanExtensionAccessURL(
          permission_helper_, com_extension_->id(), url,
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          initiator, kWebRequestType));
  // Navigation requests don't need access to the initiator.
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      WebRequestPermissions::CanExtensionAccessURL(
          permission_helper_, com_extension_->id(), url,
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          initiator, extensions::WebRequestResourceType::SUB_FRAME));

  EXPECT_EQ(
      PermissionsData::PageAccess::kDenied,
      WebRequestPermissions::CanExtensionAccessURL(
          permission_helper_, com_extension_->id(), url,
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_ALL_URLS, initiator, kWebRequestType));

  // com_extension_ doesn't have host permission for .org URLs.
  const GURL org_url("http://example.org");
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, com_policy_extension_->id(), org_url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                std::nullopt, kWebRequestType));

  // Make sure that chrome:// URLs cannot be accessed.
  const GURL chrome_url("chrome://version/");
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                permission_helper_, com_policy_extension_->id(), chrome_url,
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                std::nullopt, kWebRequestType));
}
