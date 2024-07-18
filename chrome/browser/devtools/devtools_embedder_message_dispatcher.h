// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

struct RegisterOptions;
struct ImpressionEvent;
struct ResizeEvent;
struct ClickEvent;
struct HoverEvent;
struct DragEvent;
struct ChangeEvent;
struct KeyDownEvent;

/**
 * Dispatcher for messages sent from the DevTools frontend running in an
 * isolated renderer (on devtools://) to the embedder in the browser.
 *
 * The messages are sent via InspectorFrontendHost.sendMessageToEmbedder method.
 */
class DevToolsEmbedderMessageDispatcher {
 public:
  class Delegate {
   public:
    using DispatchCallback = base::OnceCallback<void(const base::Value*)>;

    virtual ~Delegate() = default;

    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void LoadCompleted() = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void InspectedURLChanged(const std::string& url) = 0;
    virtual void SetIsDocked(DispatchCallback callback, bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void OpenSearchResultsInNewTab(const std::string& query) = 0;
    virtual void ShowItemInFolder(const std::string& file_system_path) = 0;
    virtual void SaveToFile(const std::string& url,
                            const std::string& content,
                            bool save_as,
                            bool is_base64) = 0;
    virtual void AppendToFile(const std::string& url,
                              const std::string& content) = 0;
    virtual void RequestFileSystems() = 0;
    virtual void AddFileSystem(const std::string& type) = 0;
    virtual void RemoveFileSystem(const std::string& file_system_path) = 0;
    virtual void UpgradeDraggedFileSystemPermissions(
        const std::string& file_system_url) = 0;
    virtual void IndexPath(int index_request_id,
                           const std::string& file_system_path,
                           const std::string& excluded_folders) = 0;
    virtual void StopIndexing(int index_request_id) = 0;
    virtual void LoadNetworkResource(DispatchCallback callback,
                                     const std::string& url,
                                     const std::string& headers,
                                     int stream_id) = 0;
    virtual void SearchInPath(int search_request_id,
                              const std::string& file_system_path,
                              const std::string& query) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;
    virtual void SetEyeDropperActive(bool active) = 0;
    virtual void ShowCertificateViewer(const std::string& cert_chain) = 0;
    virtual void ZoomIn() = 0;
    virtual void ZoomOut() = 0;
    virtual void ResetZoom() = 0;
    virtual void SetDevicesUpdatesEnabled(bool enabled) = 0;
    virtual void SetDevicesDiscoveryConfig(
        bool discover_usb_devices,
        bool port_forwarding_enabled,
        const std::string& port_forwarding_config,
        bool network_discovery_enabled,
        const std::string& network_discovery_config) = 0;
    virtual void OpenRemotePage(const std::string& browser_id,
                                const std::string& url) = 0;
    virtual void OpenNodeFrontend() = 0;
    virtual void RegisterPreference(const std::string& name,
                                    const RegisterOptions& options) = 0;
    virtual void GetPreferences(DispatchCallback callback) = 0;
    virtual void GetPreference(DispatchCallback callback,
                               const std::string& name) = 0;
    virtual void SetPreference(const std::string& name,
                               const std::string& value) = 0;
    virtual void RemovePreference(const std::string& name) = 0;
    virtual void ClearPreferences() = 0;
    virtual void GetSyncInformation(DispatchCallback callback) = 0;
    virtual void GetHostConfig(DispatchCallback callback) = 0;
    virtual void DispatchProtocolMessageFromDevToolsFrontend(
        const std::string& message) = 0;
    virtual void RecordCountHistogram(const std::string& name,
                                      int sample,
                                      int min,
                                      int exclusive_max,
                                      int buckets) = 0;
    virtual void RecordEnumeratedHistogram(const std::string& name,
                                           int sample,
                                           int boundary_value) = 0;
    virtual void RecordPerformanceHistogram(const std::string& name,
                                            double duration) = 0;
    virtual void RecordUserMetricsAction(const std::string& name) = 0;
    virtual void RecordImpression(const ImpressionEvent& event) = 0;
    virtual void RecordResize(const ResizeEvent& event) = 0;
    virtual void RecordClick(const ClickEvent& event) = 0;
    virtual void RecordHover(const HoverEvent& event) = 0;
    virtual void RecordDrag(const DragEvent& event) = 0;
    virtual void RecordChange(const ChangeEvent& event) = 0;
    virtual void RecordKeyDown(const KeyDownEvent& event) = 0;
    virtual void SendJsonRequest(DispatchCallback callback,
                                 const std::string& browser_id,
                                 const std::string& url) = 0;
    virtual void Reattach(DispatchCallback callback) = 0;
    virtual void ReadyForTest() = 0;
    virtual void ConnectionReady() = 0;
    virtual void SetOpenNewWindowForPopups(bool value) = 0;
    virtual void RegisterExtensionsAPI(const std::string& origin,
                                       const std::string& script) = 0;
    virtual void ShowSurvey(DispatchCallback callback,
                            const std::string& trigger) = 0;
    virtual void CanShowSurvey(DispatchCallback callback,
                               const std::string& trigger) = 0;
    virtual void DoAidaConversation(DispatchCallback callback,
                                    const std::string& request,
                                    int stream_id) = 0;
    virtual void RegisterAidaClientEvent(DispatchCallback callback,
                                         const std::string& request) = 0;
  };

  using DispatchCallback = Delegate::DispatchCallback;

  virtual ~DevToolsEmbedderMessageDispatcher() = default;
  virtual bool Dispatch(DispatchCallback callback,
                        const std::string& method,
                        const base::Value::List& params) = 0;

  static std::unique_ptr<DevToolsEmbedderMessageDispatcher>
  CreateForDevToolsFrontend(Delegate* delegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
