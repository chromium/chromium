// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const char kAllowlistedExtensionID[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

// Tests that ChromePermissionMessageProvider produces the expected messages for
// various combinations of app/extension permissions.
class PermissionMessageCombinationsUnittest : public testing::Test {
 public:
  PermissionMessageCombinationsUnittest()
      : message_provider_(new ChromePermissionMessageProvider()),
        allowlisted_extension_id_(kAllowlistedExtensionID) {}
  ~PermissionMessageCombinationsUnittest() override {}

  // Overridden from testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
  }

 protected:
  // Create and install an app or extension with the given manifest JSON string.
  // Single-quotes in the string will be replaced with double quotes.
  void CreateAndInstall(const std::string& json_manifest) {
    std::string json_manifest_with_double_quotes = json_manifest;
    std::replace(json_manifest_with_double_quotes.begin(),
                 json_manifest_with_double_quotes.end(), '\'', '"');
    app_ = env_.MakeExtension(
        *base::test::ParseJsonDeprecated(json_manifest_with_double_quotes),
        kAllowlistedExtensionID);
  }

  // Checks whether the currently installed app or extension produces the given
  // permission messages. Call this after installing an app with the expected
  // permission messages. The messages are tested for existence in any order.
  testing::AssertionResult CheckManifestProducesPermissions() {
    return VerifyNoPermissionMessages(app_->permissions_data());
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1) {
    return VerifyOnePermissionMessage(app_->permissions_data(),
                                      expected_message_1);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2) {
    return VerifyTwoPermissionMessages(app_->permissions_data(),
                                       expected_message_1, expected_message_2,
                                       false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    return VerifyPermissionMessages(app_->permissions_data(), expected_messages,
                                    false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    return VerifyPermissionMessages(app_->permissions_data(), expected_messages,
                                    false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::string& expected_message_2,
      const std::string& expected_message_3,
      const std::string& expected_message_4,
      const std::string& expected_message_5) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    expected_messages.push_back(expected_message_5);
    return VerifyPermissionMessages(app_->permissions_data(), expected_messages,
                                    false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::vector<std::string>& expected_submessages_1) {
    return VerifyOnePermissionMessageWithSubmessages(
        app_->permissions_data(), expected_message_1, expected_submessages_1);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::vector<std::string>& expected_submessages_1,
      const std::string& expected_message_2,
      const std::vector<std::string>& expected_submessages_2) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    std::vector<std::vector<std::string>> expected_submessages;
    expected_submessages.push_back(expected_submessages_1);
    expected_submessages.push_back(expected_submessages_2);
    return VerifyPermissionMessagesWithSubmessages(app_->permissions_data(),
                                                   expected_messages,
                                                   expected_submessages, false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::vector<std::string>& expected_submessages_1,
      const std::string& expected_message_2,
      const std::vector<std::string>& expected_submessages_2,
      const std::string& expected_message_3,
      const std::vector<std::string>& expected_submessages_3) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    std::vector<std::vector<std::string>> expected_submessages;
    expected_submessages.push_back(expected_submessages_1);
    expected_submessages.push_back(expected_submessages_2);
    expected_submessages.push_back(expected_submessages_3);
    return VerifyPermissionMessagesWithSubmessages(app_->permissions_data(),
                                                   expected_messages,
                                                   expected_submessages, false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::vector<std::string>& expected_submessages_1,
      const std::string& expected_message_2,
      const std::vector<std::string>& expected_submessages_2,
      const std::string& expected_message_3,
      const std::vector<std::string>& expected_submessages_3,
      const std::string& expected_message_4,
      const std::vector<std::string>& expected_submessages_4) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    std::vector<std::vector<std::string>> expected_submessages;
    expected_submessages.push_back(expected_submessages_1);
    expected_submessages.push_back(expected_submessages_2);
    expected_submessages.push_back(expected_submessages_3);
    expected_submessages.push_back(expected_submessages_4);
    return VerifyPermissionMessagesWithSubmessages(app_->permissions_data(),
                                                   expected_messages,
                                                   expected_submessages, false);
  }
  testing::AssertionResult CheckManifestProducesPermissions(
      const std::string& expected_message_1,
      const std::vector<std::string>& expected_submessages_1,
      const std::string& expected_message_2,
      const std::vector<std::string>& expected_submessages_2,
      const std::string& expected_message_3,
      const std::vector<std::string>& expected_submessages_3,
      const std::string& expected_message_4,
      const std::vector<std::string>& expected_submessages_4,
      const std::string& expected_message_5,
      const std::vector<std::string>& expected_submessages_5) {
    std::vector<std::string> expected_messages;
    expected_messages.push_back(expected_message_1);
    expected_messages.push_back(expected_message_2);
    expected_messages.push_back(expected_message_3);
    expected_messages.push_back(expected_message_4);
    expected_messages.push_back(expected_message_5);
    std::vector<std::vector<std::string>> expected_submessages;
    expected_submessages.push_back(expected_submessages_1);
    expected_submessages.push_back(expected_submessages_2);
    expected_submessages.push_back(expected_submessages_3);
    expected_submessages.push_back(expected_submessages_4);
    expected_submessages.push_back(expected_submessages_5);
    return VerifyPermissionMessagesWithSubmessages(app_->permissions_data(),
                                                   expected_messages,
                                                   expected_submessages, false);
  }

 private:
  extensions::TestExtensionEnvironment env_;
  std::unique_ptr<ChromePermissionMessageProvider> message_provider_;
  scoped_refptr<const Extension> app_;
  // Add a known extension id to the explicit allowlist so we can test all
  // permissions. This ID will be used for each test app.
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlisted_extension_id_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMessageCombinationsUnittest);
};

// Test that the USB, Bluetooth and Serial permissions do not coalesce on their
// own, but do coalesce when more than 1 is present.
TEST_F(PermissionMessageCombinationsUnittest, USBSerialBluetoothCoalescing) {
  // Test that the USB permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor"));

  // Test that the serial permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'serial'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Access your serial devices"));

  // Test that the bluetooth permission does not coalesce on its own.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'bluetooth': {}"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access information about Bluetooth devices paired with your system and "
      "discover nearby Bluetooth devices."));

  // Test that the bluetooth permission does not coalesce on its own, even
  // when it specifies additional permissions.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'bluetooth': {"
      "    'uuids': ['1105', '1106']"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access information about Bluetooth devices paired with your system and "
      "discover nearby Bluetooth devices."));

  // Test that the USB and Serial permissions coalesce.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your serial devices"));

  // Test that the USB, Serial and Bluetooth permissions coalesce.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial'"
      "  ],"
      "  'bluetooth': {}"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your Bluetooth and Serial devices"));

  // Test that the USB, Serial and Bluetooth permissions coalesce even when
  // Bluetooth specifies multiple additional permissions.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': [{ 'vendorId': 123, 'productId': 456 }] },"
      "    'serial'"
      "  ],"
      "  'bluetooth': {"
      "    'uuids': ['1105', '1106'],"
      "    'socket': true,"
      "    'low_energy': true"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor",
      "Access your Bluetooth and Serial devices"));
}

// Test that the History permission takes precedence over the Tabs permission,
// and that the Sessions permission modifies this final message.
TEST_F(PermissionMessageCombinationsUnittest, TabsHistorySessionsCoalescing) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Read your browsing history"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'sessions'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'history'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your browsing history"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'history', 'sessions'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your browsing history on all your signed-in devices"));
}

// Test that the fileSystem permission produces no messages by itself, unless it
// has both the 'write' and 'directory' additional permissions, in which case it
// displays a message.
TEST_F(PermissionMessageCombinationsUnittest, FileSystemReadWriteCoalescing) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem', {'fileSystem': ['retainEntries', 'write']}"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'fileSystem', {'fileSystem': ["
      "      'retainEntries', 'write', 'directory'"
      "    ]}"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Write to files and folders that you open in the application"));
}

// Check that host permission messages are generated correctly when URLs are
// entered as permissions.
TEST_F(PermissionMessageCombinationsUnittest, HostsPermissionMessages) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on www.blogger.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites and "
      "www.blogger.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites, all news.com sites, "
      "and www.blogger.com"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "  ]"
      "}");
  std::vector<std::string> submessages;
  submessages.push_back("All google.com sites");
  submessages.push_back("All news.com sites");
  submessages.push_back("www.blogger.com");
  submessages.push_back("www.foobar.com");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on a number of websites", submessages));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");
  submessages.clear();
  submessages.push_back("All go.com sites");
  submessages.push_back("All google.com sites");
  submessages.push_back("All news.com sites");
  submessages.push_back("www.blogger.com");
  submessages.push_back("www.foobar.com");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on a number of websites", submessages));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.go.com/',"
      "    'chrome://favicon/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all go.com sites",
      "Read the icons of the websites you visit"));

  // Having the 'all sites' permission doesn't change the permission message,
  // since its pseudo-granted at runtime.
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'http://*.go.com/',"
      "    'chrome://favicon/',"
      "    'http://*.*',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all go.com sites",
      "Read the icons of the websites you visit"));
}

// Check that permission messages are generated correctly for
// SocketsManifestPermission, which has host-like permission messages.
TEST_F(PermissionMessageCombinationsUnittest,
       SocketsManifestPermissionMessages) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'udp': {'send': '*'},"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'udp': {'send': ':99'},"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcp': {'connect': '127.0.0.1:80'},"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the device named 127.0.0.1"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcp': {'connect': 'www.example.com:23'},"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the device named www.example.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcpServer': {'listen': '127.0.0.1:80'}"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the device named 127.0.0.1"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcpServer': {'listen': ':8080'}"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcpServer': {"
      "      'listen': ["
      "        '127.0.0.1:80',"
      "        'www.google.com',"
      "        'www.example.com:*',"
      "        'www.foo.com:200',"
      "        'www.bar.com:200'"
      "      ]"
      "    }"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the devices named: 127.0.0.1 www.bar.com "
      "www.example.com www.foo.com www.google.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcp': {"
      "      'connect': ["
      "        'www.abc.com:*',"
      "        'www.mywebsite.com:320',"
      "        'www.freestuff.com',"
      "        'www.foo.com:34',"
      "        'www.test.com'"
      "      ]"
      "    },"
      "    'tcpServer': {"
      "      'listen': ["
      "        '127.0.0.1:80',"
      "        'www.google.com',"
      "        'www.example.com:*',"
      "        'www.foo.com:200',"
      "      ]"
      "    }"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the devices named: 127.0.0.1 www.abc.com "
      "www.example.com www.foo.com www.freestuff.com www.google.com "
      "www.mywebsite.com www.test.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'sockets': {"
      "    'tcp': {'send': '*:*'},"
      "    'tcpServer': {'listen': '*:*'},"
      "  }"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));
}

// Check that permission messages are generated correctly for
// MediaGalleriesPermission (an API permission with custom messages).
TEST_F(PermissionMessageCombinationsUnittest,
       MediaGalleriesPermissionMessages) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'mediaGalleries': ['read'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'mediaGalleries': ['read', 'allAutoDetected'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access photos, music, and other media from your computer"));

  // TODO(sashab): Add a test for the
  // IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE message (generated
  // with the 'read' and 'copyTo' permissions, but not the 'delete' permission),
  // if it's possible to get this message. Otherwise, remove it from the code.

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'mediaGalleries': ['read', 'delete', 'allAutoDetected'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and delete photos, music, and other media from your computer"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'mediaGalleries':"
      "      [ 'read', 'delete', 'copyTo', 'allAutoDetected' ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read, change and delete photos, music, and other media from your "
      "computer"));

  // Without the allAutoDetected permission, there should be no install-time
  // permission messages.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'mediaGalleries': ['read', 'delete', 'copyTo'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());
}

// TODO(sashab): Add tests for SettingsOverrideAPIPermission (an API permission
// with custom messages).

// Check that permission messages are generated correctly for SocketPermission
// (an API permission with custom messages).
TEST_F(PermissionMessageCombinationsUnittest, SocketPermissionMessages) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ['tcp-connect:*:*'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ["
      "      'tcp-connect:*:443',"
      "      'tcp-connect:*:50032',"
      "      'tcp-connect:*:23',"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ['tcp-connect:foo.example.com:443'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the device named foo.example.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ['tcp-connect:foo.example.com:443', 'udp-send-to'] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ["
      "      'tcp-connect:foo.example.com:443',"
      "      'udp-send-to:test.ping.com:50032',"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the devices named: foo.example.com test.ping.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ["
      "      'tcp-connect:foo.example.com:443',"
      "      'udp-send-to:test.ping.com:50032',"
      "      'udp-send-to:www.ping.com:50032',"
      "      'udp-send-to:test2.ping.com:50032',"
      "      'udp-bind:test.ping.com:50032',"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with the devices named: foo.example.com test.ping.com "
      "test2.ping.com www.ping.com"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'socket': ["
      "      'tcp-connect:foo.example.com:443',"
      "      'udp-send-to:test.ping.com:50032',"
      "      'tcp-connect:*:23',"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Exchange data with any device on the local network or internet"));
}

// Check that permission messages are generated correctly for
// USBDevicePermission (an API permission with custom messages).
TEST_F(PermissionMessageCombinationsUnittest, USBDevicePermissionMessages) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': ["
      "      { 'vendorId': 0, 'productId': 0 },"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from an unknown vendor"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': ["
      "      { 'vendorId': 4179, 'productId': 529 },"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access USB devices from Immanuel Electronics Co., Ltd"));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': ["
      "      { 'vendorId': 6353, 'productId': 8192 },"
      "    ] }"
      "  ]"
      "}");
  ASSERT_TRUE(
      CheckManifestProducesPermissions("Access USB devices from Google Inc."));

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    { 'usbDevices': ["
      "      { 'vendorId': 4179, 'productId': 529 },"
      "      { 'vendorId': 6353, 'productId': 8192 },"
      "    ] }"
      "  ]"
      "}");
  std::vector<std::string> submessages;
  submessages.push_back("unknown devices from Immanuel Electronics Co., Ltd");
  submessages.push_back("unknown devices from Google Inc.");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access any of these USB devices", submessages));

  // TODO(sashab): Add a test with a valid product/vendor USB device.
}

// Test that hosted apps are not given any messages for host permissions.
TEST_F(PermissionMessageCombinationsUnittest,
       PackagedAppsHaveNoHostPermissions) {
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions());

  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'serial',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Access your serial devices"));
}

// Test various apps with lots of permissions, including those with no
// permission messages, or those that only apply to apps or extensions even when
// the given manifest is for a different type.
TEST_F(PermissionMessageCombinationsUnittest, PermissionMessageCombos) {
  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'bookmarks',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'unlimitedStorage',"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change your data on all google.com sites and www.blogger.com",
      "Read your browsing history", "Read and change your bookmarks"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'sessions',"
      "    'bookmarks',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");
  std::vector<std::string> submessages;
  submessages.push_back("All go.com sites");
  submessages.push_back("All google.com sites");
  submessages.push_back("All news.com sites");
  submessages.push_back("www.blogger.com");
  submessages.push_back("www.foobar.com");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices",
      std::vector<std::string>(), "Read and change your bookmarks",
      std::vector<std::string>(),
      "Read and change your data on a number of websites", submessages));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs',"
      "    'sessions',"
      "    'bookmarks',"
      "    'accessibilityFeatures.read',"
      "    'accessibilityFeatures.modify',"
      "    'alarms',"
      "    'browsingData',"
      "    'cookies',"
      "    'desktopCapture',"
      "    'gcm',"
      "    'topSites',"
      "    'storage',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");

  submessages.clear();
  submessages.push_back("All go.com sites");
  submessages.push_back("All google.com sites");
  submessages.push_back("All news.com sites");
  submessages.push_back("www.blogger.com");
  submessages.push_back("www.foobar.com");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read your browsing history on all your signed-in devices",
      std::vector<std::string>(), "Capture content of your screen",
      std::vector<std::string>(), "Read and change your bookmarks",
      std::vector<std::string>(),
      "Read and change your data on a number of websites", submessages,
      "Read and change your accessibility settings",
      std::vector<std::string>()));

  // Create an App instead, ensuring that the host permission messages are not
  // added.
  CreateAndInstall(
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': ["
      "    'contextMenus',"
      "    'permissions',"
      "    'accessibilityFeatures.read',"
      "    'accessibilityFeatures.modify',"
      "    'alarms',"
      "    'power',"
      "    'cookies',"
      "    'serial',"
      "    'usb',"
      "    'storage',"
      "    'gcm',"
      "    'topSites',"
      "    'storage',"
      "    'unlimitedStorage',"
      "    'syncFileSystem',"
      "    'http://www.blogger.com/',"
      "    'http://*.google.com/',"
      "    'http://*.news.com/',"
      "    'http://www.foobar.com/',"
      "    'http://*.go.com/',"
      "  ]"
      "}");

  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Access your serial devices", "Store data in your Google Drive account",
      "Read and change your accessibility settings"));

}

// Tests that the deprecated 'plugins' manifest key produces no permission.
TEST_F(PermissionMessageCombinationsUnittest, PluginPermission) {
  CreateAndInstall(
      "{"
      "  'plugins': ["
      "    { 'path': 'extension_plugin.dll' }"
      "  ]"
      "}");

  ASSERT_TRUE(CheckManifestProducesPermissions());
}

TEST_F(PermissionMessageCombinationsUnittest, ClipboardPermissionMessages) {
  const char kManifest[] =
      "{"
      "  'app': {"
      "    'background': {"
      "      'scripts': ['background.js']"
      "    }"
      "  },"
      "  'permissions': [%s]"
      "}";

  CreateAndInstall(base::StringPrintf(kManifest, "'clipboardRead'"));
  ASSERT_TRUE(CheckManifestProducesPermissions("Read data you copy and paste"));

  CreateAndInstall(
      base::StringPrintf(kManifest, "'clipboardRead', 'clipboardWrite'"));
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and modify data you copy and paste"));

  CreateAndInstall(base::StringPrintf(kManifest, "'clipboardWrite'"));
  ASSERT_TRUE(
      CheckManifestProducesPermissions("Modify data you copy and paste"));
}

TEST_F(PermissionMessageCombinationsUnittest, NewTabPagePermissionMessages) {
  const char kManifest[] =
      "{"
      "  'chrome_url_overrides': {"
      "    'newtab': 'newtab.html'"
      "  }"
      "}";

  CreateAndInstall(kManifest);
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Replace the page you see when opening a new tab"));
}

TEST_F(PermissionMessageCombinationsUnittest,
       DeclarativeNetRequestFeedbackPermissionMessages) {
  // Set the current channel to trunk.
  ScopedCurrentChannel scoped_channel(version_info::Channel::UNKNOWN);

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'declarativeNetRequestFeedback'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Read your browsing history"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    'tabs', 'declarativeNetRequestFeedback'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions("Read your browsing history"));

  CreateAndInstall(
      "{"
      "  'permissions': ["
      "    '<all_urls>', 'declarativeNetRequestFeedback'"
      "  ]"
      "}");
  ASSERT_TRUE(CheckManifestProducesPermissions(
      "Read and change all your data on the websites you visit"));
}

// TODO(sashab): Add a test that checks that messages are generated correctly
// for withheld permissions, when an app is granted the 'all sites' permission.

// TODO(sashab): Add a test that ensures that all permissions that can generate
// a coalesced message can also generate a message on their own (i.e. ensure
// that no permissions only modify other permissions).

// TODO(sashab): Add a test for every permission message combination that can
// generate a message.

// TODO(aboxhall): Add tests for the automation API permission messages.

}  // namespace extensions
