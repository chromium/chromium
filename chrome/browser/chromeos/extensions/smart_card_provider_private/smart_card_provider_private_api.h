// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace extensions {
struct Event;
class EventRouter;

// Implements device::mojom::SmartCardContextFactory (and the other
// device::mojom::SmartCard interfaces) by talking to the extension that is
// listening to chrome.smartCardProviderPrivate events.
class SmartCardProviderPrivateAPI
    : public BrowserContextKeyedAPI,
      public device::mojom::SmartCardContextFactory,
      public device::mojom::SmartCardContext {
 public:
  // Uniquely identifies a request sent by this class to the PC/SC provider
  // extension.
  using RequestId = base::IdType32<class SmartCardRequestIdClass>;
  // A smart card context id, as given by the PC/SC provider extension.
  using ContextId = base::IdType32<class SmartCardContextIdClass>;

  static BrowserContextKeyedAPIFactory<SmartCardProviderPrivateAPI>*
  GetFactoryInstance();

  // Convenience method to get the SmartCardProviderPrivateAPI for a
  // BrowserContext.
  static SmartCardProviderPrivateAPI& Get(content::BrowserContext& context);

  explicit SmartCardProviderPrivateAPI(content::BrowserContext* context);

  ~SmartCardProviderPrivateAPI() override;

  // device::mojom::SmartCardContextFactory overrides:
  void CreateContext(CreateContextCallback) override;

  // device::mojom::SmartCardContext overrides:
  void ListReaders(ListReadersCallback callback) override;
  void GetStatusChange(
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      GetStatusChangeCallback callback) override;

  // Called by extension functions:
  void ReportEstablishContextResult(RequestId request_id,
                                    ContextId scard_context,
                                    device::mojom::SmartCardResultPtr result);
  void ReportReleaseContextResult(RequestId request_id,
                                  device::mojom::SmartCardResultPtr result);
  void ReportListReadersResult(RequestId request_id,
                               std::vector<std::string> readers,
                               device::mojom::SmartCardResultPtr result);
  void ReportGetStatusChangeResult(
      RequestId request_id,
      std::vector<device::mojom::SmartCardReaderStateOutPtr> reader_states,
      device::mojom::SmartCardResultPtr result);

  void SetResponseTimeLimitForTesting(base::TimeDelta);

 private:
  // BrowserContextKeyedAPI:
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const char* service_name() { return "SmartCardProviderPrivateAPI"; }

  friend class BrowserContextKeyedAPIFactory<SmartCardProviderPrivateAPI>;

  void ProviderReleaseContext(ContextId scard_context);

  // Called when a device::mojom::SmartCardContext loses its mojo connection.
  // eg: because its mojo Remote was destroyed.
  void OnMojoContextDisconnected();

  std::string GetListenerExtensionId(const extensions::Event& event);

  void OnEstablishContextTimeout(const std::string& provider_extension_id,
                                 RequestId request_id);
  void OnReleaseContextTimeout(const std::string& provider_extension_id,
                               RequestId request_id);
  void OnListReadersTimeout(const std::string& provider_extension_id,
                            RequestId request_id);
  void OnGetStatusChangeTimeout(const std::string& provider_extension_id,
                                RequestId request_id);

  SEQUENCE_CHECKER(sequence_checker_);

  base::TimeDelta response_time_limit_{base::Minutes(5)};

  struct PendingEstablishContext;
  std::map<RequestId, std::unique_ptr<PendingEstablishContext>>
      pending_establish_context_;

  struct PendingReleaseContext;
  std::map<RequestId, std::unique_ptr<PendingReleaseContext>>
      pending_release_context_;

  struct PendingListReaders;
  std::map<RequestId, std::unique_ptr<PendingListReaders>>
      pending_list_readers_;

  struct PendingGetStatusChange;
  std::map<RequestId, std::unique_ptr<PendingGetStatusChange>>
      pending_get_status_change_;

  RequestId::Generator request_id_generator_;
  const raw_ref<content::BrowserContext> browser_context_;
  const raw_ref<EventRouter> event_router_;

  mojo::ReceiverSet<device::mojom::SmartCardContext, ContextId>
      context_receivers_;

  base::WeakPtrFactory<SmartCardProviderPrivateAPI> weak_ptr_factory_{this};
};

class SmartCardProviderPrivateReportEstablishContextResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportEstablishContextResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportEstablishContextResult",
      SMARTCARDPROVIDERPRIVATE_REPORTESTABLISHCONTEXTRESULT)
};

class SmartCardProviderPrivateReportReleaseContextResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportReleaseContextResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportReleaseContextResult",
      SMARTCARDPROVIDERPRIVATE_REPORTRELEASECONTEXTRESULT)
};

class SmartCardProviderPrivateReportListReadersResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportListReadersResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("smartCardProviderPrivate.reportListReadersResult",
                             SMARTCARDPROVIDERPRIVATE_REPORTLISTREADERSRESULT)
};

class SmartCardProviderPrivateReportGetStatusChangeResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportGetStatusChangeResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportGetStatusChangeResult",
      SMARTCARDPROVIDERPRIVATE_REPORTGETSTATUSCHANGERESULT)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_
