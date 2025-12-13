// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_android_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// static
std::unique_ptr<NativeMessageHost> NativeMessageHost::Create(
    content::BrowserContext* browser_context,
    gfx::NativeView native_view,
    const ExtensionId& source_extension_id,
    const std::string& native_host_name,
    bool allow_user_level,
    std::string* error_message) {
  return std::make_unique<NativeMessageAndroidHost>(source_extension_id,
                                                    native_host_name);
}

NativeMessageAndroidHost::NativeMessageAndroidHost(
    const ExtensionId& source_extension_id,
    const std::string& native_host_name)
    : source_extension_id_(source_extension_id),
      native_app_name_(native_host_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

NativeMessageAndroidHost::~NativeMessageAndroidHost() = default;

void NativeMessageAndroidHost::OnMessage(const std::string& json) {
  // TODO(crbug.com/417786914): Send the `json` message to the Android app
  // `native_app_name_`.
}

void NativeMessageAndroidHost::Start(Client* client) {
  DCHECK(!client_);
  client_ = client;
}

scoped_refptr<base::SingleThreadTaskRunner>
NativeMessageAndroidHost::task_runner() const {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace extensions
