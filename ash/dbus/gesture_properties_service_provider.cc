// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/gesture_properties_service_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

GesturePropertiesServiceProvider::GesturePropertiesServiceProvider()
    : weak_ptr_factory_(this) {}

GesturePropertiesServiceProvider::~GesturePropertiesServiceProvider() = default;

void GesturePropertiesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  auto on_exported =
      base::BindRepeating(&GesturePropertiesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr());
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListDevicesMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::ListDevices,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceListPropertiesMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::ListProperties,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceGetPropertyMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::GetProperty,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
  exported_object->ExportMethod(
      chromeos::kGesturePropertiesServiceInterface,
      chromeos::kGesturePropertiesServiceSetPropertyMethod,
      base::BindRepeating(&GesturePropertiesServiceProvider::SetProperty,
                          weak_ptr_factory_.GetWeakPtr()),
      on_exported);
}

void GesturePropertiesServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

namespace {

void GetPropertyCallback(dbus::MethodCall* method_call,
                         std::unique_ptr<dbus::Response> response,
                         dbus::ExportedObject::ResponseSender response_sender,
                         bool is_read_only,
                         ui::ozone::mojom::GesturePropValuePtr values) {
  if (values.is_null()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "The device ID or property name specified was not found."));
    return;
  }

  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter variant_writer(nullptr);
  dbus::MessageWriter array_writer(nullptr);
  writer.AppendBool(is_read_only);
  switch (values->which()) {
    case ui::ozone::mojom::GesturePropValue::Tag::kInts: {
      writer.AppendUint32(values->get_ints().size());
      writer.OpenVariant("ai", &variant_writer);
      variant_writer.AppendArrayOfInt32s(values->get_ints());
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::kShorts: {
      writer.AppendUint32(values->get_shorts().size());
      writer.OpenVariant("an", &variant_writer);
      variant_writer.OpenArray("n", &array_writer);
      for (int16_t value : values->get_shorts()) {
        array_writer.AppendInt16(value);
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::kBools: {
      writer.AppendUint32(values->get_bools().size());
      writer.OpenVariant("ab", &variant_writer);
      variant_writer.OpenArray("b", &array_writer);
      for (bool value : values->get_bools()) {
        array_writer.AppendBool(value);
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::kStr: {
      writer.AppendUint32(1);
      writer.AppendVariantOfString(values->get_str());
      break;
    }
    case ui::ozone::mojom::GesturePropValue::Tag::kReals: {
      writer.AppendUint32(values->get_reals().size());
      writer.OpenVariant("ad", &variant_writer);
      variant_writer.AppendArrayOfDoubles(values->get_reals());
      writer.CloseContainer(&variant_writer);
      break;
    }
    default: {
      // This should never happen.
      LOG(WARNING) << "No value set on GesturePropValue union; not returning "
                      "values to GetProperty call.";
      writer.AppendUint32(0);
      break;
    }
  }
  std::move(response_sender).Run(std::move(response));
}

void SetPropertyCallback(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender,
                         ui::ozone::mojom::SetGesturePropErrorCode error) {
  std::string error_message;
  switch (error) {
    case ui::ozone::mojom::SetGesturePropErrorCode::SUCCESS:
      std::move(response_sender)
          .Run(dbus::Response::FromMethodCall(method_call));
      return;
    case ui::ozone::mojom::SetGesturePropErrorCode::NOT_FOUND:
      error_message = "The device ID or property name specified was not found.";
      break;
    case ui::ozone::mojom::SetGesturePropErrorCode::READ_ONLY:
      error_message = "That property is read-only.";
      break;
    case ui::ozone::mojom::SetGesturePropErrorCode::TYPE_MISMATCH:
      error_message =
          "The property is of a different type than the value(s) "
          "provided.";
      break;
    case ui::ozone::mojom::SetGesturePropErrorCode::SIZE_MISMATCH:
      error_message =
          "The property has a different number of values to that "
          "provided.";
      break;
    case ui::ozone::mojom::SetGesturePropErrorCode::UNKNOWN_ERROR:
    default:
      error_message = "An unknown error occurred.";
      break;
  }
  LOG(ERROR) << "SetProperty error: " << error_message;
  std::move(response_sender)
      .Run(dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_INVALID_ARGS, error_message));
}

void ListDevicesCallback(std::unique_ptr<dbus::Response> response,
                         dbus::ExportedObject::ResponseSender response_sender,
                         const base::flat_map<int, std::string>& result) {
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result.size());
  dbus::MessageWriter dict_writer(nullptr);
  writer.OpenArray("{is}", &dict_writer);
  for (const auto& pair : result) {
    dbus::MessageWriter dict_entry_writer(nullptr);
    dict_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendInt32(pair.first);
    dict_entry_writer.AppendString(pair.second);
    dict_writer.CloseContainer(&dict_entry_writer);
  }
  writer.CloseContainer(&dict_writer);
  std::move(response_sender).Run(std::move(response));
}

void ListPropertiesCallback(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::vector<std::string>& result) {
  dbus::MessageWriter writer(response.get());
  writer.AppendUint32(result.size());
  writer.AppendArrayOfStrings(result);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace

void GesturePropertiesServiceProvider::ListDevices(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->ListDevices(base::BindOnce(
      &ListDevicesCallback, std::move(response), std::move(response_sender)));
}

void GesturePropertiesServiceProvider::ListProperties(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  int32_t device_id;
  if (!reader.PopInt32(&device_id)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "The device ID (int32) is missing."));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->ListProperties(
      device_id, base::BindOnce(&ListPropertiesCallback, std::move(response),
                                std::move(response_sender)));
}

void GesturePropertiesServiceProvider::GetProperty(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  int32_t device_id;
  std::string property_name;

  if (!reader.PopInt32(&device_id) || !reader.PopString(&property_name)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "The device ID (int32) and/or property name (string) is missing."));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  GetService()->GetProperty(
      device_id, property_name,
      base::BindOnce(&GetPropertyCallback, method_call, std::move(response),
                     std::move(response_sender)));
}

static ui::ozone::mojom::GesturePropValuePtr GesturePropValueFromVariant(
    dbus::MessageReader* variant_reader,
    std::string* error_message) {
  std::string string_value;
  if (variant_reader->PopString(&string_value)) {
    return ui::ozone::mojom::GesturePropValue::NewStr(string_value);
  }

  dbus::MessageReader array_reader(nullptr);
  if (!variant_reader->PopArray(&array_reader)) {
    *error_message =
        "Value(s) should be specified either as a string or an "
        "array of the appropriate type.";
    return nullptr;
  }
  switch (array_reader.GetDataType()) {
    case dbus::Message::DataType::INT32: {
      std::vector<int32_t> values = {};
      int32_t value;
      while (array_reader.PopInt32(&value)) {
        values.push_back(value);
      }
      return ui::ozone::mojom::GesturePropValue::NewInts(values);
    }
    case dbus::Message::DataType::INT16: {
      std::vector<int16_t> values = {};
      int16_t value;
      while (array_reader.PopInt16(&value)) {
        values.push_back(value);
      }
      return ui::ozone::mojom::GesturePropValue::NewShorts(values);
    }
    case dbus::Message::DataType::BOOL: {
      std::vector<bool> values = {};
      bool value;
      while (array_reader.PopBool(&value)) {
        values.push_back(value);
      }
      return ui::ozone::mojom::GesturePropValue::NewBools(values);
    }
    case dbus::Message::DataType::DOUBLE: {
      std::vector<double> values = {};
      double value;
      while (array_reader.PopDouble(&value)) {
        values.push_back(value);
      }
      return ui::ozone::mojom::GesturePropValue::NewReals(values);
    }
    case dbus::Message::DataType::STRING: {
      *error_message =
          "String properties can only have one value, and so "
          "should not be specified as arrays.";
      return nullptr;
    }
    default: {
      *error_message =
          "Unsupported D-Bus value type; supported types are "
          "int32, int16, bool, double, and string.";
      return nullptr;
    }
  }
}

void GesturePropertiesServiceProvider::SetProperty(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  int32_t device_id;
  std::string property_name;

  if (!reader.PopInt32(&device_id) || !reader.PopString(&property_name)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS,
            "The device ID (int32) and/or property name (string) is missing."));
    return;
  }

  if (!reader.HasMoreData()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "No value(s) specified."));
    return;
  }

  std::string error_message;
  ui::ozone::mojom::GesturePropValuePtr values =
      GesturePropValueFromVariant(&reader, &error_message);

  if (values.is_null()) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  GetService()->SetProperty(device_id, property_name, std::move(values),
                            base::BindOnce(&SetPropertyCallback, method_call,
                                           std::move(response_sender)));
}

ui::ozone::mojom::GesturePropertiesService*
GesturePropertiesServiceProvider::GetService() {
  if (service_for_test_ != nullptr)
    return service_for_test_;

  if (!service_.is_bound()) {
    ui::OzonePlatform::GetInstance()
        ->GetInputController()
        ->GetGesturePropertiesService(service_.BindNewPipeAndPassReceiver());
  }
  return service_.get();
}

}  // namespace ash
