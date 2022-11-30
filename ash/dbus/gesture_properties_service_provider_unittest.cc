// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/gesture_properties_service_provider.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/ozone/testhelpers/mock_gesture_properties_service.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InvokeArgument;
using ::testing::Unused;

namespace ash {

namespace {

bool expect_bool(dbus::MessageReader* reader) {
  bool result;
  EXPECT_TRUE(reader->PopBool(&result));
  return result;
}

int16_t expect_int16(dbus::MessageReader* reader) {
  int16_t result;
  EXPECT_TRUE(reader->PopInt16(&result));
  return result;
}

uint32_t expect_uint32(dbus::MessageReader* reader) {
  uint32_t result;
  EXPECT_TRUE(reader->PopUint32(&result));
  return result;
}

int32_t expect_int32(dbus::MessageReader* reader) {
  int32_t result;
  EXPECT_TRUE(reader->PopInt32(&result));
  return result;
}

std::string expect_string(dbus::MessageReader* reader) {
  std::string result;
  EXPECT_TRUE(reader->PopString(&result));
  return result;
}

double expect_double(dbus::MessageReader* reader) {
  double result;
  EXPECT_TRUE(reader->PopDouble(&result));
  return result;
}

}  // namespace

class GesturePropertiesServiceProviderTest : public testing::Test {
 public:
  GesturePropertiesServiceProviderTest() {
    mock_service_ = std::make_unique<MockGesturePropertiesService>();
    ON_CALL(*mock_service_, ListDevices(_))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeListDevices));
    ON_CALL(*mock_service_, ListProperties(_, _))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeListProperties));
    ON_CALL(*mock_service_, GetProperty(_, _, _))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeGetProperty));
    ON_CALL(*mock_service_, SetProperty(_, _, _, _))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeSetProperty));

    service_provider_ = std::make_unique<GesturePropertiesServiceProvider>();
    service_provider_->set_service_for_test(mock_service_.get());
  }

  GesturePropertiesServiceProviderTest(
      const GesturePropertiesServiceProviderTest&) = delete;
  GesturePropertiesServiceProviderTest& operator=(
      const GesturePropertiesServiceProviderTest&) = delete;

  ~GesturePropertiesServiceProviderTest() override { test_helper_.TearDown(); }

  void FakeListDevices(
      ui::ozone::mojom::GesturePropertiesService::ListDevicesCallback
          callback) {
    std::move(callback).Run(list_devices_response_);
  }

  void FakeListProperties(
      Unused,
      ui::ozone::mojom::GesturePropertiesService::ListPropertiesCallback
          callback) {
    std::move(callback).Run(list_properties_response_);
  }

  void FakeGetProperty(
      Unused,
      Unused,
      ui::ozone::mojom::GesturePropertiesService::GetPropertyCallback
          callback) {
    std::move(callback).Run(get_property_read_only_,
                            std::move(get_property_response_));
  }

  void FakeSetProperty(
      Unused,
      Unused,
      ui::ozone::mojom::GesturePropValuePtr values,
      ui::ozone::mojom::GesturePropertiesService::SetPropertyCallback
          callback) {
    set_property_values_ = std::move(values);
    std::move(callback).Run(set_property_error_code_);
  }

 protected:
  void CallDBusMethod(std::string name,
                      dbus::MethodCall* method_call,
                      std::unique_ptr<dbus::Response>& response) {
    test_helper_.SetUp(
        chromeos::kGesturePropertiesServiceName,
        dbus::ObjectPath(chromeos::kGesturePropertiesServicePath),
        chromeos::kGesturePropertiesServiceInterface, name,
        service_provider_.get());
    response = test_helper_.CallMethod(method_call);
    ASSERT_TRUE(response);
  }

  void CallWithoutParameters(std::string name,
                             std::unique_ptr<dbus::Response>& response) {
    dbus::MethodCall method_call(chromeos::kGesturePropertiesServiceInterface,
                                 name);
    CallDBusMethod(name, &method_call, response);
  }

  void CallGetProperty(int32_t device_id,
                       std::string name,
                       std::unique_ptr<dbus::Response>& response) {
    dbus::MethodCall method_call(
        chromeos::kGesturePropertiesServiceInterface,
        chromeos::kGesturePropertiesServiceGetPropertyMethod);

    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(device_id);
    writer.AppendString(name);
    CallDBusMethod(chromeos::kGesturePropertiesServiceGetPropertyMethod,
                   &method_call, response);
  }

  void CheckMethodErrorsWithNoParameters(std::string name) {
    std::unique_ptr<dbus::Response> response;
    CallWithoutParameters(name, response);
    EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::flat_map<int, std::string> list_devices_response_ = {};
  std::vector<std::string> list_properties_response_ = {};
  bool get_property_read_only_ = true;
  ui::ozone::mojom::GesturePropValuePtr get_property_response_ = nullptr;

  ui::ozone::mojom::GesturePropValuePtr set_property_values_ = nullptr;
  ui::ozone::mojom::SetGesturePropErrorCode set_property_error_code_ =
      ui::ozone::mojom::SetGesturePropErrorCode::SUCCESS;

  std::unique_ptr<MockGesturePropertiesService> mock_service_;

  std::unique_ptr<GesturePropertiesServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

TEST_F(GesturePropertiesServiceProviderTest, ListDevicesEmpty) {
  list_devices_response_ = {};
  EXPECT_CALL(*mock_service_, ListDevices(_));

  std::unique_ptr<dbus::Response> response;
  CallWithoutParameters(chromeos::kGesturePropertiesServiceListDevicesMethod,
                        response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(0u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, ListDevicesSuccess) {
  list_devices_response_ = {
      {4, "dev 1"},
      {7, "dev 2"},
  };
  EXPECT_CALL(*mock_service_, ListDevices(_));

  std::unique_ptr<dbus::Response> response;
  CallWithoutParameters(chromeos::kGesturePropertiesServiceListDevicesMethod,
                        response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(2u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));

  dbus::MessageReader dict_entry_reader(nullptr);
  ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));
  EXPECT_EQ(4, expect_int32(&dict_entry_reader));
  EXPECT_EQ("dev 1", expect_string(&dict_entry_reader));

  ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));
  EXPECT_EQ(7, expect_int32(&dict_entry_reader));
  EXPECT_EQ("dev 2", expect_string(&dict_entry_reader));

  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, ListPropertiesEmpty) {
  list_properties_response_ = {};
  EXPECT_CALL(*mock_service_, ListProperties(4, _));

  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListPropertiesMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  CallDBusMethod(chromeos::kGesturePropertiesServiceListPropertiesMethod,
                 &method_call, response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(0u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, ListPropertiesSuccess) {
  list_properties_response_ = {"prop 1", "prop 2"};
  EXPECT_CALL(*mock_service_, ListProperties(4, _));

  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListPropertiesMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  CallDBusMethod(chromeos::kGesturePropertiesServiceListPropertiesMethod,
                 &method_call, response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(2u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  EXPECT_EQ("prop 1", expect_string(&array_reader));
  EXPECT_EQ("prop 2", expect_string(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, ListPropertiesMissingParameter) {
  CheckMethodErrorsWithNoParameters(
      chromeos::kGesturePropertiesServiceListPropertiesMethod);
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertySuccessInts) {
  get_property_read_only_ = false;
  get_property_response_ =
      ui::ozone::mojom::GesturePropValue::NewInts({1, 2, 4});
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(false, expect_bool(&reader));
  EXPECT_EQ(3u, expect_uint32(&reader));
  dbus::MessageReader variant_reader(nullptr);
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(variant_reader.PopArray(&array_reader));
  EXPECT_EQ(1, expect_int32(&array_reader));
  EXPECT_EQ(2, expect_int32(&array_reader));
  EXPECT_EQ(4, expect_int32(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertySuccessShorts) {
  get_property_read_only_ = false;
  get_property_response_ =
      ui::ozone::mojom::GesturePropValue::NewShorts({1, 2, 4});
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(false, expect_bool(&reader));
  EXPECT_EQ(3u, expect_uint32(&reader));
  dbus::MessageReader variant_reader(nullptr);
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(variant_reader.PopArray(&array_reader));
  EXPECT_EQ(1, expect_int16(&array_reader));
  EXPECT_EQ(2, expect_int16(&array_reader));
  EXPECT_EQ(4, expect_int16(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertySuccessBools) {
  get_property_read_only_ = false;
  get_property_response_ =
      ui::ozone::mojom::GesturePropValue::NewBools({true, false});
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(false, expect_bool(&reader));
  EXPECT_EQ(2u, expect_uint32(&reader));
  dbus::MessageReader variant_reader(nullptr);
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(variant_reader.PopArray(&array_reader));
  EXPECT_EQ(true, expect_bool(&array_reader));
  EXPECT_EQ(false, expect_bool(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertySuccessStr) {
  get_property_read_only_ = false;
  get_property_response_ = ui::ozone::mojom::GesturePropValue::NewStr("llama");
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(false, expect_bool(&reader));
  EXPECT_EQ(1u, expect_uint32(&reader));
  dbus::MessageReader variant_reader(nullptr);
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  EXPECT_EQ("llama", expect_string(&variant_reader));
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertySuccessReals) {
  get_property_read_only_ = false;
  get_property_response_ =
      ui::ozone::mojom::GesturePropValue::NewReals({3.14, 6.28});
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(false, expect_bool(&reader));
  EXPECT_EQ(2u, expect_uint32(&reader));
  dbus::MessageReader variant_reader(nullptr);
  ASSERT_TRUE(reader.PopVariant(&variant_reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(variant_reader.PopArray(&array_reader));
  EXPECT_EQ(3.14, expect_double(&array_reader));
  EXPECT_EQ(6.28, expect_double(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertyPropertyDoesntExist) {
  get_property_read_only_ = true;
  get_property_response_ = nullptr;
  EXPECT_CALL(*mock_service_, GetProperty(4, "prop 1", _));

  std::unique_ptr<dbus::Response> response;
  CallGetProperty(4, "prop 1", response);
  EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
}

TEST_F(GesturePropertiesServiceProviderTest, GetPropertyMissingParameters) {
  CheckMethodErrorsWithNoParameters(
      chromeos::kGesturePropertiesServiceGetPropertyMethod);
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertySuccessInts) {
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("i", &array_writer);
  array_writer.AppendInt32(1);
  array_writer.AppendInt32(2);
  array_writer.AppendInt32(4);
  writer.CloseContainer(&array_writer);
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_NE(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  EXPECT_THAT(set_property_values_->get_ints(), ElementsAre(1, 2, 4));
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertySuccessShorts) {
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("n", &array_writer);
  array_writer.AppendInt16(1);
  array_writer.AppendInt16(2);
  array_writer.AppendInt16(4);
  writer.CloseContainer(&array_writer);
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_NE(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  EXPECT_THAT(set_property_values_->get_shorts(), ElementsAre(1, 2, 4));
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertySuccessBools) {
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("b", &array_writer);
  array_writer.AppendBool(true);
  array_writer.AppendBool(false);
  writer.CloseContainer(&array_writer);
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_NE(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  EXPECT_THAT(set_property_values_->get_bools(), ElementsAre(true, false));
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertySuccessStr) {
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  writer.AppendString("llamas");
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_NE(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  EXPECT_EQ(set_property_values_->get_str(), "llamas");
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertySuccessReals) {
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;
  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("d", &array_writer);
  array_writer.AppendDouble(3.14);
  array_writer.AppendDouble(6.28);
  writer.CloseContainer(&array_writer);
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_NE(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  EXPECT_THAT(set_property_values_->get_reals(), ElementsAre(3.14, 6.28));
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertyError) {
  set_property_error_code_ =
      ui::ozone::mojom::SetGesturePropErrorCode::UNKNOWN_ERROR;
  EXPECT_CALL(*mock_service_, SetProperty(4, "prop 1", _, _));
  std::unique_ptr<dbus::Response> response;

  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  writer.AppendString("llamas");
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertyNoData) {
  std::unique_ptr<dbus::Response> response;

  dbus::MethodCall method_call(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendInt32(4);
  writer.AppendString("prop 1");
  CallDBusMethod(chromeos::kGesturePropertiesServiceSetPropertyMethod,
                 &method_call, response);
  EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
}

TEST_F(GesturePropertiesServiceProviderTest, SetPropertyMissingParameters) {
  CheckMethodErrorsWithNoParameters(
      chromeos::kGesturePropertiesServiceSetPropertyMethod);
}

}  // namespace ash
