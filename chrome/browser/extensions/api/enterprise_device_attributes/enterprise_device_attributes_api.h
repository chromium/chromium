// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction();

 protected:
  ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() override;

  ResponseAction Run() override;

 private:
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

 protected:
  ~EnterpriseDeviceAttributesGetDeviceAssetIdFunction() override;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("enterprise.deviceAttributes.getDeviceAssetId",
                             ENTERPRISE_DEVICEATTRIBUTES_GETDEVICEASSETID)
};

class EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction
    : public ExtensionFunction {
 public:
  EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction();

 protected:
  ~EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() override;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "enterprise.deviceAttributes.getDeviceAnnotatedLocation",
      ENTERPRISE_DEVICEATTRIBUTES_GETDEVICEANNOTATEDLOCATION)
};

}  //  namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_DEVICE_ATTRIBUTES_ENTERPRISE_DEVICE_ATTRIBUTES_API_H_
