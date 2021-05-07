// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_printer_list.h"

#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"

namespace cloud_print {

CloudPrintPrinterList::Delegate::~Delegate() {}

CloudPrintPrinterList::CloudPrintPrinterList(Delegate* delegate)
    : delegate_(delegate) {}

CloudPrintPrinterList::~CloudPrintPrinterList() {
}

void CloudPrintPrinterList::OnGCDApiFlowError(GCDApiFlow::Status status) {
  delegate_->OnDeviceListUnavailable();
}

void CloudPrintPrinterList::OnGCDApiFlowComplete(
    const base::DictionaryValue& value) {
  const base::ListValue* printers;
  if (!value.GetList(cloud_print::kPrinterListValue, &printers)) {
    delegate_->OnDeviceListUnavailable();
    return;
  }

  DeviceList devices;
  for (const auto& printer : printers->GetList()) {
    const base::DictionaryValue* printer_dict;
    if (!printer.GetAsDictionary(&printer_dict))
      continue;

    Device printer_details;
    if (!FillPrinterDetails(*printer_dict, &printer_details))
      continue;

    devices.push_back(printer_details);
  }

  delegate_->OnDeviceListReady(devices);
}

GURL CloudPrintPrinterList::GetURL() {
  return cloud_devices::GetCloudPrintRelativeURL("search");
}

GCDApiFlow::Request::NetworkTrafficAnnotation
CloudPrintPrinterList::GetNetworkTrafficAnnotationType() {
  return TYPE_SEARCH;
}

bool CloudPrintPrinterList::FillPrinterDetails(
    const base::DictionaryValue& printer_value,
    Device* printer_details) {
  if (!printer_value.GetString(cloud_print::kIdValue, &printer_details->id))
    return false;

  if (!printer_value.GetString(cloud_print::kDisplayNameValue,
                               &printer_details->display_name)) {
    return false;
  }

  // Non-essential.
  printer_value.GetString(cloud_print::kPrinterDescValue,
                          &printer_details->description);

  return true;
}

}  // namespace cloud_print
