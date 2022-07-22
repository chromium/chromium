// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_parser.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/system_display/system_display_serialization.h"
#include "chromeos/crosapi/mojom/system_display.mojom.h"
#include "extensions/common/api/system_display.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Converts |obj| to a JSON string, assuming T::ToValue() is defined.
template <class T>
std::string ObjectToString(const T& obj) {
  std::unique_ptr<base::Value> value = obj.ToValue();
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  EXPECT_TRUE(serializer.Serialize(*value));
  return json_string;
}

// Converts JSON string to |obj|, assuming static T::FromValue() is defined.
template <class T>
std::unique_ptr<T> StringToObject(const std::string& json_string) {
  JSONStringValueDeserializer deserializer(json_string);
  int err_code = 0;
  std::string err_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&err_code, &err_message);
  EXPECT_EQ(base::ValueDeserializer::kErrorCodeNoError, err_code)
      << "Unexpected error: " << err_message;
  EXPECT_NE(nullptr, value.get());
  std::unique_ptr<T> obj = T::FromValue(*value);
  EXPECT_NE(nullptr, obj.get());
  return obj;
}

// Returns a valid JSON string by concatenating a list of strings while making
// the following transforms:
// * Remove all prettifying double spaces.
// * Replace all prettifying "'" with "\"",
// * If |include_optional == false| the ignore all lines starting with "??".
//   Otherwise just strip "??".
std::string MakeJsonString(const std::vector<std::string>& lines,
                           bool include_optional) {
  std::vector<std::string> used_lines;
  for (const std::string& line : lines) {
    if (include_optional || line.substr(0, 2) != "??")
      used_lines.push_back(line);
  }
  std::string ret = base::StrCat(used_lines);
  base::ReplaceSubstringsAfterOffset(&ret, 0, "??", "");
  base::ReplaceSubstringsAfterOffset(&ret, 0, "'", "\"");
  base::ReplaceSubstringsAfterOffset(&ret, 0, "  ", "");
  return ret;
}

}  // namespace

namespace extensions {
namespace api {
namespace system_display {

// This test fails if extensions::api::system_display::DisplayMode changes.
// To fix, update |original_json_string|, and fix SerializeDisplayMode() and
// DeserializeDisplayMode().
TEST(SystemDisplaySerialization, DisplayMode) {
  std::vector<std::string> test_lines = {
      "{",
      "  'deviceScaleFactor':3.14159,",
      "  'height':487,",
      "  'heightInNativePixels':512,",
      "??'isInterlaced':true,",
      "  'isNative':true,",
      "  'isSelected':true,",
      "  'refreshRate':59.5,",
      "  'width':640,",
      "  'widthInNativePixels':671",
      "}",
  };

  for (int iter = 0; iter < 2; ++iter) {
    std::string original_json_string =
        MakeJsonString(test_lines, /*include_optional=*/iter > 0);
    auto original_obj = StringToObject<DisplayMode>(original_json_string);
    crosapi::mojom::SysDisplayModePtr serialized_obj =
        SerializeDisplayMode(*original_obj);
    DisplayMode copied_obj;
    DeserializeDisplayMode(*serialized_obj, &copied_obj);
    std::string copied_json_string = ObjectToString(copied_obj);

    EXPECT_EQ(original_json_string, copied_json_string);
  }
}

// This test fails if extensions::api::system_display::Edid changes. To fix,
// update |original_json_string|, and fix SerializeEdid() and DeserializeEdid().
TEST(SystemDisplaySerialization, Edid) {
  std::vector<std::string> test_lines = {
      "{",
      "  'manufacturerId':'ACME Display 3000',",
      "  'productId':'ABC-123-XYZ',",
      "  'yearOfManufacture':2038",
      "}",
  };

  std::string original_json_string = MakeJsonString(test_lines, true);
  auto original_obj = StringToObject<Edid>(original_json_string);
  crosapi::mojom::SysDisplayEdidPtr serialized_obj =
      SerializeEdid(*original_obj);
  Edid copied_obj;
  DeserializeEdid(*serialized_obj, &copied_obj);
  std::string copied_json_string = ObjectToString(copied_obj);

  EXPECT_EQ(original_json_string, copied_json_string);
}

// This test fails if extensions::api::system_display::DisplayUnitInfo changes.
// To fix, update |original_json_string|, and fix  SerializeDisplayUnitInfo()
// and DeserializeDisplayUnitInfo().
TEST(SystemDisplaySerialization, DisplayUnitInfo) {
  // Prettified lines used to create test cases. This was created by first
  // taking the output form running the following:
  //
  //   LOG(ERROR) << ObjectToString(DisplayUnitInfo());
  //
  // followed by populating lists and adding optional fields, obtained by
  // inspecting system_display.idl.
  std::vector<std::string> test_lines = {
      "{",
      "  'availableDisplayZoomFactors':[",
      "??  0.8,",
      "??  1.2,",
      "??  1.0",
      "  ],",
      "  'bounds':{",
      "    'height':749,",
      "    'left':7,",
      "    'top':11,",
      "    'width':1024",
      "  },",
      "  'displayZoomFactor':1.0,",
      "  'dpiX':90.0,",
      "  'dpiY':88.25,",
      "??'edid':{",
      "??  'manufacturerId':'ACME Display 3000',",
      "??  'productId':'ABC-123-XYZ',",
      "??  'yearOfManufacture':2038",
      "??},",
      "  'hasAccelerometerSupport':true,",
      "  'hasTouchSupport':true,",
      "  'id':'AWESOME-display-ID',",
      "??'isAutoRotationAllowed':true,",
      "  'isEnabled':true,",
      "  'isInternal':true,",
      "  'isPrimary':true,",
      "  'isUnified':true,",
      "  'mirroringDestinationIds':[",
      "    'DEST-ID-2',",
      "    'DEST-ID-1',",
      "    'DEST-ID-3'",
      "  ],",
      "  'mirroringSourceId':'SOURCE-ID-0',",
      "  'modes':[",
      "??  {",
      "??    'deviceScaleFactor':3.14159,",
      "??    'height':487,",
      "??    'heightInNativePixels':512,",
      "??    'isNative':true,",
      "??    'isSelected':true,",
      "??    'refreshRate':59.5,",
      "??    'width':640,",
      "??    'widthInNativePixels':671",
      "??  },",
      "??  {",
      "??    'deviceScaleFactor':2.71828,",
      "??    'height':30,",
      "??    'heightInNativePixels':30,",
      "??    'isInterlaced':true,",
      "??    'isNative':false,",
      "??    'isSelected':false,",
      "??    'refreshRate':122.0,",
      "??    'width':50,",
      "??    'widthInNativePixels':50",
      "??  }",
      "  ],",
      "  'name':'Display--123456789',",
      "  'overscan':{",
      "    'bottom':6,",
      "    'left':7,",
      "    'right':8,",
      "    'top':9",
      "  },",
      "  'rotation':90,",
      "  'workArea':{",
      "    'height':300,",
      "    'left':11,",
      "    'top':23,",
      "    'width':400",
      "  }",
      "}",
  };

  for (int iter = 0; iter < 2; ++iter) {
    std::string original_json_string =
        MakeJsonString(test_lines, /*include_optional=*/iter > 0);
    auto original_obj = StringToObject<DisplayUnitInfo>(original_json_string);
    crosapi::mojom::SysDisplayUnitInfoPtr serialized_obj =
        SerializeDisplayUnitInfo(*original_obj);
    DisplayUnitInfo copied_obj;
    DeserializeDisplayUnitInfo(*serialized_obj, &copied_obj);
    std::string copied_json_string = ObjectToString(copied_obj);

    EXPECT_EQ(original_json_string, copied_json_string);
  }
}

}  // namespace system_display
}  // namespace api
}  // namespace extensions
