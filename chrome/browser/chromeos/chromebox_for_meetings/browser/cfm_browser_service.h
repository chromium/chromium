// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_browser.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

// Implementation of the CfmBrowser Service.
// The lifespan of this service is indicative of the lifespan of chrome browser
// process, allowing |CfmServiceContext| to rebuild its IPC graph if required.
class CfmBrowserService : public CfmObserver,
                          public ServiceAdaptor::Delegate,
                          public mojom::CfmBrowser {
 public:
  CfmBrowserService(const CfmBrowserService&) = delete;
  CfmBrowserService& operator=(const CfmBrowserService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static CfmBrowserService* Get();
  static bool IsInitialized();

 protected:
  // Forward |CfmObserver| implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // Disconnect handler for |mojom::CfmServiceAdaptor|
  void OnAdaptorDisconnect() override;

  // Forward |ServiceAdaptorDelegate| implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

  // Disconnect handler for |mojom::CfmBrowser|
  virtual void OnMojoDisconnect();

 private:
  CfmBrowserService();
  ~CfmBrowserService() override;

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::CfmBrowser> receivers_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_
