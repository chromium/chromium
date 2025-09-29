// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_echo_host.h"

#include <optional>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

// static
std::unique_ptr<NativeMessageHost> NativeMessageEchoHost::Create(
    content::BrowserContext* browser_context) {
  return std::make_unique<NativeMessageEchoHost>();
}

NativeMessageEchoHost::NativeMessageEchoHost() = default;
NativeMessageEchoHost::~NativeMessageEchoHost() = default;

void NativeMessageEchoHost::Start(Client* client) {
  client_ = client;
}

void NativeMessageEchoHost::OnMessage(const std::string& request_string) {
  std::optional<base::Value> request_value = base::JSONReader::Read(
      request_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!request_value.has_value()) {
    client_->CloseChannel(kHostInputOutputError);
  } else if (request_string.find("stopHostTest") != std::string::npos) {
    client_->CloseChannel(kNativeHostExited);
  } else if (request_string.find("bigMessageTest") != std::string::npos) {
    client_->CloseChannel(kHostInputOutputError);
  } else if (request_string.find("sendInvalidResponse") != std::string::npos) {
    SendInvalidResponse();
  } else {
    ProcessEcho(request_value->GetDict());
  }
}

scoped_refptr<base::SingleThreadTaskRunner> NativeMessageEchoHost::task_runner()
    const {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

void NativeMessageEchoHost::ProcessEcho(const base::Value::Dict& request) {
  base::Value::Dict response;
  response.Set("id", ++message_number_);
  response.Set("echo", request.Clone());
  response.Set("caller_url", kOrigins[0]);
  client_->PostMessageFromNativeHost(base::WriteJson(response).value_or(""));
}

void NativeMessageEchoHost::SendInvalidResponse() {
  // Send a malformed JSON string.
  client_->PostMessageFromNativeHost("{");
}

}  // namespace extensions
