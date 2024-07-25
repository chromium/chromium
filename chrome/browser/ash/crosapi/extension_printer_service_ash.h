// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_

// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Bridge between ash-chrome's ExtensionPrinterHandlerAdapterAsh and
// lacros-chrome's ExtensionPrinterServiceProvider.
class ExtensionPrinterServiceAsh : public mojom::ExtensionPrinterService {
 public:
  using AddedPrintersCallback =
      base::RepeatingCallback<void(base::Value::List printers)>;
  using GetPrintersDoneCallback = base::OnceClosure;

  ExtensionPrinterServiceAsh();
  ExtensionPrinterServiceAsh(const ExtensionPrinterServiceAsh&) = delete;
  ExtensionPrinterServiceAsh& operator=(const ExtensionPrinterServiceAsh&) =
      delete;
  ~ExtensionPrinterServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::ExtensionPrinterService> pending_receiver);

  // mojom::ExtensionPrinterService:
  void RegisterServiceProvider(
      mojo::PendingRemote<mojom::ExtensionPrinterServiceProvider> provider)
      override;
  void PrintersAdded(const base::UnguessableToken& request_id,
                     base::Value::List printers,
                     bool is_done) override;

  // Called when an ExtensionPrinterServiceProvider is disconnected.
  void ExtensionPrinterServiceProviderDisconnected();

  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback);

 private:
  friend class ExtensionPrinterServiceAshBrowserTest;

  // Returns true iff there is any registered ExtensionPrinterServiceProvider.
  bool HasProvider() const;
  void ClearPendingRequests();

  // Supports any number of connections.
  mojo::ReceiverSet<mojom::ExtensionPrinterService> receivers_;

  // The ExtensionPrinterServiceProvider from Lacros. The
  // ExtensionPrinterServiceProvider only supports Lacros primary profile for
  // extension printer service.
  mojo::Remote<mojom::ExtensionPrinterServiceProvider> service_provider_;
  // Keeps a mapping between request_id and the corresponding
  // AddedPrintersCallback.
  std::map<base::UnguessableToken, AddedPrintersCallback>
      pending_printers_added_callbacks_;
  // Keeps a mapping between request_id and the corresponding
  // GetPrintersDoneCallback.
  std::map<base::UnguessableToken, GetPrintersDoneCallback>
      pending_get_printers_done_callbacks_;

  base::WeakPtrFactory<ExtensionPrinterServiceAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_
