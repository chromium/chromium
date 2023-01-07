// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for web page info. Lives in Ash-Chrome on
// the UI thread.
class WebPageInfoFactoryAsh : public mojom::WebPageInfoFactory {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLacrosInstanceRegistered(
        const mojo::RemoteSetElementId& remote_id) = 0;
    virtual void OnLacrosInstanceDisconnected(
        const mojo::RemoteSetElementId& remote_id) = 0;
  };

  WebPageInfoFactoryAsh();
  WebPageInfoFactoryAsh(const WebPageInfoFactoryAsh&) = delete;
  WebPageInfoFactoryAsh& operator=(const WebPageInfoFactoryAsh&) = delete;
  ~WebPageInfoFactoryAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver);

  // crosapi::mojom::WebPageInfoFactory:
  void RegisterWebPageInfoProvider(
      mojo::PendingRemote<mojom::WebPageInfoProvider> web_page_info_provider)
      override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called by smart dim to request web page info from lacros. When receiving a
  // WebPageInfoPtr, runs the callback.
  using RequestCurrentWebPageInfoCallback =
      base::OnceCallback<void(mojom::WebPageInfoPtr)>;
  void RequestCurrentWebPageInfo(const mojo::RemoteSetElementId& remote_id,
                                 RequestCurrentWebPageInfoCallback callback);

 private:
  void OnDisconnected(mojo::RemoteSetElementId mojo_id);

  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::WebPageInfoFactory> receivers_;

  // This set maintains all registered web page info providers.
  mojo::RemoteSet<mojom::WebPageInfoProvider> web_page_info_providers_;

  // The customers of lacros web page info. When `RegisterWebPageInfoProvider`
  // is called, notify observers with the RemoteSetElementId.
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<WebPageInfoFactoryAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_
