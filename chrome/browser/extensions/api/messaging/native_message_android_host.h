// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ANDROID_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ANDROID_HOST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Manages the native side of a connection between an extension and an Android
// app.
class NativeMessageAndroidHost : public NativeMessageHost {
 public:
  NativeMessageAndroidHost(const ExtensionId& source_extension_id,
                           const std::string& native_app_package_name);

  NativeMessageAndroidHost(const NativeMessageAndroidHost&) = delete;
  NativeMessageAndroidHost& operator=(const NativeMessageAndroidHost&) = delete;

  ~NativeMessageAndroidHost() override;

  // NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  // ID of the calling extension.
  ExtensionId source_extension_id_;

  // Package name of the Android app.
  std::string native_app_name_;

  // The Client messages will be posted to. Should only be accessed from the
  // UI thread.
  raw_ptr<Client> client_ = nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<NativeMessageAndroidHost> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_ANDROID_HOST_H_
