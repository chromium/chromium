// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/timer/timer.h"
#include "chrome/common/extensions/api/smart_card_provider_private.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace scard_api = extensions::api::smart_card_provider_private;

using device::mojom::SmartCardCreateContextResult;
using device::mojom::SmartCardCreateContextResultPtr;
using device::mojom::SmartCardError;
using device::mojom::SmartCardListReadersResult;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardResultPtr;
using device::mojom::SmartCardStatusChangeResult;
using device::mojom::SmartCardSuccess;

namespace {
device::mojom::SmartCardResultPtr ProviderResultCodeToSmartCardResult(
    scard_api::ResultCode code) {
  switch (code) {
    case scard_api::RESULT_CODE_NONE:
      return SmartCardResult::NewError(SmartCardError::kUnknown);
    case scard_api::RESULT_CODE_SUCCESS:
      return SmartCardResult::NewSuccess(SmartCardSuccess::kOk);
    case scard_api::RESULT_CODE_REMOVED_CARD:
      return SmartCardResult::NewError(SmartCardError::kRemovedCard);
    case scard_api::RESULT_CODE_RESET_CARD:
      return SmartCardResult::NewError(SmartCardError::kResetCard);
    case scard_api::RESULT_CODE_UNPOWERED_CARD:
      return SmartCardResult::NewError(SmartCardError::kUnpoweredCard);
    case scard_api::RESULT_CODE_UNRESPONSIVE_CARD:
      return SmartCardResult::NewError(SmartCardError::kUnresponsiveCard);
    case scard_api::RESULT_CODE_UNSUPPORTED_CARD:
      return SmartCardResult::NewError(SmartCardError::kUnsupportedCard);
    case scard_api::RESULT_CODE_READER_UNAVAILABLE:
      return SmartCardResult::NewError(SmartCardError::kReaderUnavailable);
    case scard_api::RESULT_CODE_SHARING_VIOLATION:
      return SmartCardResult::NewError(SmartCardError::kSharingViolation);
    case scard_api::RESULT_CODE_NOT_TRANSACTED:
      return SmartCardResult::NewError(SmartCardError::kNotTransacted);
    case scard_api::RESULT_CODE_NO_SMARTCARD:
      return SmartCardResult::NewError(SmartCardError::kNoSmartcard);
    case scard_api::RESULT_CODE_PROTO_MISMATCH:
      return SmartCardResult::NewError(SmartCardError::kProtoMismatch);
    case scard_api::RESULT_CODE_SYSTEM_CANCELLED:
      return SmartCardResult::NewError(SmartCardError::kSystemCancelled);
    case scard_api::RESULT_CODE_NOT_READY:
      return SmartCardResult::NewError(SmartCardError::kNotReady);
    case scard_api::RESULT_CODE_CANCELLED:
      return SmartCardResult::NewError(SmartCardError::kCancelled);
    case scard_api::RESULT_CODE_INSUFFICIENT_BUFFER:
      return SmartCardResult::NewError(SmartCardError::kInsufficientBuffer);
    case scard_api::RESULT_CODE_INVALID_HANDLE:
      return SmartCardResult::NewError(SmartCardError::kInvalidHandle);
    case scard_api::RESULT_CODE_INVALID_PARAMETER:
      return SmartCardResult::NewError(SmartCardError::kInvalidParameter);
    case scard_api::RESULT_CODE_INVALID_VALUE:
      return SmartCardResult::NewError(SmartCardError::kInvalidValue);
    case scard_api::RESULT_CODE_NO_MEMORY:
      return SmartCardResult::NewError(SmartCardError::kNoMemory);
    case scard_api::RESULT_CODE_TIMEOUT:
      return SmartCardResult::NewError(SmartCardError::kTimeout);
    case scard_api::RESULT_CODE_UNKNOWN_READER:
      return SmartCardResult::NewError(SmartCardError::kUnknownReader);
    case scard_api::RESULT_CODE_UNSUPPORTED_FEATURE:
      return SmartCardResult::NewError(SmartCardError::kUnsupportedFeature);
    case scard_api::RESULT_CODE_NO_READERS_AVAILABLE:
      return SmartCardResult::NewError(SmartCardError::kNoReadersAvailable);
    case scard_api::RESULT_CODE_SERVICE_STOPPED:
      return SmartCardResult::NewError(SmartCardError::kServiceStopped);
    case scard_api::RESULT_CODE_NO_SERVICE:
      return SmartCardResult::NewError(SmartCardError::kNoService);
    case scard_api::RESULT_CODE_COMM_ERROR:
      return SmartCardResult::NewError(SmartCardError::kCommError);
    case scard_api::RESULT_CODE_INTERNAL_ERROR:
      return SmartCardResult::NewError(SmartCardError::kInternalError);
    case scard_api::RESULT_CODE_UNKNOWN_ERROR:
      return SmartCardResult::NewError(SmartCardError::kUnknownError);
    case scard_api::RESULT_CODE_SERVER_TOO_BUSY:
      return SmartCardResult::NewError(SmartCardError::kServerTooBusy);
    case scard_api::RESULT_CODE_UNEXPECTED:
      return SmartCardResult::NewError(SmartCardError::kUnexpected);
    case scard_api::RESULT_CODE_SHUTDOWN:
      return SmartCardResult::NewError(SmartCardError::kShutdown);
    case scard_api::RESULT_CODE_UNKNOWN:
      return SmartCardResult::NewError(SmartCardError::kUnknown);
  }
}

device::mojom::SmartCardReaderStateFlagsPtr ToSmartCardProviderReaderStateFlags(
    const scard_api::ReaderStateFlags& flags) {
  auto result = device::mojom::SmartCardReaderStateFlags::New();

#define CONVERT_FLAG(flag)             \
  if (flags.flag.has_value()) {        \
    result->flag = flags.flag.value(); \
  }

  CONVERT_FLAG(unaware)
  CONVERT_FLAG(ignore)
  CONVERT_FLAG(changed)
  CONVERT_FLAG(unknown)
  CONVERT_FLAG(unavailable)
  CONVERT_FLAG(empty)
  CONVERT_FLAG(present)
  CONVERT_FLAG(exclusive)
  CONVERT_FLAG(inuse)
  CONVERT_FLAG(mute)
  CONVERT_FLAG(unpowered)

#undef CONVERT_FLAG
  return result;
}

std::vector<device::mojom::SmartCardReaderStateOutPtr>
ToSmartCardProviderReaderStateOutVector(
    std::vector<scard_api::ReaderStateOut>& reader_states) {
  std::vector<device::mojom::SmartCardReaderStateOutPtr> result_vector;
  result_vector.reserve(reader_states.size());

  for (auto& reader_state : reader_states) {
    auto mojom_reader_state = device::mojom::SmartCardReaderStateOut::New();

    mojom_reader_state->reader = std::move(reader_state.reader);
    mojom_reader_state->event_state =
        ToSmartCardProviderReaderStateFlags(reader_state.event_state);
    mojom_reader_state->answer_to_reset = std::move(reader_state.atr);

    result_vector.push_back(std::move(mojom_reader_state));
  }

  return result_vector;
}

base::Value::Dict ToValue(
    const device::mojom::SmartCardReaderStateFlags& state_flags) {
  base::Value::Dict to_value_result;

  scard_api::ReaderStateFlags result;

#define CONVERT_FLAG(flag) result.flag = state_flags.flag;

  CONVERT_FLAG(unaware)
  CONVERT_FLAG(ignore)
  CONVERT_FLAG(changed)
  CONVERT_FLAG(unknown)
  CONVERT_FLAG(unavailable)
  CONVERT_FLAG(empty)
  CONVERT_FLAG(present)
  CONVERT_FLAG(exclusive)
  CONVERT_FLAG(inuse)
  CONVERT_FLAG(mute)
  CONVERT_FLAG(unpowered)

#undef CONVERT_FLAG

  return result.ToValue();
}

base::Value::Dict ToValue(
    const device::mojom::SmartCardReaderStateIn& state_in) {
  base::Value::Dict to_value_result;

  to_value_result.Set("reader", state_in.reader);
  to_value_result.Set("currentState", ToValue(*state_in.current_state.get()));

  return to_value_result;
}

}  // namespace

namespace extensions {

struct SmartCardProviderPrivateAPI::PendingEstablishContext {
  base::OneShotTimer timer;
  CreateContextCallback callback;
};

struct SmartCardProviderPrivateAPI::PendingReleaseContext {
  base::OneShotTimer timer;
};

struct SmartCardProviderPrivateAPI::PendingListReaders {
  base::OneShotTimer timer;
  device::mojom::SmartCardContext::ListReadersCallback callback;
};

struct SmartCardProviderPrivateAPI::PendingGetStatusChange {
  base::OneShotTimer timer;
  device::mojom::SmartCardContext::GetStatusChangeCallback callback;
};

// static
BrowserContextKeyedAPIFactory<SmartCardProviderPrivateAPI>*
SmartCardProviderPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<SmartCardProviderPrivateAPI>>
      instance;
  return instance.get();
}

// static
SmartCardProviderPrivateAPI& SmartCardProviderPrivateAPI::Get(
    content::BrowserContext& context) {
  return *GetFactoryInstance()->Get(&context);
}

SmartCardProviderPrivateAPI::SmartCardProviderPrivateAPI(
    content::BrowserContext* context)
    : browser_context_(raw_ref<content::BrowserContext>::from_ptr(context)),
      event_router_(raw_ref<EventRouter>::from_ptr(
          EventRouterFactory::GetForBrowserContext(context))) {
  context_receivers_.set_disconnect_handler(base::BindRepeating(
      &SmartCardProviderPrivateAPI::OnMojoContextDisconnected,
      weak_ptr_factory_.GetWeakPtr()));
}

SmartCardProviderPrivateAPI::~SmartCardProviderPrivateAPI() = default;

void SmartCardProviderPrivateAPI::CreateContext(
    CreateContextCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestId request_id = request_id_generator_.GenerateNextId();

  base::Value::List event_args;
  event_args.Append(request_id.GetUnsafeValue());

  auto event = std::make_unique<extensions::Event>(
      extensions::events::
          SMART_CARD_PROVIDER_PRIVATE_ON_ESTABLISH_CONTEXT_REQUESTED,
      scard_api::OnEstablishContextRequested::kEventName, std::move(event_args),
      &*browser_context_);

  const std::string provider_extension_id = GetListenerExtensionId(*event);

  if (provider_extension_id.empty()) {
    std::move(callback).Run(
        SmartCardCreateContextResult::NewError(SmartCardError::kNoService));
    return;
  }

  auto pending = std::make_unique<PendingEstablishContext>();
  pending->callback = std::move(callback);
  pending->timer.Start(
      FROM_HERE, response_time_limit_,
      base::BindOnce(&SmartCardProviderPrivateAPI::OnEstablishContextTimeout,
                     weak_ptr_factory_.GetWeakPtr(), provider_extension_id,
                     request_id));

  pending_establish_context_[request_id] = std::move(pending);

  event_router_->DispatchEventToExtension(provider_extension_id,
                                          std::move(event));
}

void SmartCardProviderPrivateAPI::OnMojoContextDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ContextId scard_context = context_receivers_.current_context();
  DCHECK(!scard_context.is_null());

  ProviderReleaseContext(scard_context);
}

void SmartCardProviderPrivateAPI::ProviderReleaseContext(
    ContextId scard_context) {
  RequestId request_id = request_id_generator_.GenerateNextId();

  base::Value::List event_args;
  event_args.Append(request_id.GetUnsafeValue());
  event_args.Append(scard_context.GetUnsafeValue());

  auto event = std::make_unique<extensions::Event>(
      extensions::events::
          SMART_CARD_PROVIDER_PRIVATE_ON_RELEASE_CONTEXT_REQUESTED,
      scard_api::OnReleaseContextRequested::kEventName, std::move(event_args),
      &*browser_context_);

  const std::string provider_extension_id = GetListenerExtensionId(*event);

  if (provider_extension_id.empty()) {
    return;
  }

  auto pending = std::make_unique<PendingReleaseContext>();
  pending->timer.Start(
      FROM_HERE, response_time_limit_,
      base::BindOnce(&SmartCardProviderPrivateAPI::OnReleaseContextTimeout,
                     weak_ptr_factory_.GetWeakPtr(), provider_extension_id,
                     request_id));

  pending_release_context_[request_id] = std::move(pending);

  event_router_->DispatchEventToExtension(provider_extension_id,
                                          std::move(event));
}

void SmartCardProviderPrivateAPI::ReportEstablishContextResult(
    RequestId request_id,
    ContextId scard_context,
    SmartCardResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_establish_context_.find(request_id);
  if (it == pending_establish_context_.end()) {
    if (result->is_success() && !scard_context.is_null()) {
      LOG(WARNING) << "Releasing scard_context from an unknown "
                      "EstablishContext request.";
      ProviderReleaseContext(scard_context);
    }
    return;
  }

  std::unique_ptr<PendingEstablishContext> pending = std::move(it->second);
  pending_establish_context_.erase(it);

  SmartCardCreateContextResultPtr context_result;

  if (result->is_success()) {
    if (scard_context) {
      mojo::PendingRemote<device::mojom::SmartCardContext> context_remote;
      context_receivers_.Add(
          this, context_remote.InitWithNewPipeAndPassReceiver(), scard_context);
      context_result =
          SmartCardCreateContextResult::NewContext(std::move(context_remote));
    } else {
      LOG(ERROR) << "Provider reported an invalid scard_context value: "
                 << scard_context.GetUnsafeValue();
      // Just ignore this result.
      context_result = SmartCardCreateContextResult::NewError(
          SmartCardError::kInternalError);
    }
  } else {
    context_result =
        SmartCardCreateContextResult::NewError(result->get_error());
  }

  std::move(pending->callback).Run(std::move(context_result));
}

void SmartCardProviderPrivateAPI::ReportReleaseContextResult(
    RequestId request_id,
    SmartCardResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_release_context_.find(request_id);
  if (it == pending_release_context_.end()) {
    return;
  }

  std::unique_ptr<PendingReleaseContext> pending = std::move(it->second);
  pending_release_context_.erase(it);

  if (result->is_error()) {
    // There's nothing really to be done about it.
    LOG(WARNING) << "Failed to release context: " << result->get_error();
  }
}

void SmartCardProviderPrivateAPI::ReportListReadersResult(
    RequestId request_id,
    std::vector<std::string> readers,
    SmartCardResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_list_readers_.find(request_id);
  if (it == pending_list_readers_.end()) {
    return;
  }

  std::unique_ptr<PendingListReaders> pending = std::move(it->second);
  pending_list_readers_.erase(it);

  std::move(pending->callback)
      .Run(result->is_success()
               ? SmartCardListReadersResult::NewReaders(std::move(readers))
               : SmartCardListReadersResult::NewError(result->get_error()));
}

void SmartCardProviderPrivateAPI::ReportGetStatusChangeResult(
    RequestId request_id,
    std::vector<device::mojom::SmartCardReaderStateOutPtr> reader_states,
    SmartCardResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_get_status_change_.find(request_id);
  if (it == pending_get_status_change_.end()) {
    return;
  }

  std::unique_ptr<PendingGetStatusChange> pending = std::move(it->second);
  pending_get_status_change_.erase(it);

  device::mojom::SmartCardStatusChangeResultPtr status_change_result;

  if (result->is_success()) {
    status_change_result =
        SmartCardStatusChangeResult::NewReaderStates(std::move(reader_states));
  } else {
    status_change_result =
        SmartCardStatusChangeResult::NewError(result->get_error());
  }

  std::move(pending->callback).Run(std::move(status_change_result));
}

void SmartCardProviderPrivateAPI::SetResponseTimeLimitForTesting(
    base::TimeDelta value) {
  response_time_limit_ = value;
}

// TODO(crbug.com/1386175): Consider if we need to wait for a known
// SmartCard provider Extension to load or finish installation
// before querying for listeners.
// Use case is if the Web API is used immediately after a user logs
// in.
std::string SmartCardProviderPrivateAPI::GetListenerExtensionId(
    const extensions::Event& event) {
  std::set<const extensions::EventListener*> listener_set =
      event_router_->listeners().GetEventListeners(event);

  if (listener_set.empty()) {
    LOG(ERROR) << "No extension listening to " << event.event_name << ".";
    return std::string();
  }

  // Allow list on the extension API permission enforces that there can't
  // be multiple extensions with access to it. Thus don't bother
  // iterating through the set.
  return (*listener_set.cbegin())->extension_id();
}

void SmartCardProviderPrivateAPI::ListReaders(ListReadersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ContextId scard_context = context_receivers_.current_context();
  DCHECK(!scard_context.is_null());

  RequestId request_id = request_id_generator_.GenerateNextId();

  base::Value::List event_args;
  event_args.Append(request_id.GetUnsafeValue());
  event_args.Append(scard_context.GetUnsafeValue());

  auto event = std::make_unique<extensions::Event>(
      extensions::events::SMART_CARD_PROVIDER_PRIVATE_ON_LIST_READERS_REQUESTED,
      scard_api::OnListReadersRequested::kEventName, std::move(event_args),
      &*browser_context_);

  const std::string provider_extension_id = GetListenerExtensionId(*event);
  if (provider_extension_id.empty()) {
    std::move(callback).Run(
        SmartCardListReadersResult::NewError(SmartCardError::kNoService));
    return;
  }

  auto pending = std::make_unique<PendingListReaders>();
  pending->callback = std::move(callback);
  pending->timer.Start(
      FROM_HERE, response_time_limit_,
      base::BindOnce(&SmartCardProviderPrivateAPI::OnListReadersTimeout,
                     weak_ptr_factory_.GetWeakPtr(), provider_extension_id,
                     request_id));

  pending_list_readers_[request_id] = std::move(pending);

  event_router_->DispatchEventToExtension(provider_extension_id,
                                          std::move(event));
}

void SmartCardProviderPrivateAPI::GetStatusChange(
    base::TimeDelta time_delta,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    GetStatusChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ContextId scard_context = context_receivers_.current_context();
  DCHECK(!scard_context.is_null());

  RequestId request_id = request_id_generator_.GenerateNextId();

  base::Value::List event_args;
  event_args.Append(request_id.GetUnsafeValue());
  event_args.Append(scard_context.GetUnsafeValue());

  const bool finite_timeout =
      !time_delta.is_inf() &&
      time_delta.InMilliseconds() < int64_t(std::numeric_limits<int>::max());

  scard_api::Timeout timeout;
  if (finite_timeout) {
    timeout.milliseconds = int(time_delta.InMilliseconds());
  }
  event_args.Append(timeout.ToValue());

  base::Value::List reader_states_list;
  for (const auto& reader_state : reader_states) {
    reader_states_list.Append(ToValue(*reader_state.get()));
  }
  event_args.Append(std::move(reader_states_list));

  auto event = std::make_unique<extensions::Event>(
      events::SMART_CARD_PROVIDER_PRIVATE_ON_GET_STATUS_CHANGE_REQUESTED,
      scard_api::OnGetStatusChangeRequested::kEventName, std::move(event_args),
      &*browser_context_);

  const std::string provider_extension_id = GetListenerExtensionId(*event);
  if (provider_extension_id.empty()) {
    std::move(callback).Run(
        SmartCardStatusChangeResult::NewError(SmartCardError::kNoService));
    return;
  }

  auto pending = std::make_unique<PendingGetStatusChange>();
  pending->callback = std::move(callback);
  if (finite_timeout) {
    pending->timer.Start(
        FROM_HERE, std::max(base::Milliseconds(500), time_delta * 2),
        base::BindOnce(&SmartCardProviderPrivateAPI::OnGetStatusChangeTimeout,
                       weak_ptr_factory_.GetWeakPtr(), provider_extension_id,
                       request_id));
  }
  pending_get_status_change_[request_id] = std::move(pending);

  event_router_->DispatchEventToExtension(provider_extension_id,
                                          std::move(event));
}

#define ON_TIMEOUT_IMPL(FunctionName, ...)                                \
  void SmartCardProviderPrivateAPI::On##FunctionName##Timeout(            \
      const std::string& provider_extension_id, RequestId request_id) {   \
    LOG(ERROR) << "Provider extension " << provider_extension_id          \
               << " did not report the result of " << #FunctionName       \
               << " (request id " << request_id.GetUnsafeValue() << ")."; \
    Report##FunctionName##Result(request_id, __VA_ARGS__);                \
  }

ON_TIMEOUT_IMPL(EstablishContext,
                ContextId(),
                SmartCardResult::NewError(SmartCardError::kNoService))

ON_TIMEOUT_IMPL(ReleaseContext,
                SmartCardResult::NewError(SmartCardError::kNoService))

ON_TIMEOUT_IMPL(ListReaders,
                std::vector<std::string>(),
                SmartCardResult::NewError(SmartCardError::kNoService))

ON_TIMEOUT_IMPL(GetStatusChange,
                std::vector<device::mojom::SmartCardReaderStateOutPtr>(),
                SmartCardResult::NewError(SmartCardError::kNoService))

#undef ON_TIMEOUT_IMPL

#define REPORT_RESULT_FUNCTION_IMPL(FunctionName, ...)                       \
  SmartCardProviderPrivateReport##FunctionName##ResultFunction::             \
      ~SmartCardProviderPrivateReport##FunctionName##ResultFunction() =      \
          default;                                                           \
  ExtensionFunction::ResponseAction                                          \
      SmartCardProviderPrivateReport##FunctionName##ResultFunction::Run() {  \
    std::unique_ptr<scard_api::Report##FunctionName##Result::Params> params( \
        scard_api::Report##FunctionName##Result::Params::Create(args()));    \
    EXTENSION_FUNCTION_VALIDATE(params);                                     \
                                                                             \
    SmartCardProviderPrivateAPI::RequestId request_id(params->request_id);   \
                                                                             \
    auto& scard_provider =                                                   \
        SmartCardProviderPrivateAPI::Get(*browser_context());                \
    scard_provider.Report##FunctionName##Result(request_id, __VA_ARGS__);    \
    return RespondNow(NoArguments());                                        \
  }

REPORT_RESULT_FUNCTION_IMPL(
    EstablishContext,
    SmartCardProviderPrivateAPI::ContextId(params->scard_context),
    ProviderResultCodeToSmartCardResult(params->result_code))

REPORT_RESULT_FUNCTION_IMPL(
    ReleaseContext,
    ProviderResultCodeToSmartCardResult(params->result_code))

REPORT_RESULT_FUNCTION_IMPL(
    ListReaders,
    std::move(params->readers),
    ProviderResultCodeToSmartCardResult(params->result_code))

REPORT_RESULT_FUNCTION_IMPL(
    GetStatusChange,
    ToSmartCardProviderReaderStateOutVector(params->reader_states),
    ProviderResultCodeToSmartCardResult(params->result_code))

#undef REPORT_RESULT_FUNCTION_IMPL

}  // namespace extensions
