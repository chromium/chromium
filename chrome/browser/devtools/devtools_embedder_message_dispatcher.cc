// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_settings.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/visual_logging.h"

namespace {

using DispatchCallback = DevToolsEmbedderMessageDispatcher::DispatchCallback;

bool GetValue(const base::Value& value, std::string* result) {
  if (result && value.is_string()) {
    *result = value.GetString();
    return true;
  }
  return value.is_string();
}

bool GetValue(const base::Value& value, int* result) {
  if (result && value.is_int()) {
    *result = value.GetInt();
    return true;
  }
  return value.is_int();
}

bool GetValue(const base::Value& value, double* result) {
  if (result && (value.is_double() || value.is_int())) {
    *result = value.GetDouble();
    return true;
  }
  return value.is_double() || value.is_int();
}

bool GetValue(const base::Value& value, bool* result) {
  if (result && value.is_bool()) {
    *result = value.GetBool();
    return true;
  }
  return value.is_bool();
}

bool GetValue(const base::Value& value, gfx::Rect* rect) {
  if (!value.is_dict())
    return false;
  const base::Value::Dict& dict = value.GetDict();
  std::optional<int> x = dict.FindInt("x");
  std::optional<int> y = dict.FindInt("y");
  std::optional<int> width = dict.FindInt("width");
  std::optional<int> height = dict.FindInt("height");
  if (!x.has_value() || !y.has_value() || !width.has_value() ||
      !height.has_value()) {
    return false;
  }

  rect->SetRect(x.value(), y.value(), width.value(), height.value());
  return true;
}

bool GetValue(const base::Value& value, RegisterOptions* options) {
  if (!value.is_dict())
    return false;

  const bool synced = value.GetDict().FindBool("synced").value_or(false);
  options->sync_mode = synced ? RegisterOptions::SyncMode::kSync
                              : RegisterOptions::SyncMode::kDontSync;
  return true;
}

bool GetValue(const base::Value& value, ImpressionEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  const base::Value::List* impressions =
      value.GetDict().FindList("impressions");
  if (!impressions) {
    return false;
  }
  for (const auto& impression : *impressions) {
    if (!impression.is_dict()) {
      return false;
    }
    std::optional<double> id = impression.GetDict().FindDouble("id");
    std::optional<int> type = impression.GetDict().FindInt("type");
    if (!id || !type) {
      return false;
    }
    event->impressions.emplace_back(
        VisualElementImpression{static_cast<int64_t>(*id), *type});

    std::optional<double> parent = impression.GetDict().FindDouble("parent");
    if (parent) {
      event->impressions.back().parent = *parent;
    }
    std::optional<int> context = impression.GetDict().FindInt("context");
    if (context) {
      event->impressions.back().context = *context;
    }
    std::optional<int> width = impression.GetDict().FindInt("width");
    if (width) {
      event->impressions.back().width = *width;
    }
    std::optional<int> height = impression.GetDict().FindInt("height");
    if (height) {
      event->impressions.back().height = *height;
    }
  }
  return true;
}

bool GetValue(const base::Value& value, ResizeEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (!veid) {
    return false;
  }
  event->veid = *veid;

  std::optional<int> width = value.GetDict().FindInt("width");
  if (width) {
    event->width = *width;
  }
  std::optional<int> height = value.GetDict().FindInt("height");
  if (height) {
    event->height = *height;
  }
  return true;
}

bool GetValue(const base::Value& value, ClickEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (!veid) {
    return false;
  }
  event->veid = *veid;

  std::optional<int> mouse_button = value.GetDict().FindInt("mouseButton");
  if (mouse_button) {
    event->mouse_button = *mouse_button;
  }
  std::optional<int> double_click = value.GetDict().FindInt("doubleClick");
  if (double_click) {
    event->double_click = *double_click;
  }
  std::optional<int> context = value.GetDict().FindInt("context");
  if (context) {
    event->context = *context;
  }
  return true;
}

bool GetValue(const base::Value& value, HoverEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (!veid) {
    return false;
  }
  event->veid = *veid;

  std::optional<int> time = value.GetDict().FindInt("time");
  if (time) {
    event->time = *time;
  }
  std::optional<int> context = value.GetDict().FindInt("context");
  if (context) {
    event->context = *context;
  }
  return true;
}

bool GetValue(const base::Value& value, DragEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (!veid) {
    return false;
  }
  event->veid = *veid;

  std::optional<int> distance = value.GetDict().FindInt("distance");
  if (distance) {
    event->distance = *distance;
  }
  std::optional<int> context = value.GetDict().FindInt("context");
  if (context) {
    event->context = *context;
  }
  return true;
}

bool GetValue(const base::Value& value, ChangeEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (!veid) {
    return false;
  }
  event->veid = *veid;

  std::optional<int> context = value.GetDict().FindInt("context");
  if (context) {
    event->context = *context;
  }
  return true;
}

bool GetValue(const base::Value& value, KeyDownEvent* event) {
  if (!value.is_dict()) {
    return false;
  }

  std::optional<double> veid = value.GetDict().FindDouble("veid");
  if (veid) {
    event->veid = *veid;
  }

  std::optional<int> context = value.GetDict().FindInt("context");
  if (context) {
    event->context = *context;
  }
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
  bool Parse(const base::Value::List& list,
             const base::Value::List::const_iterator& it) {
    return it == list.end();
  }

  template <typename H, typename... As>
  void Apply(const H& handler, As... args) {
    handler.Run(std::forward<As>(args)...);
  }
};

template <typename T, typename... Ts>
struct ParamTuple<T, Ts...> {
  bool Parse(const base::Value::List& list,
             const base::Value::List::const_iterator& it) {
    return it != list.end() && GetValue(*it, &head) && tail.Parse(list, it + 1);
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
                    const base::Value::List& list) {
  ParamTuple<As...> tuple;
  if (!tuple.Parse(list, list.begin())) {
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
    const base::Value::List& list) {
  ParamTuple<As...> tuple;
  if (!tuple.Parse(list, list.begin())) {
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
                const base::Value::List& params) override {
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
      base::RepeatingCallback<bool(DispatchCallback, const base::Value::List&)>;
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
  d->RegisterHandler("recordUserMetricsAction",
                     &Delegate::RecordUserMetricsAction, delegate);
  d->RegisterHandler("recordImpression", &Delegate::RecordImpression, delegate);
  d->RegisterHandler("recordResize", &Delegate::RecordResize, delegate);
  d->RegisterHandler("recordClick", &Delegate::RecordClick, delegate);
  d->RegisterHandler("recordHover", &Delegate::RecordHover, delegate);
  d->RegisterHandler("recordDrag", &Delegate::RecordDrag, delegate);
  d->RegisterHandler("recordChange", &Delegate::RecordChange, delegate);
  d->RegisterHandler("recordKeyDown", &Delegate::RecordKeyDown, delegate);
  d->RegisterHandlerWithCallback("sendJsonRequest",
                                 &Delegate::SendJsonRequest, delegate);
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
  if (base::FeatureList::IsEnabled(::features::kDevToolsConsoleInsights)) {
    d->RegisterHandlerWithCallback("doAidaConversation",
                                   &Delegate::DoAidaConversation, delegate);
    d->RegisterHandlerWithCallback("registerAidaClientEvent",
                                   &Delegate::RegisterAidaClientEvent,
                                   delegate);
  }
  return d;
}
