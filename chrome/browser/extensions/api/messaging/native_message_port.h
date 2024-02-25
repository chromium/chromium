// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/api/messaging/message_port.h"
#include "extensions/common/api/messaging/port_id.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace extensions {
class NativeMessageHost;

// A port that manages communication with a native application.
// All methods must be called on the UI Thread of the browser process.
class NativeMessagePort : public MessagePort {
 public:
  NativeMessagePort(base::WeakPtr<ChannelDelegate> channel_delegate,
                    const PortId& port_id,
                    std::unique_ptr<NativeMessageHost> native_message_host);
  ~NativeMessagePort() override;

  // MessagePort implementation.
  bool IsValidPort() override;
  void DispatchOnMessage(const Message& message) override;

 private:
  class Core;
  void PostMessageFromNativeHost(const std::string& message);
  void CloseChannel(const std::string& error_message);

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::SingleThreadTaskRunner> host_task_runner_;
  std::unique_ptr<Core> core_;

  base::WeakPtrFactory<NativeMessagePort> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PORT_H_
