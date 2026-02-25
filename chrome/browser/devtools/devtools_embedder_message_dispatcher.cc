// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
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

template <typename... Ts>
struct ParamTuple {
  bool Parse(base::ListValue::const_iterator it) { return true; }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    handler.Run(std::forward<As>(args)...);
  }
};

template <typename T, typename... Ts>
struct ParamTuple<T, Ts...> {
  bool Parse(base::ListValue::const_iterator it) {
    return GetValue(*it, head) && tail.Parse(it + 1);
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
  if (list.size() != sizeof...(As) || !tuple.Parse(list.begin())) {
    LOG(ERROR) << "Failed to parse arguments for " << method
               << " call: " << list.DebugString();
    return false;
  }
  tuple.Apply(handler);
  return true;
}

template <typename... As>
bool ParseAndHandleWithCallback(
    const base::RepeatingCallback<void(DispatchCallback, As...)>& handler,
    const std::string& method,
    DispatchCallback callback,
    const base::ListValue& list) {
  ParamTuple<As...> tuple;
  if (list.size() != sizeof...(As) || !tuple.Parse(list.begin())) {
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
  ~DispatcherImpl() override = default;

  bool Dispatch(DispatchCallback callback,
                const std::string& method,
                const base::ListValue& params) override {
    auto it = handlers_.find(method);
    return it != handlers_.end() && it->second.Run(std::move(callback), params);
  }

  template<typename... As>
  void RegisterHandler(const std::string& method,
                       void (Delegate::*handler)(As...),
                       Delegate* delegate) {
    handlers_[method] = base::BindRepeating(
        &ParseAndHandle<As...>,
        base::BindRepeating(handler, base::Unretained(delegate)), method);
  }

  template <typename... As>
  void RegisterHandlerWithCallback(const std::string& method,
                                   void (Delegate::*handler)(DispatchCallback,
                                                             As...),
                                   Delegate* delegate) {
    handlers_[method] = base::BindRepeating(
        &ParseAndHandleWithCallback<As...>,
        base::BindRepeating(handler, base::Unretained(delegate)), method);
  }

 private:
  using Handler =
      base::RepeatingCallback<bool(DispatchCallback, const base::ListValue&)>;
  using HandlerMap = std::map<std::string, Handler>;
  HandlerMap handlers_;
};

// static
std::unique_ptr<DevToolsEmbedderMessageDispatcher>
DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(
    Delegate* delegate) {
  auto d = std::make_unique<DispatcherImpl>();

  d->RegisterHandler("bringToFront", &Delegate::ActivateWindow, delegate);
  d->RegisterHandler("closeWindow", &Delegate::CloseWindow, delegate);
  d->RegisterHandler("loadCompleted", &Delegate::LoadCompleted, delegate);
  d->RegisterHandler("setInspectedPageBounds",
                     &Delegate::SetInspectedPageBounds, delegate);
  d->RegisterHandler("inspectElementCompleted",
                     &Delegate::InspectElementCompleted, delegate);
  d->RegisterHandler("inspectedURLChanged",
                     &Delegate::InspectedURLChanged, delegate);
  d->RegisterHandlerWithCallback("setIsDocked",
                                 &Delegate::SetIsDocked, delegate);
  d->RegisterHandler("openInNewTab", &Delegate::OpenInNewTab, delegate);
  d->RegisterHandler("openSearchResultsInNewTab",
                     &Delegate::OpenSearchResultsInNewTab, delegate);
  d->RegisterHandler("showItemInFolder", &Delegate::ShowItemInFolder, delegate);
  d->RegisterHandler("save", &Delegate::SaveToFile, delegate);
  d->RegisterHandler("append", &Delegate::AppendToFile, delegate);
  d->RegisterHandler("requestFileSystems",
                     &Delegate::RequestFileSystems, delegate);
  d->RegisterHandler("addFileSystem", &Delegate::AddFileSystem, delegate);
  d->RegisterHandler("removeFileSystem", &Delegate::RemoveFileSystem, delegate);
  d->RegisterHandler("upgradeDraggedFileSystemPermissions",
                     &Delegate::UpgradeDraggedFileSystemPermissions, delegate);
  d->RegisterHandlerWithCallback("connectAutomaticFileSystem",
                                 &Delegate::ConnectAutomaticFileSystem,
                                 delegate);
  d->RegisterHandler("disconnectAutomaticFileSystem",
                     &Delegate::DisconnectAutomaticFileSystem, delegate);
  d->RegisterHandler("indexPath", &Delegate::IndexPath, delegate);
  d->RegisterHandlerWithCallback("loadNetworkResource",
                                 &Delegate::LoadNetworkResource, delegate);
  d->RegisterHandler("stopIndexing", &Delegate::StopIndexing, delegate);
  d->RegisterHandler("searchInPath", &Delegate::SearchInPath, delegate);
  d->RegisterHandler("setWhitelistedShortcuts",
                     &Delegate::SetWhitelistedShortcuts, delegate);
  d->RegisterHandler("setEyeDropperActive", &Delegate::SetEyeDropperActive,
                     delegate);
  d->RegisterHandler("showCertificateViewer",
                     &Delegate::ShowCertificateViewer, delegate);
  d->RegisterHandler("zoomIn", &Delegate::ZoomIn, delegate);
  d->RegisterHandler("zoomOut", &Delegate::ZoomOut, delegate);
  d->RegisterHandler("resetZoom", &Delegate::ResetZoom, delegate);
  d->RegisterHandler("setDevicesDiscoveryConfig",
                     &Delegate::SetDevicesDiscoveryConfig, delegate);
  d->RegisterHandler("setDevicesUpdatesEnabled",
                     &Delegate::SetDevicesUpdatesEnabled, delegate);
  d->RegisterHandler("openRemotePage", &Delegate::OpenRemotePage, delegate);
  d->RegisterHandler("openNodeFrontend", &Delegate::OpenNodeFrontend, delegate);
  d->RegisterHandler("dispatchProtocolMessage",
                     &Delegate::DispatchProtocolMessageFromDevToolsFrontend,
                     delegate);
  d->RegisterHandler("recordCountHistogram", &Delegate::RecordCountHistogram,
                     delegate);
  d->RegisterHandler("recordEnumeratedHistogram",
                     &Delegate::RecordEnumeratedHistogram, delegate);
  d->RegisterHandler("recordPerformanceHistogram",
                     &Delegate::RecordPerformanceHistogram, delegate);
  d->RegisterHandler("recordPerformanceHistogramMedium",
                     &Delegate::RecordPerformanceHistogramMedium, delegate);
  d->RegisterHandler("recordUserMetricsAction",
                     &Delegate::RecordUserMetricsAction, delegate);
  d->RegisterHandler("recordNewBadgeUsage", &Delegate::RecordNewBadgeUsage,
                     delegate);
  d->RegisterHandler("setChromeFlag", &Delegate::SetChromeFlag, delegate);
  d->RegisterHandler("recordImpression", &Delegate::RecordImpression, delegate);
  d->RegisterHandler("recordResize", &Delegate::RecordResize, delegate);
  d->RegisterHandler("recordClick", &Delegate::RecordClick, delegate);
  d->RegisterHandler("recordHover", &Delegate::RecordHover, delegate);
  d->RegisterHandler("recordDrag", &Delegate::RecordDrag, delegate);
  d->RegisterHandler("recordChange", &Delegate::RecordChange, delegate);
  d->RegisterHandler("recordKeyDown", &Delegate::RecordKeyDown, delegate);
  d->RegisterHandler("recordSettingAccess", &Delegate::RecordSettingAccess,
                     delegate);
  d->RegisterHandler("recordFunctionCall", &Delegate::RecordFunctionCall,
                     delegate);
  d->RegisterHandler("registerPreference", &Delegate::RegisterPreference,
                     delegate);
  d->RegisterHandlerWithCallback("getPreferences",
                                 &Delegate::GetPreferences, delegate);
  d->RegisterHandlerWithCallback("getPreference", &Delegate::GetPreference,
                                 delegate);
  d->RegisterHandler("setPreference",
                     &Delegate::SetPreference, delegate);
  d->RegisterHandler("removePreference",
                     &Delegate::RemovePreference, delegate);
  d->RegisterHandler("clearPreferences",
                     &Delegate::ClearPreferences, delegate);
  d->RegisterHandlerWithCallback("getSyncInformation",
                                 &Delegate::GetSyncInformation, delegate);
  d->RegisterHandlerWithCallback("getHostConfig", &Delegate::GetHostConfig,
                                 delegate);
  d->RegisterHandlerWithCallback("reattach",
                                 &Delegate::Reattach, delegate);
  d->RegisterHandler("readyForTest",
                     &Delegate::ReadyForTest, delegate);
  d->RegisterHandler("connectionReady", &Delegate::ConnectionReady, delegate);
  d->RegisterHandler("setOpenNewWindowForPopups",
                     &Delegate::SetOpenNewWindowForPopups, delegate);
  d->RegisterHandler("registerExtensionsAPI", &Delegate::RegisterExtensionsAPI,
                     delegate);
  d->RegisterHandlerWithCallback("showSurvey", &Delegate::ShowSurvey, delegate);
  d->RegisterHandlerWithCallback("canShowSurvey", &Delegate::CanShowSurvey,
                                 delegate);

  d->RegisterHandlerWithCallback("doAidaConversation",
                                 &Delegate::DoAidaConversation, delegate);
  d->RegisterHandlerWithCallback("aidaCodeComplete",
                                 &Delegate::AidaCodeComplete, delegate);
  d->RegisterHandlerWithCallback("registerAidaClientEvent",
                                 &Delegate::RegisterAidaClientEvent, delegate);
  d->RegisterHandlerWithCallback("dispatchHttpRequest",
                                 &Delegate::DispatchHttpRequest, delegate);
  d->RegisterHandler("requestRestart", &Delegate::RequestRestart, delegate);
  return d;
}
