// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_

#include "chrome/browser/devtools/protocol/extensions.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

// Implements the Extensions domain for Chrome DevTools Protocol.
class ExtensionsHandler : public protocol::Extensions::Backend {
 public:
  explicit ExtensionsHandler(protocol::UberDispatcher* dispatcher);

  ExtensionsHandler(const ExtensionsHandler&) = delete;
  ExtensionsHandler& operator=(const ExtensionsHandler&) = delete;

  ~ExtensionsHandler() override;

 private:
  void LoadUnpacked(const protocol::String& path,
                    std::unique_ptr<LoadUnpackedCallback> callback) override;
  void OnLoaded(std::unique_ptr<LoadUnpackedCallback> callback,
                const extensions::Extension* extension,
                const base::FilePath&,
                const std::string&);

  std::unique_ptr<protocol::Extensions::Frontend> frontend_;
  base::WeakPtrFactory<ExtensionsHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EXTENSIONS_HANDLER_H_
