// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_session.h"

#include <memory>
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/devtools/protocol/cast_handler.h"
#include "chrome/browser/devtools/protocol/page_handler.h"
#include "chrome/browser/devtools/protocol/security_handler.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "third_party/inspector_protocol/crdtp/json.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/devtools/protocol/window_manager_handler.h"
#endif

namespace {
// TODO(johannes): This is very similar to the code in
// content/browser/devtools/devtools_protocol_encoding. Once we have
// the error / status propagation story settled, move the common parts
// into a content public API.

// Platform allows us to inject the string<->double conversion
// routines from base:: into the inspector_protocol JSON parser / serializer.
class Platform : public crdtp::json::Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    return base::StringToDouble(str, result);
  }

  // Prints |value| in a format suitable for JSON.
  std::unique_ptr<char[]> DToStr(double value) const override {
    std::string str = base::NumberToString(value);
    std::unique_ptr<char[]> result(new char[str.size() + 1]);
    memcpy(result.get(), str.c_str(), str.size() + 1);
    return result;
  }
};

crdtp::Status ConvertCBORToJSON(crdtp::span<uint8_t> cbor, std::string* json) {
  Platform platform;
  return ConvertCBORToJSON(platform, cbor, json);
}
}  // namespace

ChromeDevToolsSession::ChromeDevToolsSession(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client)
    : agent_host_(agent_host),
      client_(client),
      dispatcher_(std::make_unique<protocol::UberDispatcher>(this)) {
  if (agent_host->GetWebContents() &&
      agent_host->GetType() == content::DevToolsAgentHost::kTypePage) {
    page_handler_ = std::make_unique<PageHandler>(agent_host->GetWebContents(),
                                                  dispatcher_.get());
    security_handler_ = std::make_unique<SecurityHandler>(
        agent_host->GetWebContents(), dispatcher_.get());
    if (client->MayAttachToBrowser()) {
      cast_handler_ = std::make_unique<CastHandler>(
          agent_host->GetWebContents(), dispatcher_.get());
    }
  }
  target_handler_ = std::make_unique<TargetHandler>(dispatcher_.get());
  if (client->MayAttachToBrowser()) {
    browser_handler_ = std::make_unique<BrowserHandler>(dispatcher_.get(),
                                                        agent_host->GetId());
  }
#if defined(OS_CHROMEOS)
  window_manager_protocl_handler_ =
      std::make_unique<WindowManagerHandler>(dispatcher_.get());
#endif
}

ChromeDevToolsSession::~ChromeDevToolsSession() = default;

void ChromeDevToolsSession::HandleCommand(
    const std::string& method,
    const std::string& message,
    content::DevToolsManagerDelegate::NotHandledCallback callback) {
  if (!dispatcher_->canDispatch(method)) {
    std::move(callback).Run(message);
    return;
  }

  int call_id;
  std::string unused;
  std::unique_ptr<protocol::DictionaryValue> value =
      protocol::DictionaryValue::cast(
          protocol::StringUtil::parseMessage(message, /*binary=*/true));
  if (!dispatcher_->parseCommand(value.get(), &call_id, &unused))
    return;
  pending_commands_[call_id] = std::move(callback);
  dispatcher_->dispatch(call_id, method, std::move(value), message);
}

// The following methods handle responses or notifications coming from
// the browser to the client.
static void SendProtocolResponseOrNotification(
    content::DevToolsAgentHostClient* client,
    content::DevToolsAgentHost* agent_host,
    std::unique_ptr<protocol::Serializable> message) {
  std::vector<uint8_t> cbor = std::move(*message).TakeSerialized();
  if (client->UsesBinaryProtocol()) {
    client->DispatchProtocolMessage(agent_host,
                                    std::string(cbor.begin(), cbor.end()));
    return;
  }
  std::string json;
  crdtp::Status status = ConvertCBORToJSON(crdtp::SpanFrom(cbor), &json);
  LOG_IF(ERROR, !status.ok()) << status.ToASCIIString();
  client->DispatchProtocolMessage(agent_host, json);
}

void ChromeDevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  pending_commands_.erase(call_id);
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void ChromeDevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponseOrNotification(client_, agent_host_, std::move(message));
}

void ChromeDevToolsSession::flushProtocolNotifications() {}

void ChromeDevToolsSession::fallThrough(int call_id,
                                        const std::string& method,
                                        const std::string& message) {
  auto callback = std::move(pending_commands_[call_id]);
  pending_commands_.erase(call_id);
  std::move(callback).Run(message);
}
