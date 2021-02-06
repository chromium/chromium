// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PRINTER_LIST_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PRINTER_LIST_H_

#include <string>
#include <vector>

#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"

namespace cloud_print {

class CloudPrintPrinterList : public CloudPrintApiFlowRequest {
 public:
  struct Device {
    std::string id;
    std::string display_name;
    std::string description;
  };
  using DeviceList = std::vector<Device>;

  class Delegate {
   public:
    virtual ~Delegate();

    virtual void OnDeviceListReady(const DeviceList& devices) = 0;
    virtual void OnDeviceListUnavailable() = 0;
  };

  explicit CloudPrintPrinterList(Delegate* delegate);
  ~CloudPrintPrinterList() override;

  // CloudPrintApiFlowRequest implementation:
  void OnGCDApiFlowError(GCDApiFlow::Status status) override;
  void OnGCDApiFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;
  NetworkTrafficAnnotation GetNetworkTrafficAnnotationType() override;

 private:
  bool FillPrinterDetails(const base::DictionaryValue& printer_value,
                          Device* printer_details);

  Delegate* const delegate_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PRINTER_LIST_H_
