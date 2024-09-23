// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ECHO_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ECHO_HOST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace {
class BrowserContext;
}

namespace extensions {

// A test NativeMessageHost used in ExtensionApiTest::NativeMessagingBasic.
// See //chrome/browser/extensions/api/messaging/native_messaging_apitest.cc
// The behavior in this implementation must match the expectations defined in
// //chrome/test/data/native_messaging/native_hosts/echo.py as that script is
// used to drive the tests.
class NativeMessageEchoHost : public NativeMessageHost {
 public:
  static const char* const kHostName;
  static const char* const kOrigins[];
  static const size_t kOriginCount;

  static std::unique_ptr<NativeMessageHost> Create(
      content::BrowserContext* browser_context);

  NativeMessageEchoHost();
  NativeMessageEchoHost(const NativeMessageEchoHost&) = delete;
  NativeMessageEchoHost& operator=(const NativeMessageEchoHost&) = delete;
  ~NativeMessageEchoHost() override;

  // NativeMessageHost implementation.
  void Start(Client* client) override;
  void OnMessage(const std::string& request_string) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  void ProcessEcho(const base::Value::Dict& request);
  void SendInvalidResponse();

  // Counter used to ensure message uniqueness for testing.
  int message_number_ = 0;

  // |client_| must outlive this test instance.
  raw_ptr<Client> client_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ECHO_HOST_H_
