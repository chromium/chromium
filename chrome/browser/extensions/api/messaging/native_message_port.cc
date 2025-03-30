// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_port.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

// Handles jumping between the |host_task_runner| and the
// |message_service_task_runner|.
// All methods on the host interface should be called on |host_task_runner|.
// All methods on |port| (that calls into MessageServices) should be called
// on |message_service_task_runner|.
class NativeMessagePortDispatcher : public NativeMessagePort::Dispatcher,
                                    public NativeMessageHost::Client {
 public:
  NativeMessagePortDispatcher(
      std::unique_ptr<NativeMessageHost> host,
      base::WeakPtr<NativeMessagePort> port,
      scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner_);
  ~NativeMessagePortDispatcher() override;

  void DispatchOnMessage(const std::string& message) override;

  // NativeMessageHost::Client implementation.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

 private:
  std::unique_ptr<NativeMessageHost> host_;
  base::WeakPtr<NativeMessagePort> port_;

  scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> host_task_runner_;
};

NativeMessagePortDispatcher::NativeMessagePortDispatcher(
    std::unique_ptr<NativeMessageHost> host,
    base::WeakPtr<NativeMessagePort> port,
    scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner)
    : host_(std::move(host)),
      port_(port),
      message_service_task_runner_(message_service_task_runner),
      host_task_runner_(host_->task_runner()) {
  DCHECK(message_service_task_runner_->BelongsToCurrentThread());
  host_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMessageHost::Start, base::Unretained(host_.get()),
                     base::Unretained(this)));
}

NativeMessagePortDispatcher::~NativeMessagePortDispatcher() {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
}

void NativeMessagePortDispatcher::DispatchOnMessage(
    const std::string& message) {
  DCHECK(message_service_task_runner_->BelongsToCurrentThread());
  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessageHost::OnMessage,
                                base::Unretained(host_.get()), message));
}

void NativeMessagePortDispatcher::PostMessageFromNativeHost(
    const std::string& message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessagePort::PostMessageFromNativeHost,
                                port_, message));
}

void NativeMessagePortDispatcher::CloseChannel(
    const std::string& error_message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMessagePort::CloseChannel, port_, error_message));
}

NativeMessagePort::NativeMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    std::unique_ptr<NativeMessageHost> native_message_host)
    : MessagePort(std::move(channel_delegate), port_id),
      host_task_runner_(native_message_host->task_runner()) {
  dispatcher_ = std::make_unique<NativeMessagePortDispatcher>(
      std::move(native_message_host), weak_factory_.GetWeakPtr(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

NativeMessagePort::~NativeMessagePort() {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_task_runner_->DeleteSoon(FROM_HERE, dispatcher_.release());
}

bool NativeMessagePort::IsValidPort() {
  // The native message port is immediately connected after construction, so it
  // is not possible to invalidate the port between construction and connection.
  return true;
}

void NativeMessagePort::DispatchOnMessage(const Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  dispatcher_->DispatchOnMessage(message.data);
}

void NativeMessagePort::PostMessageFromNativeHost(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    // Native messaging always uses JSON since a native host doesn't understand
    // structured cloning serialization.
    weak_channel_delegate_->PostMessage(
        port_id_, Message(message, mojom::SerializationFormat::kJson,
                          false /* user_gesture */));
  }
}

void NativeMessagePort::CloseChannel(const std::string& error_message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    weak_channel_delegate_->CloseChannel(port_id_, error_message);
  }
}

}  // namespace extensions
