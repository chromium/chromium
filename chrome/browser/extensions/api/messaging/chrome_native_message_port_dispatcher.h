// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_NATIVE_MESSAGE_PORT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_NATIVE_MESSAGE_PORT_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/api/messaging/native_message_port_dispatcher.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class SingleThreadTaskRunner;
}

namespace extensions {
class NativeMessagePort;

// Handles jumping between the `host_task_runner` and the
// `message_service_task_runner`.
// All methods on the host interface should be called on `host_task_runner`.
// All methods on `port` (that calls into MessageServices) should be called
// on `message_service_task_runner`.
class ChromeNativeMessagePortDispatcher : public NativeMessagePortDispatcher,
                                          public NativeMessageHost::Client {
 public:
  ChromeNativeMessagePortDispatcher(
      std::unique_ptr<NativeMessageHost> host,
      base::WeakPtr<NativeMessagePort> port,
      scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner);
  ~ChromeNativeMessagePortDispatcher() override;

  // NativeMessagePort::Dispatcher
  void DispatchOnMessage(const std::string& message) override;

  // NativeMessageHost::Client
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

 private:
  std::unique_ptr<NativeMessageHost> host_;
  base::WeakPtr<NativeMessagePort> port_;

  scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> host_task_runner_;
};

}  // namespace extensions.

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_NATIVE_MESSAGE_PORT_DISPATCHER_H_
