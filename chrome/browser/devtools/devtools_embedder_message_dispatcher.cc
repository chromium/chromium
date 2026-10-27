// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "chrome/browser/devtools/devtools_settings.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/visual_logging.h"

namespace {

using DispatchCallback = DevToolsEmbedderMessageDispatcher::DispatchCallback;

bool GetValue(const base::Value& value,
              DevToolsDispatchHttpRequestParams& params) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }
  auto parsed_params = DevToolsDispatchHttpRequestParams::FromDict(*dict);
  if (!parsed_params) {
    return false;
  }
  params = std::move(*parsed_params);
  return true;
}

bool GetValue(const base::Value& value, std::string& result) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return false;
  }
  result = *str;
  return true;
}

bool GetValue(const base::Value& value, int& result) {
  return base::OptionalUnwrapTo(value.GetIfInt(), result);
}

bool GetValue(const base::Value& value, double& result) {
  return base::OptionalUnwrapTo(value.GetIfDouble(), result);
}

bool GetValue(const base::Value& value, bool& result) {
  return base::OptionalUnwrapTo(value.GetIfBool(), result);
}

bool GetValue(const base::Value& value, gfx::Rect& rect) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }
  std::optional<int> x = dict->FindInt("x");
  std::optional<int> y = dict->FindInt("y");
  std::optional<int> width = dict->FindInt("width");
  std::optional<int> height = dict->FindInt("height");
  if (!x.has_value() || !y.has_value() || !width.has_value() ||
      !height.has_value()) {
    return false;
  }

  rect.SetRect(x.value(), y.value(), width.value(), height.value());
  return true;
}

bool GetValue(const base::Value& value, RegisterOptions& options) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  const bool synced = dict->FindBool("synced").value_or(false);
  options.sync_mode = synced ? RegisterOptions::SyncMode::kSync
                             : RegisterOptions::SyncMode::kDontSync;
  return true;
}

bool GetValue(const base::Value& value, ImpressionEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  const base::ListValue* impressions = dict->FindList("impressions");
  if (!impressions) {
    return false;
  }
  for (const auto& impression : *impressions) {
    const base::DictValue* impression_dict = impression.GetIfDict();
    if (!impression_dict) {
      return false;
    }
    std::optional<double> id = impression_dict->FindDouble("id");
    std::optional<int> type = impression_dict->FindInt("type");
    if (!id || !type) {
      return false;
    }

    auto& back = event.impressions.emplace_back(
        VisualElementImpression{static_cast<int64_t>(*id), *type});

    base::OptionalUnwrapTo(impression_dict->FindDouble("parent"), back.parent);
    base::OptionalUnwrapTo(impression_dict->FindInt("context"), back.context);
    base::OptionalUnwrapTo(impression_dict->FindInt("width"), back.width);
    base::OptionalUnwrapTo(impression_dict->FindInt("height"), back.height);
  }
  return true;
}

bool GetValue(const base::Value& value, ResizeEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  if (!base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid)) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("width"), event.width);
  base::OptionalUnwrapTo(dict->FindInt("height"), event.height);
  return true;
}

bool GetValue(const base::Value& value, ClickEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  if (!base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid)) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("mouseButton"), event.mouse_button);
  base::OptionalUnwrapTo(dict->FindInt("doubleClick"), event.double_click);
  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

bool GetValue(const base::Value& value, HoverEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  if (!base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid)) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("time"), event.time);
  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

bool GetValue(const base::Value& value, DragEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  if (!base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid)) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("distance"), event.distance);
  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

bool GetValue(const base::Value& value, ChangeEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  if (!base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid)) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

bool GetValue(const base::Value& value, KeyDownEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindDouble("veid"), event.veid);
  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

bool GetValue(const base::Value& value, SettingAccessEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("name"), event.name);
  base::OptionalUnwrapTo(dict->FindInt("numeric_value"), event.numeric_value);
  base::OptionalUnwrapTo(dict->FindInt("string_value"), event.string_value);
  return true;
}

bool GetValue(const base::Value& value, FunctionCallEvent& event) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return false;
  }

  base::OptionalUnwrapTo(dict->FindInt("name"), event.name);
  base::OptionalUnwrapTo(dict->FindInt("context"), event.context);
  return true;
}

template <typename T>
struct StorageTraits {
  using StorageType = T;
};

template <typename T>
struct StorageTraits<const T&> {
  using StorageType = T;
};

template <typename T>
constexpr bool kIsOptionalParam = false;

template <typename T>
constexpr bool kIsOptionalParam<std::optional<T>> = true;

// A parameter of type std::optional<T> may be sent as null by the frontend
// (which is what JSON.stringify() turns an undefined argument into), or be
// omitted altogether if no later parameter is sent.
template <typename T>
bool GetValue(const base::Value& value, std::optional<T>& result) {
  if (value.is_none()) {
    result.reset();
    return true;
  }
  T parsed;
  if (!GetValue(value, parsed)) {
    return false;
  }
  result = std::move(parsed);
  return true;
}

// The parameters of a handler, of types `Ts...`, as parsed from the arguments
// of an embedder message: Parse() parses the arguments in [`it`, `end`) into
// `Ts...` one by one, the N-th argument into the N-th parameter, and succeeds
// if and only if every argument was parsed into a parameter of the matching
// type. There may be fewer arguments than parameters, as long as all the
// parameters that are left without an argument are std::optional<T>s; those
// stay empty.
template <typename... Ts>
struct ParamTuple {
  // No parameters are left, so every argument must have been consumed.
  bool Parse(base::ListValue::const_iterator it,
             base::ListValue::const_iterator end) {
    return it == end;
  }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    handler.Run(std::forward<As>(args)...);
  }
};

template <typename T, typename... Ts>
struct ParamTuple<T, Ts...> {
  bool Parse(base::ListValue::const_iterator it,
             base::ListValue::const_iterator end) {
    if (it == end) {
      // The arguments ran out before the parameters did, so `head` gets no
      // value. That is fine if and only if `head` is a std::optional<T>, which
      // then stays empty, and (checked by the recursion) so is every parameter
      // after it.
      return kIsOptionalParam<typename StorageTraits<T>::StorageType> &&
             tail.Parse(it, end);
    }
    return GetValue(*it, head) && tail.Parse(it + 1, end);
  }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    tail.template Apply<H, As..., T>(handler, std::forward<As>(args)..., head);
  }

  typename StorageTraits<T>::StorageType head;
  ParamTuple<Ts...> tail;
};

template <typename... As>
bool ParseAndHandle(const base::RepeatingCallback<void(As...)>& handler,
                    const std::string& method,
                    DispatchCallback callback,
                    const base::ListValue& list) {
  ParamTuple<As...> tuple;
  if (list.size() > sizeof...(As) || !tuple.Parse(list.begin(), list.end())) {
    LOG(ERROR) << "Failed to parse arguments for " << method
               << " call: " << list.DebugString();
    return false;
  }
  tuple.Apply(handler);
  std::move(callback).Run(nullptr);
  return true;
}

template <typename... As>
bool ParseAndHandleWithCallback(
    const base::RepeatingCallback<void(DispatchCallback, As...)>& handler,
    const std::string& method,
    DispatchCallback callback,
    const base::ListValue& list) {
  ParamTuple<As...> tuple;
  if (list.size() > sizeof...(As) || !tuple.Parse(list.begin(), list.end())) {
    LOG(ERROR) << "Failed to parse arguments for " << method
               << " call: " << list.DebugString();
    return false;
  }
  tuple.Apply(handler, std::move(callback));
  return true;
}

}  // namespace

/**
 * Dispatcher for messages sent from the frontend running in an
 * isolated renderer (devtools:// or chrome://inspect) to the embedder
 * in the browser.
 *
 * The messages are sent via InspectorFrontendHost.sendMessageToEmbedder or
 * chrome.send method accordingly.
 */
class DispatcherImpl : public DevToolsEmbedderMessageDispatcher {
 public:
  enum class AllowedFrontend {
    kLocalOnly,
    kAny,
  };

  explicit DispatcherImpl(Delegate& delegate) : delegate_(delegate) {}
  ~DispatcherImpl() override = default;

  bool Dispatch(DispatchCallback callback,
                const std::string& method,
                const base::ListValue& params) override {
    auto it = handlers_.find(method);
    if (it == handlers_.end()) {
      return false;
    }
    if (it->second.allowed_frontend == AllowedFrontend::kLocalOnly &&
        !delegate_->IsLocalFrontend()) {
      std::move(callback).Run(&it->second.blocked_response);
      return true;
    }
    return it->second.handler.Run(std::move(callback), params);
  }

  template <typename... As>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(As...),
                       AllowedFrontend allowed_frontend) {
    handlers_[method] = {
        base::BindRepeating(
            &ParseAndHandle<As...>,
            base::BindRepeating(handler, base::Unretained(delegate_)), method),
        allowed_frontend};
  }

  template <typename... As>
  void RegisterHandlerWithCallback(
      const std::string& method,
      void (Delegate::*handler)(DispatchCallback, As...),
      AllowedFrontend allowed_frontend,
      base::Value blocked_response = base::Value()) {
    handlers_[method] = {
        base::BindRepeating(
            &ParseAndHandleWithCallback<As...>,
            base::BindRepeating(handler, base::Unretained(delegate_)), method),
        allowed_frontend, std::move(blocked_response)};
  }

 private:
  using Handler =
      base::RepeatingCallback<bool(DispatchCallback, const base::ListValue&)>;
  struct HandlerEntry {
    Handler handler;
    AllowedFrontend allowed_frontend = AllowedFrontend::kLocalOnly;
    base::Value blocked_response;
  };
  using HandlerMap = std::map<std::string, HandlerEntry>;
  raw_ref<Delegate> delegate_;
  HandlerMap handlers_;
};

// static
std::unique_ptr<DevToolsEmbedderMessageDispatcher>
DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(
    Delegate* delegate) {
  using AllowedFrontend = DispatcherImpl::AllowedFrontend;
  auto d = std::make_unique<DispatcherImpl>(*delegate);

  d->RegisterHandler("bringToFront", &Delegate::ActivateWindow,
                     AllowedFrontend::kAny);
  d->RegisterHandler("closeWindow", &Delegate::CloseWindow,
                     AllowedFrontend::kAny);
  d->RegisterHandler("loadCompleted", &Delegate::LoadCompleted,
                     AllowedFrontend::kAny);
  d->RegisterHandler("setInspectedPageBounds",
                     &Delegate::SetInspectedPageBounds, AllowedFrontend::kAny);
  d->RegisterHandler("inspectElementCompleted",
                     &Delegate::InspectElementCompleted, AllowedFrontend::kAny);
  d->RegisterHandler("inspectedURLChanged", &Delegate::InspectedURLChanged,
                     AllowedFrontend::kAny);
  d->RegisterHandlerWithCallback("setIsDocked", &Delegate::SetIsDocked,
                                 AllowedFrontend::kAny);
  d->RegisterHandler("openInNewTab", &Delegate::OpenInNewTab,
                     AllowedFrontend::kAny);
  d->RegisterHandler("openSearchResultsInNewTab",
                     &Delegate::OpenSearchResultsInNewTab,
                     AllowedFrontend::kAny);
  d->RegisterHandler("showItemInFolder", &Delegate::ShowItemInFolder,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("save", &Delegate::SaveToFile, AllowedFrontend::kAny);
  d->RegisterHandler("append", &Delegate::AppendToFile,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("requestFileSystems", &Delegate::RequestFileSystems,
                     AllowedFrontend::kAny);
  d->RegisterHandler("addFileSystem", &Delegate::AddFileSystem,
                     AllowedFrontend::kAny);
  d->RegisterHandler("removeFileSystem", &Delegate::RemoveFileSystem,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("upgradeDraggedFileSystemPermissions",
                     &Delegate::UpgradeDraggedFileSystemPermissions,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandlerWithCallback(
      "connectAutomaticFileSystem", &Delegate::ConnectAutomaticFileSystem,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("success", false)));
  d->RegisterHandler("disconnectAutomaticFileSystem",
                     &Delegate::DisconnectAutomaticFileSystem,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("indexPath", &Delegate::IndexPath, AllowedFrontend::kAny);
  d->RegisterHandlerWithCallback("loadNetworkResource",
                                 &Delegate::LoadNetworkResource,
                                 AllowedFrontend::kAny);
  d->RegisterHandler("stopIndexing", &Delegate::StopIndexing,
                     AllowedFrontend::kAny);
  d->RegisterHandler("searchInPath", &Delegate::SearchInPath,
                     AllowedFrontend::kAny);
  d->RegisterHandler("setWhitelistedShortcuts",
                     &Delegate::SetWhitelistedShortcuts, AllowedFrontend::kAny);
  d->RegisterHandler("setEyeDropperActive", &Delegate::SetEyeDropperActive,
                     AllowedFrontend::kAny);
  d->RegisterHandler("showCertificateViewer", &Delegate::ShowCertificateViewer,
                     AllowedFrontend::kAny);
  d->RegisterHandler("zoomIn", &Delegate::ZoomIn, AllowedFrontend::kAny);
  d->RegisterHandler("zoomOut", &Delegate::ZoomOut, AllowedFrontend::kAny);
  d->RegisterHandler("resetZoom", &Delegate::ResetZoom, AllowedFrontend::kAny);
  d->RegisterHandler("setDevicesDiscoveryConfig",
                     &Delegate::SetDevicesDiscoveryConfig,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("setDevicesUpdatesEnabled",
                     &Delegate::SetDevicesUpdatesEnabled,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("openRemotePage", &Delegate::OpenRemotePage,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("openNodeFrontend", &Delegate::OpenNodeFrontend,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("dispatchProtocolMessage",
                     &Delegate::DispatchProtocolMessageFromDevToolsFrontend,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordCountHistogram", &Delegate::RecordCountHistogram,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordEnumeratedHistogram",
                     &Delegate::RecordEnumeratedHistogram,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordPerformanceHistogram",
                     &Delegate::RecordPerformanceHistogram,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordPerformanceHistogramMedium",
                     &Delegate::RecordPerformanceHistogramMedium,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordUserMetricsAction",
                     &Delegate::RecordUserMetricsAction, AllowedFrontend::kAny);
  d->RegisterHandler("recordNewBadgeUsage", &Delegate::RecordNewBadgeUsage,
                     AllowedFrontend::kAny);
  d->RegisterHandler("setChromeFlag", &Delegate::SetChromeFlag,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("recordImpression", &Delegate::RecordImpression,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordResize", &Delegate::RecordResize,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordClick", &Delegate::RecordClick,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordHover", &Delegate::RecordHover,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordDrag", &Delegate::RecordDrag,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordChange", &Delegate::RecordChange,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordKeyDown", &Delegate::RecordKeyDown,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordSettingAccess", &Delegate::RecordSettingAccess,
                     AllowedFrontend::kAny);
  d->RegisterHandler("recordFunctionCall", &Delegate::RecordFunctionCall,
                     AllowedFrontend::kAny);
  d->RegisterHandler("registerPreference", &Delegate::RegisterPreference,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandlerWithCallback("getPreferences", &Delegate::GetPreferences,
                                 AllowedFrontend::kLocalOnly,
                                 base::Value(base::Value::Type::DICT));
  d->RegisterHandlerWithCallback("getPreference", &Delegate::GetPreference,
                                 AllowedFrontend::kLocalOnly);
  d->RegisterHandler("setPreference", &Delegate::SetPreference,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("removePreference", &Delegate::RemovePreference,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandler("clearPreferences", &Delegate::ClearPreferences,
                     AllowedFrontend::kLocalOnly);
  d->RegisterHandlerWithCallback(
      "getSyncInformation", &Delegate::GetSyncInformation,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("isSyncActive", false)));
  d->RegisterHandlerWithCallback("getHostConfig", &Delegate::GetHostConfig,
                                 AllowedFrontend::kAny);
  d->RegisterHandlerWithCallback("reattach", &Delegate::Reattach,
                                 AllowedFrontend::kAny);
  d->RegisterHandler("readyForTest", &Delegate::ReadyForTest,
                     AllowedFrontend::kAny);
  d->RegisterHandler("connectionReady", &Delegate::ConnectionReady,
                     AllowedFrontend::kAny);
  d->RegisterHandler("setOpenNewWindowForPopups",
                     &Delegate::SetOpenNewWindowForPopups,
                     AllowedFrontend::kAny);
  d->RegisterHandler("registerExtensionsAPI", &Delegate::RegisterExtensionsAPI,
                     AllowedFrontend::kAny);
  d->RegisterHandlerWithCallback("showSurvey", &Delegate::ShowSurvey,
                                 AllowedFrontend::kAny);
  d->RegisterHandlerWithCallback("canShowSurvey", &Delegate::CanShowSurvey,
                                 AllowedFrontend::kAny);

  d->RegisterHandlerWithCallback(
      "doAidaConversation", &Delegate::DoAidaConversation,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("error", "AIDA request was blocked")));
  d->RegisterHandlerWithCallback(
      "aidaCodeComplete", &Delegate::AidaCodeComplete,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("error", "AIDA request was blocked")));
  d->RegisterHandlerWithCallback(
      "registerAidaClientEvent", &Delegate::RegisterAidaClientEvent,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("error", "AIDA request was blocked")));
  d->RegisterHandlerWithCallback(
      "dispatchHttpRequest", &Delegate::DispatchHttpRequest,
      AllowedFrontend::kLocalOnly,
      base::Value(base::DictValue().Set("error", "Request validation failed")));
  d->RegisterHandler("requestRestart", &Delegate::RequestRestart,
                     AllowedFrontend::kLocalOnly);
  return d;
}
