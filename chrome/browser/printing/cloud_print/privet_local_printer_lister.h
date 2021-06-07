// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister.h"
#include "chrome/browser/printing/cloud_print/privet_http.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

// This is an adapter to PrivetDeviceLister that finds printers and checks if
// they support Privet local printing.
class PrivetLocalPrinterLister : PrivetDeviceLister::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void LocalPrinterChanged(const std::string& name,
                                     bool has_local_printing,
                                     const DeviceDescription& description) = 0;
    virtual void LocalPrinterRemoved(const std::string& name) = 0;
    virtual void LocalPrinterCacheFlushed() = 0;
  };

  PrivetLocalPrinterLister(
      local_discovery::ServiceDiscoveryClient* service_discovery_client,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Delegate* delegate);
  ~PrivetLocalPrinterLister() override;

  void Start();

  // Stops listening/listing, keeps the data.
  void Stop();

  const DeviceDescription* GetDeviceDescription(const std::string& name);

  // PrivetDeviceLister::Delegate implementation.
  void DeviceChanged(const std::string& name,
                     const DeviceDescription& description) override;
  void DeviceRemoved(const std::string& name) override;
  void DeviceCacheFlushed() override;

 private:
  struct DeviceContext;

  using DeviceContextMap =
      std::map<std::string, std::unique_ptr<DeviceContext>>;

  void OnPrivetInfoDone(DeviceContext* context,
                        const std::string& name,
                        const base::DictionaryValue* json_value);

  void OnPrivetResolved(const std::string& name,
                        std::unique_ptr<PrivetHTTPClient> http_client);

  std::unique_ptr<PrivetHTTPAsynchronousFactory> privet_http_factory_;
  DeviceContextMap device_contexts_;
  Delegate* const delegate_;

  std::unique_ptr<PrivetDeviceLister> privet_lister_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_LOCAL_PRINTER_LISTER_H_
