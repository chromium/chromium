// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "Out-of-process printing must be enabled."
#endif

namespace crash_keys {
class ScopedPrinterInfo;
}

namespace printing {

class PrintBackendServiceManager {
 public:
  PrintBackendServiceManager(const PrintBackendServiceManager&) = delete;
  PrintBackendServiceManager& operator=(const PrintBackendServiceManager&) =
      delete;

  // Register as a client of PrintBackendServiceManager.  This acts as a signal
  // of impending activity enabling possible optimizations within the manager.
  uint32_t RegisterClient();

  // Notify the manager that this client is no longer needing print backend
  // services.  This signal might alter the manager's internal optimizations.
  void UnregisterClient(uint32_t id);

  // Wrappers around mojom::PrintBackendService call.
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback);
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback);
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback);
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback);
  void UpdatePrintSettings(
      const std::string& printer_name,
      base::flat_map<std::string, base::Value> job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback);
  void StartPrinting(
      const std::string& printer_name,
      int document_cookie,
      const std::u16string& document_name,
      mojom::PrintTargetType target_type,
      int page_count,
      const PrintSettings& settings,
      mojom::PrintBackendService::StartPrintingCallback callback);

  // Query if printer driver has been found to require elevated privilege in
  // order to have print queries/commands succeed.
  bool PrinterDriverRequiresElevatedPrivilege(
      const std::string& printer_name) const;

  // Make note that `printer_name` has been detected as requiring elevated
  // privileges in order to operate.
  void SetPrinterDriverRequiresElevatedPrivilege(
      const std::string& printer_name);

  // Overrides the print backend service for testing.  Caller retains ownership
  // of `remote`.
  void SetServiceForTesting(
      mojo::Remote<printing::mojom::PrintBackendService>* remote);

  // Overrides the print backend service for testing when an alternate service
  // is required for fallback processing after an access denied error.  Caller
  // retains ownership of `remote`.
  void SetServiceForFallbackTesting(
      mojo::Remote<printing::mojom::PrintBackendService>* remote);

  // There is to be at most one instance of this at a time.
  static PrintBackendServiceManager& GetInstance();

  // Test support to revert to a fresh instance.
  static void ResetForTesting();

 private:
  friend base::NoDestructor<PrintBackendServiceManager>;

  // Types to track saved callbacks associated with currently executing mojom
  // service calls.  These will be run either after a Mojom call finishes
  // executing or if the service should disconnect before the mojom service
  // calls complete.
  // These need to be able to be found as a group for a particular remote that
  // might become disconnected, and so a map-per-remote is used as a container.
  // Use of a map allows for an ID key to be used to easily find any individual
  // callback that can be discarded once a service call succeeds normally.

  // Key is a callback ID.
  template <class T>
  using SavedCallbacks =
      base::flat_map<base::UnguessableToken, base::OnceCallback<void(T)>>;

  // Key is the remote ID that enables finding the correct remote.  Note that
  // the remote ID does not necessarily mean the printer name.
  template <class T>
  using RemoteSavedCallbacks = base::flat_map<std::string, SavedCallbacks<T>>;
  template <class T>
  using RemoteSavedStructCallbacks = RemoteSavedCallbacks<mojo::StructPtr<T>>;

  using RemoteSavedEnumeratePrintersCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterListResult>;
  using RemoteSavedFetchCapabilitiesCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterCapsAndInfoResult>;
  using RemoteSavedGetDefaultPrinterNameCallbacks =
      RemoteSavedStructCallbacks<mojom::DefaultPrinterNameResult>;
  using RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterSemanticCapsAndDefaultsResult>;
  using RemoteSavedUpdatePrintSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
  using RemoteSavedStartPrintingCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode>;

  PrintBackendServiceManager();
  ~PrintBackendServiceManager();

  // Determine the remote ID that is used for the specified `printer_name`.
  std::string GetRemoteIdForPrinterName(const std::string& printer_name) const;

  // Acquires a remote handle to the Print Backend Service instance, launching a
  // process to host the service if necessary. |is_sandboxed| is set if the
  // service was launched within a sandbox.
  const mojo::Remote<printing::mojom::PrintBackendService>& GetService(
      const std::string& printer_name,
      bool* is_sandboxed);

  // Help function to reset idle timeout duration to a short value.
  void UpdateServiceToShortIdleTimeout(
      mojo::Remote<printing::mojom::PrintBackendService>& service,
      bool sandboxed,
      const std::string& remote_id);

  // Callback when predetermined idle timeout occurs indicating no in-flight
  // messages for a short period of time.  `sandboxed` is used to distinguish
  // which mapping of remotes the timeout applies to.
  void OnIdleTimeout(bool sandboxed, const std::string& remote_id);

  // Callback when service has disconnected (e.g., process crashes).
  // `sandboxed` is used to distinguish which mapping of remotes the
  // disconnection applies to.
  void OnRemoteDisconnected(bool sandboxed, const std::string& remote_id);

  // Helper function to choose correct saved callbacks mapping.
  RemoteSavedEnumeratePrintersCallbacks&
  GetRemoteSavedEnumeratePrintersCallbacks(bool sandboxed);
  RemoteSavedFetchCapabilitiesCallbacks&
  GetRemoteSavedFetchCapabilitiesCallbacks(bool sandboxed);
  RemoteSavedGetDefaultPrinterNameCallbacks&
  GetRemoteSavedGetDefaultPrinterNameCallbacks(bool sandboxed);
  RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks&
  GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(bool sandboxed);
  RemoteSavedUpdatePrintSettingsCallbacks&
  GetRemoteSavedUpdatePrintSettingsCallbacks(bool sandboxed);
  RemoteSavedStartPrintingCallbacks& GetRemoteSavedStartPrintingCallbacks(
      bool sandboxed);

  // Helper function to save outstanding callbacks.
  template <class T, class X>
  void SaveCallback(RemoteSavedCallbacks<T>& saved_callbacks,
                    const std::string& remote_id,
                    const base::UnguessableToken& saved_callback_id,
                    base::OnceCallback<void(X)> callback);

  // Helper function for local callback wrappers for mojom calls.
  template <class T, class X>
  void ServiceCallbackDone(RemoteSavedCallbacks<T>& saved_callbacks,
                           const std::string& remote_id,
                           const base::UnguessableToken& saved_callback_id,
                           X data);

  // Local callback wrappers for mojom calls.
  void EnumeratePrintersDone(bool sandboxed,
                             const std::string& remote_id,
                             const base::UnguessableToken& saved_callback_id,
                             mojom::PrinterListResultPtr printer_list);
  void FetchCapabilitiesDone(
      bool sandboxed,
      const std::string& remote_id,
      const base::UnguessableToken& saved_callback_id,
      mojom::PrinterCapsAndInfoResultPtr printer_caps_and_info);
  void GetDefaultPrinterNameDone(
      bool sandboxed,
      const std::string& remote_id,
      const base::UnguessableToken& saved_callback_id,
      mojom::DefaultPrinterNameResultPtr printer_name);
  void GetPrinterSemanticCapsAndDefaultsDone(
      bool sandboxed,
      const std::string& remote_id,
      const base::UnguessableToken& saved_callback_id,
      mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps);
  void UpdatePrintSettingsDone(bool sandboxed,
                               const std::string& remote_id,
                               const base::UnguessableToken& saved_callback_id,
                               mojom::PrintSettingsResultPtr printer_caps);
  void StartPrintingDone(bool sandboxed,
                         const std::string& remote_id,
                         const base::UnguessableToken& saved_callback_id,
                         mojom::ResultCode result);

  // Helper functions to run outstanding callbacks when a remote has become
  // disconnected.
  template <class T>
  void RunSavedCallbacksStructResult(
      RemoteSavedStructCallbacks<T>& saved_callbacks,
      const std::string& remote_id,
      mojo::StructPtr<T> result_to_clone);
  template <class T>
  void RunSavedCallbacksResult(RemoteSavedCallbacks<T>& saved_callbacks,
                               const std::string& remote_id,
                               T result);

  using RemotesMap =
      base::flat_map<std::string,
                     mojo::Remote<printing::mojom::PrintBackendService>>;

  // Keep separate mapping of remotes for sandboxed vs. unsandboxed services.
  RemotesMap sandboxed_remotes_;
  RemotesMap unsandboxed_remotes_;

  // Keeps remotes for wrapping service hosts alive until they disconnect.
  mojo::RemoteSet<printing::mojom::SandboxedPrintBackendHost> sandboxed_hosts_;
  mojo::RemoteSet<printing::mojom::UnsandboxedPrintBackendHost>
      unsandboxed_hosts_;

  // Set of IDs for clients actively engaged in printing.  This could include
  // tabs in print preview as well as an active system print.  Retention of a
  // service process can have benefit so long as there are active clients.
  base::flat_set<uint32_t> clients_;
  uint32_t last_client_id_ = 0;

  // Track the saved callbacks for each remote.
  RemoteSavedEnumeratePrintersCallbacks
      sandboxed_saved_enumerate_printers_callbacks_;
  RemoteSavedEnumeratePrintersCallbacks
      unsandboxed_saved_enumerate_printers_callbacks_;
  RemoteSavedFetchCapabilitiesCallbacks
      sandboxed_saved_fetch_capabilities_callbacks_;
  RemoteSavedFetchCapabilitiesCallbacks
      unsandboxed_saved_fetch_capabilities_callbacks_;
  RemoteSavedGetDefaultPrinterNameCallbacks
      sandboxed_saved_get_default_printer_name_callbacks_;
  RemoteSavedGetDefaultPrinterNameCallbacks
      unsandboxed_saved_get_default_printer_name_callbacks_;
  RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks
      sandboxed_saved_get_printer_semantic_caps_and_defaults_callbacks_;
  RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks
      unsandboxed_saved_get_printer_semantic_caps_and_defaults_callbacks_;
  RemoteSavedUpdatePrintSettingsCallbacks
      sandboxed_saved_update_print_settings_callbacks_;
  RemoteSavedUpdatePrintSettingsCallbacks
      unsandboxed_saved_update_print_settings_callbacks_;
  RemoteSavedStartPrintingCallbacks sandboxed_saved_start_printing_callbacks_;
  RemoteSavedStartPrintingCallbacks unsandboxed_saved_start_printing_callbacks_;

  // Set of printer drivers which require elevated permissions to operate.
  // It is expected that most print drivers will succeed with the preconfigured
  // sandbox permissions.  Should any drivers be discovered to require more than
  // that (and thus fail with access denied errors) then we need to fallback to
  // performing the operation with modified restrictions.
  base::flat_set<std::string> drivers_requiring_elevated_privilege_;

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated due to Mojo
  // message response validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  // Override of service to use for testing.
  mojo::Remote<printing::mojom::PrintBackendService>*
      sandboxed_service_remote_for_test_ = nullptr;
  mojo::Remote<printing::mojom::PrintBackendService>*
      unsandboxed_service_remote_for_test_ = nullptr;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
