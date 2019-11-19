// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_port.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/messaging/message.h"

namespace extensions {

// Handles jumping between the |host_task_runner| and the
// |message_service_task_runner|.
// All methods on the host interface should be called on |host_task_runner|.
// All methods on |port| (that calls into MessageServices) should be called
// on |message_service_task_runner|.
class NativeMessagePort::Core : public NativeMessageHost::Client {
 public:
  Core(
      std::unique_ptr<NativeMessageHost> host,
      base::WeakPtr<NativeMessagePort> port,
      scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner_);
  ~Core() override;

  void OnMessageFromChrome(const std::string& message);

  // NativeMessageHost::Client implementation.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

 private:
  std::unique_ptr<NativeMessageHost> host_;
  base::WeakPtr<NativeMessagePort> port_;

  scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> host_task_runner_;
};

NativeMessagePort::Core::Core(
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

NativeMessagePort::Core::~Core() {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
}

void NativeMessagePort::Core::OnMessageFromChrome(const std::string& message) {
  DCHECK(message_service_task_runner_->BelongsToCurrentThread());
  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessageHost::OnMessage,
                                base::Unretained(host_.get()), message));
}

void NativeMessagePort::Core::PostMessageFromNativeHost(
    const std::string& message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessagePort::PostMessageFromNativeHost,
                                port_, message));
}

void NativeMessagePort::Core::CloseChannel(const std::string& error_message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMessagePort::CloseChannel, port_, error_message));
}

NativeMessagePort::NativeMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    std::unique_ptr<NativeMessageHost> native_message_host)
    : weak_channel_delegate_(channel_delegate),
      host_task_runner_(native_message_host->task_runner()),
      port_id_(port_id) {
  core_.reset(new Core(std::move(native_message_host),
                       weak_factory_.GetWeakPtr(),
                       base::ThreadTaskRunnerHandle::Get()));
}

NativeMessagePort::~NativeMessagePort() {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

bool NativeMessagePort::IsValidPort() {
  // The native message port is immediately connected after construction, so it
  // is not possible to invalidate the port between construction and connection.
  return true;
}

void NativeMessagePort::DispatchOnMessage(const Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  core_->OnMessageFromChrome(message.data);
}

void NativeMessagePort::PostMessageFromNativeHost(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    weak_channel_delegate_->PostMessage(
        port_id_, Message(message, false /* user_gesture */));
  }
}

void NativeMessagePort::CloseChannel(const std::string& error_message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    weak_channel_delegate_->CloseChannel(port_id_, error_message);
  }
}

}  // namespace extensions
