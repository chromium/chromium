// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/chrome_native_message_port_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/messaging/native_message_port.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ChromeNativeMessagePortDispatcher::ChromeNativeMessagePortDispatcher(
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

ChromeNativeMessagePortDispatcher::~ChromeNativeMessagePortDispatcher() {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
}

void ChromeNativeMessagePortDispatcher::DispatchOnMessage(
    const std::string& message) {
  DCHECK(message_service_task_runner_->BelongsToCurrentThread());
  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessageHost::OnMessage,
                                base::Unretained(host_.get()), message));
}

void ChromeNativeMessagePortDispatcher::PostMessageFromNativeHost(
    const std::string& message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessagePort::PostMessageFromNativeHost,
                                port_, message));
}

void ChromeNativeMessagePortDispatcher::CloseChannel(
    const std::string& error_message) {
  DCHECK(host_task_runner_->BelongsToCurrentThread());
  message_service_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMessagePort::CloseChannel, port_, error_message));
}

}  // namespace extensions.
