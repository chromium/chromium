// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/chrome_features_service_provider.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
void ResponseSenderCallback(const std::string& expected_message,
                            std::unique_ptr<dbus::Response> response) {
  EXPECT_EQ(expected_message, response->ToString());
}
}  // namespace

class ChromeFeaturesServiceProviderTest : public testing::Test {
 protected:
  void GetFeatureParams(dbus::MethodCall* method_call, std::string expected) {
    provider_->GetFeatureParams(
        method_call, base::BindOnce(&ResponseSenderCallback, expected));
  }
  void IsFeatureEnabled(dbus::MethodCall* method_call, std::string expected) {
    provider_->IsFeatureEnabled(
        method_call, base::BindOnce(&ResponseSenderCallback, expected));
  }
  std::unique_ptr<ChromeFeaturesServiceProvider> provider_;
};

TEST_F(ChromeFeaturesServiceProviderTest, IsFeatureEnabled_Success) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  const char enabled[] = "CrOSLateBootA";
  const char disabled[] = "";
  feature_list->InitFromCommandLine(enabled, disabled);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  const char kExpectedMessage[] =
      R"--(message_type: MESSAGE_METHOD_RETURN
signature: b
reply_serial: 123

bool true
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("CrOSLateBootA");

  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  IsFeatureEnabled(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, IsFeatureEnabled_UnknownFeature) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  const char enabled[] = "CrOSLateBootA";
  const char disabled[] = "";
  feature_list->InitFromCommandLine(enabled, disabled);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  const char kExpectedMessage[] =
      R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Chrome can't get state for 'CrOSLateBootB'; feature_library will decide"
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("CrOSLateBootB");

  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  IsFeatureEnabled(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, IsFeatureEnabled_InvalidPrefix) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  const char enabled[] = "CrOSLateBootA";
  const char disabled[] = "";
  feature_list->InitFromCommandLine(enabled, disabled);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  const char kExpectedMessage[] =
      R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Invalid prefix for feature name: 'B'. Want CrOSLateBoot"
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("B");

  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  IsFeatureEnabled(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, IsFeatureEnabled_InvalidInput) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  const char enabled[] = "";
  const char disabled[] = "";
  feature_list->InitFromCommandLine(enabled, disabled);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  const char kExpectedMessage[] =
      R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Missing or invalid feature_name string arg."
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(true);

  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  IsFeatureEnabled(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, GetFeatureParams_Success) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  const char enabled[] = "CrOSLateBootA:key1/value1/key2/value2,CrOSLateBootB";
  const char disabled[] = "CrOSLateBootC";
  feature_list->InitFromCommandLine(enabled, disabled);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  const char kExpectedMessage[] =
      R"--(message_type: MESSAGE_METHOD_RETURN
signature: a{s(bba{ss})}
reply_serial: 123

array [
  dict entry {
    string "CrOSLateBootA"
    struct {
      bool true
      bool true
      array [
        dict entry {
          string "key1"
          string "value1"
        }
        dict entry {
          string "key2"
          string "value2"
        }
      ]
    }
  }
  dict entry {
    string "CrOSLateBootB"
    struct {
      bool true
      bool true
      array [
      ]
    }
  }
  dict entry {
    string "CrOSLateBootC"
    struct {
      bool true
      bool false
      array [
      ]
    }
  }
  dict entry {
    string "CrOSLateBootD"
    struct {
      bool false
      bool false
      array [
      ]
    }
  }
]
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("s", &array_writer);
  array_writer.AppendString("CrOSLateBootA");
  array_writer.AppendString("CrOSLateBootB");
  array_writer.AppendString("CrOSLateBootC");
  array_writer.AppendString("CrOSLateBootD");
  writer.CloseContainer(&array_writer);

  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  GetFeatureParams(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, GetFeatureParams_NoInput) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  feature_list->InitFromCommandLine("", "");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  constexpr char kExpectedMessage[] = R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Could not pop string array of feature names"
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  GetFeatureParams(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, GetFeatureParams_BadInput) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  feature_list->InitFromCommandLine("", "");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  constexpr char kExpectedMessage[] = R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Could not pop string array of feature names"
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString("CrOSLateBootA");  // not in an array!
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  GetFeatureParams(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, GetFeatureParams_BadArrayEntry) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  feature_list->InitFromCommandLine("", "");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  constexpr char kExpectedMessage[] = R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Missing or invalid feature_name string arg in array."
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("b", &array_writer);
  array_writer.AppendBool(true);  // wrong type
  writer.CloseContainer(&array_writer);
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  GetFeatureParams(&method_call, kExpectedMessage);
}

TEST_F(ChromeFeaturesServiceProviderTest, GetFeatureParams_BadNameFormat) {
  auto feature_list = std::make_unique<base::FeatureList>();
  auto feature_list_accessor = feature_list->ConstructAccessor();
  feature_list->InitFromCommandLine("", "");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  provider_ = std::make_unique<ChromeFeaturesServiceProvider>(
      std::move(feature_list_accessor));

  constexpr char kExpectedMessage[] = R"--(message_type: MESSAGE_ERROR
error_name: org.freedesktop.DBus.Error.InvalidArgs
signature: s
reply_serial: 123

string "Invalid prefix for feature name: 'B'. Want CrOSLateBoot"
)--";

  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  dbus::MessageWriter writer(&method_call);
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("s", &array_writer);
  array_writer.AppendString("CrOSLateBootA");
  array_writer.AppendString("B");  // missing prefix!
  writer.CloseContainer(&array_writer);
  // Not setting the serial causes a crash.
  method_call.SetSerial(123);
  GetFeatureParams(&method_call, kExpectedMessage);
}

}  // namespace ash
