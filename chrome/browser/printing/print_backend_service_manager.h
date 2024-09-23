// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/types/strong_alias.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "printing/buildflags/buildflags.h"
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
  // A RemoteId is used to identify a particular PrintBackendService that
  // will be servicing queries and printing of a document.  This abstraction
  // allows for identifying the service desired to be used, without relying
  // upon a printer name (which sometimes is not available, such as when
  // doing general queries).
  using RemoteId = base::StrongAlias<class RemoteIdTag, uint32_t>;

  // A ClientId represents a printing action that is being performed by a
  // browser tab.  There can be different ClientIds depending upon the
  // action that is being performed:
  // - During Print Preview, the tab will have a ClientId for the related
  //   queries.  This ClientId might make use of multiple different RemoteIds,
  //   depending upon the destinations selected during the preview.
  // - If a user initiates system print, then a different ClientId is used to
  //   manage the actions of the system dialog.
  // - Once it is time to print a document, a different ClientId is used for
  //   managing the printing sequence.
  // For system print dialog and document printing, using the same RemoteId
  // between the two different clients is important to be able to maintain the
  // same device context in the PrintBackendService.
  using ClientId = base::StrongAlias<class ClientIdTag, uint32_t>;

  // A ContextId is an abstraction of a printing context which resides in the
  // PrintBackendService.  There can be multiple ContextIds associated with a
  // RemoteId, since a service could be supporting a system print dialog as
  // well as multiple documents being printed.  A ContextId is only ever
  // associated with a single RemoteId.
  // For system print dialogs, the ContextId used to get the settings will
  // be shared with another ClientId for printing the document, so that the
  // same device context settings are used at printing time.
  using ContextId = base::StrongAlias<class ContextIdTag, uint32_t>;

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

  // Launch a service that is intended to persist indefinitely and can be used
  // by all further clients.
  static void LaunchPersistentService();

  // Client registration routines.  These act as a signal of impending activity
  // enabling possible optimizations within the manager.  They return an ID
  // which the callers are to use with `UnregisterClient()` once they have
  // completed their printing activity.

  // Register as a client of PrintBackendServiceManager for print queries.
  ClientId RegisterQueryClient();

  // Register as a client of PrintBackendServiceManager for print queries which
  // require a system print dialog UI.  If a platform cannot support concurrent
  // queries of this type then this will return `std::nullopt` if another
  // client is already registered.
  std::optional<ClientId> RegisterQueryWithUiClient();

  // Register as a client of PrintBackendServiceManager for printing a document
  // to a specific printer.
  ClientId RegisterPrintDocumentClient(const std::string& printer_name);

  // Register as a client of PrintBackendServiceManager for printing a document
  // to a specific printer.  Use the same `RemoteId` for this new printing
  // client as has been used by the indicated query with UI client.  This method
  // will DCHECK if the client ID provided is not for a query with UI client.
  // Call can return nullopt if the service has terminated by the time this call
  // is made and the remote used by client `id` no longer exists.
  std::optional<ClientId> RegisterPrintDocumentClientReusingClientRemote(
      ClientId id);

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback);
#endif
#if BUILDFLAG(IS_WIN)
  void GetPaperPrintableArea(
      const std::string& printer_name,
      const PrintSettings::RequestedMedia& media,
      mojom::PrintBackendService::GetPaperPrintableAreaCallback callback);
#endif
  ContextId EstablishPrintingContext(ClientId client_id,
                                     const std::string& printer_name
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
                                     ,
                                     gfx::NativeView parent_view
#endif
  );
  void UseDefaultSettings(
      ClientId client_id,
      ContextId context_id,
      mojom::PrintBackendService::UseDefaultSettingsCallback callback);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void AskUserForSettings(
      ClientId client_id,
      ContextId context_id,
      int max_pages,
      bool has_selection,
      bool is_scripted,
      mojom::PrintBackendService::AskUserForSettingsCallback callback);
#endif
  // `UpdatePrintSettings()` can be used prior to initiating a system print
  // dialog or right before starting to print a document.  The first requires a
  // `client_id` of `kQueryWithUi` type, while the latter requires a the ID to
  // be of type `kPrintDocument`.
  // The destination printer is still unknown when initiating a system print
  // dialog, so `printer_name` will be empty in this case.  The destination
  // must be known when starting to print a document.  `UpdatePrintSettings()`
  // uses this insight to know what kind of client type is to be expected for
  // the provided `client_id`.  The function will CHECK if the `client_id`
  // is not registered for the expected type.
  void UpdatePrintSettings(
      ClientId client_id,
      const std::string& printer_name,
      ContextId context_id,
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback);
  // `StartPrinting()` initiates the printing of a document.  The optional
  // `settings` is used in the case where a system print dialog is invoked
  // from in the browser, and this provides those settings for printing.
  void StartPrinting(
      ClientId client_id,
      const std::string& printer_name,
      ContextId context_id,
      int document_cookie,
      const std::u16string& document_name,
#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      std::optional<PrintSettings> settings,
#endif
      mojom::PrintBackendService::StartPrintingCallback callback);
#if BUILDFLAG(IS_WIN)
  void RenderPrintedPage(
      ClientId client_id,
      const std::string& printer_name,
      int document_cookie,
      const PrintedPage& page,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page_data,
      mojom::PrintBackendService::RenderPrintedPageCallback callback);
#endif
  void RenderPrintedDocument(
      ClientId client_id,
      const std::string& printer_name,
      int document_cookie,
      uint32_t page_count,
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_data,
      mojom::PrintBackendService::RenderPrintedDocumentCallback callback);
  void DocumentDone(ClientId client_id,
                    const std::string& printer_name,
                    int document_cookie,
                    mojom::PrintBackendService::DocumentDoneCallback callback);
  void Cancel(ClientId client_id,
              const std::string& printer_name,
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
  // of `remote`.  Can be reset by passing in nullptr.
  void SetServiceForTesting(mojo::Remote<mojom::PrintBackendService>* remote);

  // Overrides the print backend service for testing when an alternate service
  // is required for fallback processing after an access denied error.  Caller
  // retains ownership of `remote`.  Can be reset by passing in nullptr.
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
#if BUILDFLAG(IS_WIN)
  using RemoteSavedGetPaperPrintableAreaCallbacks =
      RemoteSavedCallbacks<const gfx::Rect&>;
#endif
  using RemoteSavedUseDefaultSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  using RemoteSavedAskUserForSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
#endif
  using RemoteSavedUpdatePrintSettingsCallbacks =
      RemoteSavedStructCallbacks<mojom::PrintSettingsResult>;
  using RemoteSavedStartPrintingCallbacks =
      RemoteSavedCallbacks<mojom::ResultCode, int /*job_id*/>;
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

  struct ServiceAndCallbackContext {
    ServiceAndCallbackContext(
        CallbackContext callback_context,
        const mojo::Remote<mojom::PrintBackendService>& backend_service);
    ServiceAndCallbackContext(ServiceAndCallbackContext&& other) = delete;
    ~ServiceAndCallbackContext();
    CallbackContext context;
    const raw_ref<const mojo::Remote<mojom::PrintBackendService>> service;
  };

  PrintBackendServiceManager();
  ~PrintBackendServiceManager();

  static std::string ClientTypeToString(ClientType client_type);

  static void LogCallToRemote(std::string_view name,
                              const CallbackContext& context);
  static void LogCallbackFromRemote(std::string_view name,
                                    const CallbackContext& context);

  void SetCrashKeys(const std::string& printer_name);

  // Determine the remote ID that is used for the specified `printer_name`.
  // Could generate a new RemoteId if one has not been previously created
  // for the indicated printer.
  RemoteId GetRemoteIdForPrinterName(const std::string& printer_name);

  // Determine the remote ID that is used for the specified `client_id` of a
  // query with UI client.  Will crash if no such client is found.
  RemoteId GetRemoteIdForQueryWithUiClientId(ClientId client_id) const;

  // Determine the remote ID that is used for the specified `client_id` of a
  // print document client.  Will crash if no such client is found.
  RemoteId GetRemoteIdForPrintDocumentClientId(ClientId client_id) const;

  // Common helper for registering clients.  The `destination` parameter can be
  // either a `std::string` for a printer name or a `RemoteId` which was
  // generated from a prior registration.  This method will DCHECK if the
  // `destination` is a `RemoteId` and the registration requires launching
  // another service instance.
  std::optional<ClientId> RegisterClient(
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
      ClientType client_type) const;
#endif

  // Determines if a service should be sandboxed when launched.
  bool ShouldServiceBeSandboxed(const std::string& printer_name,
                                ClientType client_type) const;

  // Acquires a remote handle to the Print Backend Service instance, launching a
  // process to host the service if necessary.
  const mojo::Remote<mojom::PrintBackendService>&
  GetService(const RemoteId& remote_id, ClientType client_type, bool sandboxed);

  // Helper to `GetService` for a particular remotes bundle type.
  template <class T>
  mojo::Remote<mojom::PrintBackendService>& GetServiceFromBundle(
      const RemoteId& remote_id,
      ClientType client_type,
      bool sandboxed,
      RemotesBundleMap<T>& bundle_map);

  // Get the idle timeout value to user for a particular client type.
  constexpr base::TimeDelta GetClientTypeIdleTimeout(
      ClientType client_type) const;

  // Whether any clients are queries with UI to `remote_id`.
  bool HasQueryWithUiClientForRemoteId(const RemoteId& remote_id) const;

  // Whether any clients are printing documents to `remote_id`.
  bool HasPrintDocumentClientForRemoteId(const RemoteId& remote_id) const;

  // Get the number of clients printing documents to `remote_id`.
  size_t GetPrintDocumentClientsCountForRemoteId(
      const RemoteId& remote_id) const;

  // Determine if idle timeout should be modified based upon there having been
  // a new client registered for `registered_client_type`.
  std::optional<base::TimeDelta> DetermineIdleTimeoutUpdateOnRegisteredClient(
      ClientType registered_client_type,
      const RemoteId& remote_id) const;

  // Determine if idle timeout should be modified after a client of type
  // `unregistered_client_type` has been unregistered.
  std::optional<base::TimeDelta> DetermineIdleTimeoutUpdateOnUnregisteredClient(
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
#if BUILDFLAG(IS_WIN)
  RemoteSavedGetPaperPrintableAreaCallbacks&
  GetRemoteSavedGetPaperPrintableAreaCallbacks(bool sandboxed);
#endif
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
  // `printer_name`.  This is used for calls supporting Print Preview, where
  // the client type is `kQuery`.
  ServiceAndCallbackContext GetServiceAndCallbackContextForQuery(
      const std::string& printer_name);

  // Helper function to get the service and initialize a `context` for a given
  // query with UI `client_id`.  Use `printer_name` for extra sandbox behavior
  // handling.  This is used for calls supporting system print dialogs and
  // printing of a document.
  ServiceAndCallbackContext GetServiceAndCallbackContextForQueryWithUiClient(
      ClientId client_id,
      const std::string& printer_name);

  // Helper function to get the service and initialize a `context` for a given
  // print document `client_id`.  Use `printer_name` for extra sandbox behavior
  // handling.  This is used for calls supporting system print dialogs and
  // printing of a document.
  ServiceAndCallbackContext GetServiceAndCallbackContextForPrintDocumentClient(
      ClientId client_id,
      const std::string& printer_name);

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
#if BUILDFLAG(IS_WIN)
  void OnDidGetPaperPrintableArea(const CallbackContext& context,
                                  const gfx::Rect& printable_area_um);
#endif
  void OnDidUseDefaultSettings(const CallbackContext& context,
                               mojom::PrintSettingsResultPtr settings);
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  void OnDidAskUserForSettings(const CallbackContext& context,
                               mojom::PrintSettingsResultPtr settings);
#endif
  void OnDidUpdatePrintSettings(const CallbackContext& context,
                                mojom::PrintSettingsResultPtr printer_caps);
  void OnDidStartPrinting(const CallbackContext& context,
                          mojom::ResultCode result,
                          int job_id);
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
                         typename std::remove_reference<T>::type... result);

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
  // only within the browser process, so no need for this to be a more
  // complicated token.
  uint32_t last_client_id_ = 0;

  // Simple counter for incrementing ContextId.  ContextId objects are passed
  // as parameters to the service but are never provided back, so no need for
  // this to be a more complicated token.
  uint32_t last_context_id_ = 0;

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
#if BUILDFLAG(IS_WIN)
  RemoteSavedGetPaperPrintableAreaCallbacks
      sandboxed_saved_get_paper_printable_area_callbacks_;
  RemoteSavedGetPaperPrintableAreaCallbacks
      unsandboxed_saved_get_paper_printable_area_callbacks_;
#endif
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

  // Gets set to false once there has been at least one attempt to print using
  // a sandboxed PrintBackend service.  Used for metrics reporting.
  bool first_sandboxed_print_ = true;

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

  // Used as base for generating `RemoteId` values.  Only used internally
  // within browser process management code, so a simple incrementating
  // sequence is sufficient.
  uint32_t remote_id_sequence_ = 0;

  // Set when launched services are intended to persist indefinitely, rather
  // than being disconnected after a finite idle timeout expires.
  bool persistent_service_ = false;

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
