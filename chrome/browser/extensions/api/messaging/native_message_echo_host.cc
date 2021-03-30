// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_echo_host.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

// static
// Must match ScopedTestNativeMessagingHost::kHostName.
const char* const NativeMessageEchoHost::kHostName =
    "com.google.chrome.test.echo";

// static
// Must match ScopedTestNativeMessagingHost::kExtensionId.
const char* const NativeMessageEchoHost::kOrigins[] = {
    "chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/"};

// static
const int NativeMessageEchoHost::kOriginCount = base::size(kOrigins);

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
  base::Optional<base::Value> request_value =
      base::JSONReader::Read(request_string);
  if (!request_value.has_value()) {
    client_->CloseChannel(kHostInputOutputError);
  } else if (request_string.find("stopHostTest") != std::string::npos) {
    client_->CloseChannel(kNativeHostExited);
  } else if (request_string.find("bigMessageTest") != std::string::npos) {
    client_->CloseChannel(kHostInputOutputError);
  } else {
    ProcessEcho(base::Value::AsDictionaryValue(request_value.value()));
  }
}

scoped_refptr<base::SingleThreadTaskRunner> NativeMessageEchoHost::task_runner()
    const {
  return base::ThreadTaskRunnerHandle::Get();
}

void NativeMessageEchoHost::ProcessEcho(const base::DictionaryValue& request) {
  base::DictionaryValue response;
  response.SetInteger("id", ++message_number_);
  response.Set("echo", request.CreateDeepCopy());
  response.SetString("caller_url", kOrigins[0]);
  std::string response_string;
  base::JSONWriter::Write(response, &response_string);
  client_->PostMessageFromNativeHost(response_string);
}

}  // namespace extensions
