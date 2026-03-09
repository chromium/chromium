// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_SCRIPT_LOADER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_SCRIPT_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace base {
class FilePath;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace indigo {

// Loads a JavaScript file from either a local file or from the network.
class IndigoScriptLoader {
 public:
  using LoadCallback = base::OnceCallback<void(std::optional<std::string>)>;

  explicit IndigoScriptLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  IndigoScriptLoader(const IndigoScriptLoader&) = delete;
  IndigoScriptLoader& operator=(const IndigoScriptLoader&) = delete;
  ~IndigoScriptLoader();

  // Loads the script at |path|. If |path| starts with "http:" or "https:",
  // it is loaded over the network. Otherwise, it is treated as a local
  // filename.
  void Load(std::string_view path, LoadCallback callback);

 private:
  void LoadFromFile(const base::FilePath& path, LoadCallback callback);
  void LoadFromNetwork(const GURL& url, LoadCallback callback);

  void OnNetworkLoadComplete(
      LoadCallback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      std::optional<std::string> response_body);
  void OnFileLoadComplete(LoadCallback callback,
                          std::optional<std::string> content);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<IndigoScriptLoader> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_SCRIPT_LOADER_H_
