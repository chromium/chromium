// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_MESSAGE_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/messages/android/message_enums.h"

// Android uses messages; Win/Mac/Linux/Chrome OS use infobars.
static_assert(BUILDFLAG(IS_ANDROID));

namespace messages {
class MessageWrapper;
}

namespace ui {
class WindowAndroid;
}

namespace extensions {

// Manages the "Foo started debugging this browser" warning message on Android.
class ExtensionDevToolsMessageDelegate {
 public:
  ExtensionDevToolsMessageDelegate(const std::string& extension_name,
                                   base::OnceClosure dismissed_callback);

  ExtensionDevToolsMessageDelegate(const ExtensionDevToolsMessageDelegate&) =
      delete;
  ExtensionDevToolsMessageDelegate& operator=(
      const ExtensionDevToolsMessageDelegate&) = delete;

  ~ExtensionDevToolsMessageDelegate();

  void Show(ui::WindowAndroid* window);

 private:
  void OnActionClick();
  void OnMessageDismissed(messages::DismissReason reason);

  base::OnceClosure dismissed_callback_;
  std::unique_ptr<messages::MessageWrapper> message_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_EXTENSION_DEV_TOOLS_MESSAGE_DELEGATE_H_
