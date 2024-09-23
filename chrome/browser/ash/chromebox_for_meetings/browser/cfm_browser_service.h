// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_browser.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// Implementation of the CfmBrowser Service.
// The lifespan of this service is indicative of the lifespan of chrome browser
// process, allowing |CfmServiceContext| to rebuild its IPC graph if required.
class CfmBrowserService : public CfmObserver,
                          public chromeos::cfm::ServiceAdaptor::Delegate,
                          public chromeos::cfm::mojom::CfmBrowser {
 public:
  CfmBrowserService(const CfmBrowserService&) = delete;
  CfmBrowserService& operator=(const CfmBrowserService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static CfmBrowserService* Get();
  static bool IsInitialized();

 protected:
  // CfmObserver:
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate:
  void OnAdaptorDisconnect() override;
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

  // mojom::CfmBrowser:
  void GetVariationsData(GetVariationsDataCallback callback) override;
  void GetMemoryDetails(GetMemoryDetailsCallback callback) override;

  // Disconnect handler for |mojom::CfmBrowser|
  virtual void OnMojoDisconnect();

 private:
  CfmBrowserService();
  ~CfmBrowserService() override;

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::CfmBrowser> receivers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CfmBrowserService> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_BROWSER_CFM_BROWSER_SERVICE_H_
