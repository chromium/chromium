// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_

#include <map>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "base/values.h"
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
  using GetCapabilityCallback = base::OnceCallback<void(::base::Value::Dict)>;
  using StartPrintCallback = base::OnceCallback<void(mojom::StartPrintStatus)>;
  using GetPrinterInfoCallback = base::OnceCallback<void(::base::Value::Dict)>;

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
  void Reset();
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback);
  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  StartPrintCallback callback);
  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback);

  // Returns true if a pending get printer request is found.
  bool HasAnyPendingGetPrintersRequests() const;
  // Returns true if a pending get printer request is found with |request_id|.
  bool HasPendingGetPrintersRequestForTesting(
      base::UnguessableToken& request_id) const;
  bool HasProviderForTesting() const;

 private:
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
  // There may be more than one printer extensions installed. Each one will
  // report printers separately. Cache their printer counts and record the total
  // when all extensions have reported.
  std::map<base::UnguessableToken, size_t> total_printers_so_far_;

  base::WeakPtrFactory<ExtensionPrinterServiceAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EXTENSION_PRINTER_SERVICE_ASH_H_
