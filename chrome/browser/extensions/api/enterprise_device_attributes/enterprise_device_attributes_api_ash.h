// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_ASH_H_

#include <memory>

#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction();
  explicit EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction(
      std::unique_ptr<policy::DeviceAttributes> attributes);

 protected:
  ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() override;

  ResponseAction Run() override;

 private:
  std::unique_ptr<policy::DeviceAttributes> attributes_;

  DECLARE_EXTENSION_FUNCTION("enterprise.deviceAttributes.getDirectoryDeviceId",
                             ENTERPRISE_DEVICEATTRIBUTES_GETDIRECTORYDEVICEID)
};

class EnterpriseDeviceAttributesGetDeviceSerialNumberFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDeviceSerialNumberFunction();

 protected:
  ~EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() override;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "enterprise.deviceAttributes.getDeviceSerialNumber",
      ENTERPRISE_DEVICEATTRIBUTES_GETDEVICESERIALNUMBER)
};

class EnterpriseDeviceAttributesGetDeviceAssetIdFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDeviceAssetIdFunction();
  explicit EnterpriseDeviceAttributesGetDeviceAssetIdFunction(
      std::unique_ptr<policy::DeviceAttributes> attributes);

 protected:
  ~EnterpriseDeviceAttributesGetDeviceAssetIdFunction() override;

  ResponseAction Run() override;

 private:
  std::unique_ptr<policy::DeviceAttributes> attributes_;

  DECLARE_EXTENSION_FUNCTION("enterprise.deviceAttributes.getDeviceAssetId",
                             ENTERPRISE_DEVICEATTRIBUTES_GETDEVICEASSETID)
};

class EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction();
  explicit EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction(
      std::unique_ptr<policy::DeviceAttributes> attributes);

 protected:
  ~EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() override;

  ResponseAction Run() override;

 private:
  std::unique_ptr<policy::DeviceAttributes> attributes_;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.deviceAttributes.getDeviceAnnotatedLocation",
      ENTERPRISE_DEVICEATTRIBUTES_GETDEVICEANNOTATEDLOCATION)
};

class EnterpriseDeviceAttributesGetDeviceHostnameFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDeviceHostnameFunction();
  explicit EnterpriseDeviceAttributesGetDeviceHostnameFunction(
      std::unique_ptr<policy::DeviceAttributes> attributes);

 protected:
  ~EnterpriseDeviceAttributesGetDeviceHostnameFunction() override;

  ResponseAction Run() override;

 private:
  std::unique_ptr<policy::DeviceAttributes> attributes_;

  DECLARE_EXTENSION_FUNCTION("enterprise.deviceAttributes.getDeviceHostname",
                             ENTERPRISE_DEVICEATTRIBUTES_GETDEVICEHOSTNAME)
};

}  //  namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_ASH_H_
