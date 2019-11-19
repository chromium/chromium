// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_local_printer_lister.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace cloud_print {

struct PrivetLocalPrinterLister::DeviceContext {
  std::unique_ptr<PrivetHTTPResolution> privet_resolution;
  std::unique_ptr<PrivetHTTPClient> privet_client;
  std::unique_ptr<PrivetJSONOperation> info_operation;
  DeviceDescription description;

  bool has_local_printing = false;
};

PrivetLocalPrinterLister::PrivetLocalPrinterLister(
    local_discovery::ServiceDiscoveryClient* service_discovery_client,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Delegate* delegate)
    : privet_http_factory_(
          PrivetHTTPAsynchronousFactory::CreateInstance(url_loader_factory)),
      delegate_(delegate),
      privet_lister_(
          new PrivetDeviceListerImpl(service_discovery_client, this)) {}

PrivetLocalPrinterLister::~PrivetLocalPrinterLister() {
}

void PrivetLocalPrinterLister::Start() {
  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices();
}

void PrivetLocalPrinterLister::Stop() {
  privet_lister_.reset();
}

void PrivetLocalPrinterLister::DeviceChanged(
    const std::string& name,
    const DeviceDescription& description) {
  if (description.type != kPrivetTypePrinter)
    return;

  auto it = device_contexts_.find(name);
  if (it != device_contexts_.end()) {
    it->second->description = description;
    delegate_->LocalPrinterChanged(name, it->second->has_local_printing,
                                   description);
    return;
  }

  auto context = std::make_unique<DeviceContext>();
  context->has_local_printing = false;
  context->description = description;
  context->privet_resolution = privet_http_factory_->CreatePrivetHTTP(name);

  DeviceContext* context_ptr = context.get();
  device_contexts_[name] = std::move(context);
  context_ptr->privet_resolution->Start(
      description.address,
      base::BindOnce(&PrivetLocalPrinterLister::OnPrivetResolved,
                     base::Unretained(this), name));
}

void PrivetLocalPrinterLister::DeviceCacheFlushed() {
  device_contexts_.clear();
  delegate_->LocalPrinterCacheFlushed();
}

void PrivetLocalPrinterLister::OnPrivetResolved(
    const std::string& name,
    std::unique_ptr<PrivetHTTPClient> http_client) {
  if (!http_client) {
    // Remove device if we can't resolve it.
    device_contexts_.erase(name);
    return;
  }
  auto it = device_contexts_.find(http_client->GetName());
  DCHECK(it != device_contexts_.end());

  it->second->info_operation = http_client->CreateInfoOperation(base::BindOnce(
      &PrivetLocalPrinterLister::OnPrivetInfoDone, base::Unretained(this),
      it->second.get(), http_client->GetName()));
  it->second->privet_client = std::move(http_client);
  it->second->info_operation->Start();
}

void PrivetLocalPrinterLister::OnPrivetInfoDone(
    DeviceContext* context,
    const std::string& name,
    const base::DictionaryValue* json_value) {
  bool has_local_printing = false;
  const base::ListValue* api_list = nullptr;
  if (json_value && json_value->GetList(kPrivetInfoKeyAPIList, &api_list)) {
    for (size_t i = 0; i < api_list->GetSize(); ++i) {
      std::string api;
      api_list->GetString(i, &api);
      if (api == kPrivetSubmitdocPath) {
        has_local_printing = true;
        break;
      }
    }
  }

  context->has_local_printing = has_local_printing;
  delegate_->LocalPrinterChanged(name, has_local_printing,
                                 context->description);
}

void PrivetLocalPrinterLister::DeviceRemoved(const std::string& device_name) {
  size_t removed = device_contexts_.erase(device_name);
  if (removed)
    delegate_->LocalPrinterRemoved(device_name);
}

const DeviceDescription* PrivetLocalPrinterLister::GetDeviceDescription(
    const std::string& name) {
  auto it = device_contexts_.find(name);
  return (it != device_contexts_.end()) ? &it->second->description : nullptr;
}

}  // namespace cloud_print
