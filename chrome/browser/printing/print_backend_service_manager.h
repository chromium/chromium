// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/types/strong_alias.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
#include "ui/gfx/native_widget_types.h"
#endif

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "Out-of-process printing must be enabled."
#endif

namespace crash_keys {
class ScopedPrinterInfo;
}

namespace printing {

#if BUILDFLAG(IS_WIN)
class PrinterXmlParserImpl;
#endif  // BUILDFLAG(IS_WIN)

class PrintedPage;

class PrintBackendServiceManager {
 public:
  using RemoteId = base::StrongAlias<class RemoteIdTag, uint32_t>;
  using ClientId = base::StrongAlias<class ClientIdTag, uint32_t>;

  // Contains set of client IDs.
  using ClientsSet = base::flat_set<ClientId>;

  // Mapping of the RemoteId
  using QueryWithUiClientsMap = base::flat_map<ClientId, RemoteId>;

  // Mapping of clients to each remote ID that is for printing.
  using PrintClientsMap = base::flat_map<RemoteId, ClientsSet>;

  // Amount of idle time to wait before resetting the connection to the service.
  static constexpr base::TimeDelta kNoClientsRegisteredResetOnIdleTimeout =
      base::Seconds(10);
  static constexpr base::TimeDelta kClientsRegisteredResetOnIdleTimeout =
      base::Seconds(120);

  PrintBackendServiceManager(const PrintBackendServiceManager&) = delete;
  PrintBackendServiceManager& operator=(const PrintBackendServiceManager&) =
      delete;

  // Client registration routines.  These act as a signal of impending activity
  // enabling possible optimizations within the manager.  They return an ID
  // which the callers are to use with `UnregisterClient()` once they have
  // completed their printing activity.

  // Register as a client of PrintBackendServiceManager for print queries.
  ClientId RegisterQueryClient();

  // Register as a client of PrintBackendServiceManager for print queries which
  // require a system print dialog UI.  If a platform cannot support concurrent
  // queries of this type then this will return `absl::nullopt` if another
  // client is already registered.
  absl::optional<ClientId> RegisterQueryWithUiClient();

  // Register as a client of PrintBackendServiceManager for printing a document
  // to a specific printer.
  ClientId RegisterPrintDocumentClient(const std::string& printer_name);

  // Register as a client of PrintBackendServiceManager for printing a document
  // to a specific printer.  Use the same `RemoteId` for this new printing
  // client as has been used by the indicated query with UI client.  This method
  // will DCHECK if the client ID provided is not for a query with UI client.
  ClientId RegisterPrintDocumentClientReusingClientRemote(ClientId id);

  // Notify the manager that this client is no longer needing print backend
  // services.  This signal might alter the manager's internal optimizations.
  void UnregisterClient(ClientId id);

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
  void UseDefaultSettings(
      const std::string& printer_name,
      mojom::PrintBackendService::UseDefaultSettingsCallback callback);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void AskUserForSettings(
      const std::string& printer_name,
      gfx::NativeView parent_view,
      int max_pages,
      bool has_selection,
      bool is_scripted,
      mojom::PrintBackendService::AskUserForSettingsCallback callback);
#endif
  void UpdatePrintSettings(
      const std::string& printer_name,
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback);
  void StartPrinting(
      const std::string& printer_name,
      int document_cookie,
      const std::u16string& document_name,
      mojom::PrintTargetType target_type,
      const PrintSettings& settings,
      mojom::PrintBackendService::StartPrintingCallback callback);
#if BUILDFLAG(IS_WIN)
  void RenderPrintedPage(
      const std::string& printer_name,
      int document_cookie,
      const PrintedPage& page,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page_data,
      mojom::PrintBackendService::RenderPrintedPageCallback callback);
#endif
  void RenderPrintedDocument(
      const std::string& printer_name,
      int document_cookie,
      uint32_t page_count,
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_data,
      mojom::PrintBackendService::RenderPrintedDocumentCallback callback);
  void DocumentDone(const std::string& printer_name,
                    int document_cookie,
                    mojom::PrintBackendService::DocumentDoneCallback callback);
  void Cancel(const std::string& printer_name,
              int document_cookie,
              mojom::PrintBackendService::CancelCallback callback);

  // Query if printer driver has been found to require elevated privilege in
  // order to have print queries/commands succeed.
  bool PrinterDriverFoundToRequireElevatedPrivilege(
      const std::string& printer_name) const;

  // Make note that `printer_name` has been detected as requiring elevated
  // privileges in order to operate.
  void SetPrinterDriverFoundToRequireElevatedPrivilege(
      const std::string& printer_name);

  // Overrides the print backend service for testing.  Caller retains ownership
  // of `remote`.
  void SetServiceForTesting(mojo::Remote<mojom::PrintBackendService>* remote);

  // Overrides the print backend service for testing when an alternate service
  // is required for fallback processing after an access denied error.  Caller
  // retains ownership of `remote`.
  void SetServiceForFallbackTesting(
      mojo::Remote<mojom::PrintBackendService>* remote);

  // There is to be at most one instance of this at a time.
  static PrintBackendServiceManager& GetInstance();

  // Test support to revert to a fresh instance.
  static void ResetForTesting();

 private:
  friend base::NoDestructor<PrintBackendServiceManager>;
  friend class SystemAccessProcessPrintBrowserTestBase;
  FRIEND_TEST_ALL_PREFIXES(PrintBackendServiceManagerTest,
                           IsIdleTimeoutUpdateNeededForRegisteredClient);
  FRIEND_TEST_ALL_PREFIXES(PrintBackendServiceManagerTest,
                           IsIdleTimeoutUpdateNeededForUnregisteredClient);

  enum class ClientType {
    // Print Preview scenario, where printer might not be known.  Only performs
    // queries, none of which would invoke a system dialog.
    kQuery,
    // System print scenario, where printer is not known. Only performs queries,
    // and can require a window-modal system dialog be displayed to satisfy
    // those queries.
    kQueryWithUi,
    // Printer is known, and printing of a document will be performed.  System
    // dialogs might be required to complete printing (e.g., if driver saves to
    // a file).
    kPrintDocument,
  };

  // Types to track saved callbacks associated with currently executing mojom
  // service calls.  These will be run either after a Mojom call finishes
  // executing or if the service should disconnect before the mojom service
  // calls complete.
  // These need to be able to be found as a group for a particular remote that
  // might become disconnected, and so a map-per-remote is used as a container.
  // Use of a map allows for an ID key to be used to easily find any individual
  // callback that can be discarded once a service call succeeds normally.

  // Key is a callback ID.
  template <class... T>
  using SavedCallbacks =
      base::flat_map<base::UnguessableToken, base::OnceCallback<void(T...)>>;

  // Key is the remote ID that enables finding the correct remote.  Note that
  // the remote ID does not necessarily mean the printer name.
  template <class... T>
  using RemoteSavedCallbacks = base::flat_map<RemoteId, SavedCallbacks<T...>>;
  template <class... T>
  using RemoteSavedStructCallbacks =
      RemoteSavedCallbacks<mojo::StructPtr<T...>>;

  using RemoteSavedEnumeratePrintersCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterListResult>;
  using RemoteSavedFetchCapabilitiesCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterCapsAndInfoResult>;
  using RemoteSavedGetDefaultPrinterNameCallbacks =
      RemoteSavedStructCallbacks<mojom::DefaultPrinterNameResult>;
  using RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrinterSemanticCapsAndDefaultsResult>;
  using RemoteSavedUseDefaultSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  using RemoteSavedAskUserForSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
#endif
  using RemoteSavedUpdatePrintSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
  using RemoteSavedStartPrintingCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode>;
#if BUILDFLAG(IS_WIN)
  using RemoteSavedRenderPrintedPageCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode>;
#endif
  using RemoteSavedRenderPrintedDocumentCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode>;
  using RemoteSavedDocumentDoneCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode>;
  using RemoteSavedCancelCallbacks = RemoteSavedCallbacks<>;

  // Bundle of the `PrintBackendService` and its sandboxed/unsandboxed host
  // remotes.
  template <class T>
  struct RemotesBundle {
    RemotesBundle() = default;
    ~RemotesBundle() = default;
    mojo::Remote<mojom::PrintBackendService> service;
    mojo::Remote<T> host;
  };

  template <class T>
  using RemotesBundleMap =
      base::flat_map<RemoteId, std::unique_ptr<RemotesBundle<T>>>;

  // PrintBackendServiceManager needs to be able to run a callback either after
  // a successful return from the service or after the remote was disconnected.
  // This structure is used to save the callback's context.
  struct CallbackContext {
    CallbackContext();
    CallbackContext(CallbackContext&& other) noexcept;
    ~CallbackContext();

    bool is_sandboxed;
    RemoteId remote_id;
    base::UnguessableToken saved_callback_id;
  };

  PrintBackendServiceManager();
  ~PrintBackendServiceManager();

  static void LogCallToRemote(base::StringPiece name,
                              const CallbackContext& context);
  static void LogCallbackFromRemote(base::StringPiece name,
                                    const CallbackContext& context);

  void SetCrashKeys(const std::string& printer_name);

  // Determine the remote ID that is used for the specified `printer_name`.
  RemoteId GetRemoteIdForPrinterName(const std::string& printer_name);

  // Common helper for registering clients.  The `destination` parameter can be
  // either a `std::string` for a printer name or a `RemoteId` which was
  // generated from a prior registration.  This method will DCHECK if the
  // `destination` is a `RemoteId` and the registration requires launching
  // another service instance.
  absl::optional<ClientId> RegisterClient(
      ClientType client_type,
      absl::variant<std::string, RemoteId> destination);

  // Get the total number of clients registered.
  size_t GetClientsRegisteredCount() const;

#if BUILDFLAG(IS_WIN)
  // Query if printer driver has known reasons for requiring elevated
  // privileges in order to operate.  In these cases relying upon fallback
  // after an access-denied error is not preferable.  Any such reasons are
  // platform specific.
  bool PrinterDriverKnownToRequireElevatedPrivilege(
      const std::string& printer_name,
      ClientType client_type);
#endif

  // Acquires a remote handle to the Print Backend Service instance, launching a
  // process to host the service if necessary. `is_sandboxed` is set to indicate
  // if the service was launched within a sandbox.
  const mojo::Remote<mojom::PrintBackendService>& GetService(
      const std::string& printer_name,
      ClientType client_type,
      bool* is_sandboxed);

  // Helper to `GetService` for a particular remotes bundle type.
  template <class T>
  mojo::Remote<mojom::PrintBackendService>& GetServiceFromBundle(
      const RemoteId& remote_id,
      ClientType client_type,
      bool sandboxed,
      RemotesBundleMap<T>& bundle_map);

  // Get the idle timeout value to user for a particular client type.
  static constexpr base::TimeDelta GetClientTypeIdleTimeout(
      ClientType client_type);

  // Whether any clients are queries with UI to `remote_id`.
  bool HasQueryWithUiClientForRemoteId(const RemoteId& remote_id) const;

  // Whether any clients are printing documents to `remote_id`.
  bool HasPrintDocumentClientForRemoteId(const RemoteId& remote_id) const;

  // Get the number of clients printing documents to `remote_id`.
  size_t GetPrintDocumentClientsCountForRemoteId(
      const RemoteId& remote_id) const;

  // Determine if idle timeout should be modified based upon there having been
  // a new client registered for `registered_client_type`.
  absl::optional<base::TimeDelta> DetermineIdleTimeoutUpdateOnRegisteredClient(
      ClientType registered_client_type,
      const RemoteId& remote_id) const;

  // Determine if idle timeout should be modified after a client of type
  // `unregistered_client_type` has been unregistered.
  absl::optional<base::TimeDelta>
  DetermineIdleTimeoutUpdateOnUnregisteredClient(
      ClientType unregistered_client_type,
      const RemoteId& remote_id) const;

  // Helper functions to adjust service idle timeout duration.
  void SetServiceIdleHandler(
      mojo::Remote<printing::mojom::PrintBackendService>& service,
      bool sandboxed,
      const RemoteId& remote_id,
      const base::TimeDelta& timeout);
  void UpdateServiceIdleTimeoutByRemoteId(const RemoteId& remote_id,
                                          const base::TimeDelta& timeout);

  // Callback when predetermined idle timeout occurs indicating no in-flight
  // messages for a short period of time.  `sandboxed` is used to distinguish
  // which mapping of remotes the timeout applies to.
  void OnIdleTimeout(bool sandboxed, const RemoteId& remote_id);

  // Callback when service has disconnected (e.g., process crashes).
  // `sandboxed` is used to distinguish which mapping of remotes the
  // disconnection applies to.
  void OnRemoteDisconnected(bool sandboxed, const RemoteId& remote_id);

  // Helper function to choose correct saved callbacks mapping.
  RemoteSavedEnumeratePrintersCallbacks&
  GetRemoteSavedEnumeratePrintersCallbacks(bool sandboxed);
  RemoteSavedFetchCapabilitiesCallbacks&
  GetRemoteSavedFetchCapabilitiesCallbacks(bool sandboxed);
  RemoteSavedGetDefaultPrinterNameCallbacks&
  GetRemoteSavedGetDefaultPrinterNameCallbacks(bool sandboxed);
  RemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks&
  GetRemoteSavedGetPrinterSemanticCapsAndDefaultsCallbacks(bool sandboxed);
  RemoteSavedUseDefaultSettingsCallbacks&
  GetRemoteSavedUseDefaultSettingsCallbacks(bool sandboxed);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  RemoteSavedAskUserForSettingsCallbacks&
  GetRemoteSavedAskUserForSettingsCallbacks(bool sandboxed);
#endif
  RemoteSavedUpdatePrintSettingsCallbacks&
  GetRemoteSavedUpdatePrintSettingsCallbacks(bool sandboxed);
  RemoteSavedStartPrintingCallbacks& GetRemoteSavedStartPrintingCallbacks(
      bool sandboxed);
#if BUILDFLAG(IS_WIN)
  RemoteSavedRenderPrintedPageCallbacks&
  GetRemoteSavedRenderPrintedPageCallbacks(bool sandboxed);
#endif
  RemoteSavedRenderPrintedDocumentCallbacks&
  GetRemoteSavedRenderPrintedDocumentCallbacks(bool sandboxed);
  RemoteSavedDocumentDoneCallbacks& GetRemoteSavedDocumentDoneCallbacks(
      bool sandboxed);
  RemoteSavedCancelCallbacks& GetRemoteSavedCancelCallbacks(bool sandboxed);

  // Helper function to get the service and initialize a `context` for a given
  // `printer_name`.
  const mojo::Remote<mojom::PrintBackendService>& GetServiceAndCallbackContext(
      const std::string& printer_name,
      ClientType client_type,
      CallbackContext& context);

  // Helper functions to save outstanding callbacks.
  template <class... T, class... X>
  void SaveCallback(RemoteSavedCallbacks<T...>& saved_callbacks,
                    const RemoteId& remote_id,
                    const base::UnguessableToken& saved_callback_id,
                    base::OnceCallback<void(X...)> callback);

  // Helper functions for local callback wrappers for mojom calls.
  template <class... T, class... X>
  void ServiceCallbackDone(RemoteSavedCallbacks<T...>& saved_callbacks,
                           const RemoteId& remote_id,
                           const base::UnguessableToken& saved_callback_id,
                           X... data);

  // Local callback wrappers for mojom calls.
  void OnDidEnumeratePrinters(const CallbackContext& context,
                              mojom::PrinterListResultPtr printer_list);
  void OnDidFetchCapabilities(
      const CallbackContext& context,
      mojom::PrinterCapsAndInfoResultPtr printer_caps_and_info);
  void OnDidGetDefaultPrinterName(
      const CallbackContext& context,
      mojom::DefaultPrinterNameResultPtr printer_name);
  void OnDidGetPrinterSemanticCapsAndDefaults(
      const CallbackContext& context,
      mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps);
  void OnDidUseDefaultSettings(const CallbackContext& context,
                               mojom::PrintSettingsResultPtr settings);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void OnDidAskUserForSettings(const CallbackContext& context,
                               mojom::PrintSettingsResultPtr settings);
#endif
  void OnDidUpdatePrintSettings(const CallbackContext& context,
                                mojom::PrintSettingsResultPtr printer_caps);
  void OnDidStartPrinting(const CallbackContext& context,
                          mojom::ResultCode result);
#if BUILDFLAG(IS_WIN)
  void OnDidRenderPrintedPage(const CallbackContext& context,
                              mojom::ResultCode result);
#endif
  void OnDidRenderPrintedDocument(const CallbackContext& context,
                                  mojom::ResultCode result);
  void OnDidDocumentDone(const CallbackContext& context,
                         mojom::ResultCode result);
  void OnDidCancel(const CallbackContext& context);

  // Helper functions to run outstanding callbacks when a remote has become
  // disconnected.
  template <class T>
  void RunSavedCallbacksStructResult(
      RemoteSavedStructCallbacks<T>& saved_callbacks,
      const RemoteId& remote_id,
      mojo::StructPtr<T> result_to_clone);
  template <class... T>
  void RunSavedCallbacks(RemoteSavedCallbacks<T...>& saved_callbacks,
                         const RemoteId& remote_id,
                         T... result);

  // Test support for client ID management.
  static void SetClientsForTesting(
      const ClientsSet& query_clients,
      const QueryWithUiClientsMap& query_with_ui_clients,
      const PrintClientsMap& print_document_clients);

#if BUILDFLAG(IS_WIN)
  // Printer XML Parser implementation used to allow Print Backend Service to
  // send XML parse requests to the browser process.
  std::unique_ptr<PrinterXmlParserImpl> xml_parser_;
#endif  // BUILDFLAG(IS_WIN)

  // Bundles of remotes for the Print Backend Service and their corresponding
  // wrapping hosts, to manage these sets until they disconnect.  The sandboxed
  // and unsandboxed services are kept separate.
  RemotesBundleMap<mojom::SandboxedPrintBackendHost> sandboxed_remotes_bundles_;
  RemotesBundleMap<mojom::UnsandboxedPrintBackendHost>
      unsandboxed_remotes_bundles_;

  // Members tracking active clients to aid retention of a service process.

  // Set of IDs for clients actively engaged in printing queries.  This could
  // include any tab which has triggered Print Preview.
  ClientsSet query_clients_;

  // Set of IDs for clients actively engaged in a printing query which requires
  // the use of a UI.  Such a UI corresponds to a modal system dialog.  For
  // Linux there can be multiple of these, but for other platforms there can be
  // at most one such client.  Track the `RemoteId` which is associated with
  // each such client.
  QueryWithUiClientsMap query_with_ui_clients_;

  // Map of remote ID to the set of clients printing documents to it.
  PrintClientsMap print_document_clients_;

  // Simple counter for incrementing ClientId.  All ClientId objects are used
  // only within the browser process, so need for this to be a more complicated
  // token.
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
  RemoteSavedUseDefaultSettingsCallbacks
      sandboxed_saved_use_default_settings_callbacks_;
  RemoteSavedUseDefaultSettingsCallbacks
      unsandboxed_saved_use_default_settings_callbacks_;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  RemoteSavedAskUserForSettingsCallbacks
      sandboxed_saved_ask_user_for_settings_callbacks_;
  RemoteSavedAskUserForSettingsCallbacks
      unsandboxed_saved_ask_user_for_settings_callbacks_;
#endif
  RemoteSavedUpdatePrintSettingsCallbacks
      sandboxed_saved_update_print_settings_callbacks_;
  RemoteSavedUpdatePrintSettingsCallbacks
      unsandboxed_saved_update_print_settings_callbacks_;
  RemoteSavedStartPrintingCallbacks sandboxed_saved_start_printing_callbacks_;
  RemoteSavedStartPrintingCallbacks unsandboxed_saved_start_printing_callbacks_;
#if BUILDFLAG(IS_WIN)
  RemoteSavedRenderPrintedPageCallbacks
      sandboxed_saved_render_printed_page_callbacks_;
  RemoteSavedRenderPrintedPageCallbacks
      unsandboxed_saved_render_printed_page_callbacks_;
#endif
  RemoteSavedRenderPrintedDocumentCallbacks
      sandboxed_saved_render_printed_document_callbacks_;
  RemoteSavedRenderPrintedDocumentCallbacks
      unsandboxed_saved_render_printed_document_callbacks_;
  RemoteSavedDocumentDoneCallbacks sandboxed_saved_document_done_callbacks_;
  RemoteSavedDocumentDoneCallbacks unsandboxed_saved_document_done_callbacks_;
  RemoteSavedCancelCallbacks sandboxed_saved_cancel_callbacks_;
  RemoteSavedCancelCallbacks unsandboxed_saved_cancel_callbacks_;

  // Set of printer drivers which require elevated permissions to operate.
  // It is expected that most print drivers will succeed with the preconfigured
  // sandbox permissions.  Should any drivers be discovered to require more than
  // that (and thus fail with access denied errors) then we need to fallback to
  // performing the operation with modified restrictions.
  base::flat_set<std::string> drivers_requiring_elevated_privilege_;

#if BUILDFLAG(IS_WIN)
  // Support for process model where there can be multiple PrintBackendService
  // instances.  This is necessary because Windows printer drivers are not
  // thread safe.  Map key is a printer name.
  base::flat_map<std::string, RemoteId> remote_id_map_;
#endif

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated due to Mojo
  // message response validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  // Override of service to use for testing.
  raw_ptr<mojo::Remote<mojom::PrintBackendService>>
      sandboxed_service_remote_for_test_ = nullptr;
  raw_ptr<mojo::Remote<mojom::PrintBackendService>>
      unsandboxed_service_remote_for_test_ = nullptr;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
