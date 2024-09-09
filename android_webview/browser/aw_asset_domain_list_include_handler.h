// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ASSET_DOMAIN_LIST_INCLUDE_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ASSET_DOMAIN_LIST_INCLUDE_HANDLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace android_webview {

// Class to handle "include" links in app asset statements by fetching the
// referenced file over the network and parsing the JSON structure.
// See
// https://developers.google.com/digital-asset-links/v1/statements#scaling-to-dozens-of-statements-or-more
class AssetDomainListIncludeHandler {
 public:
  using LoadCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  explicit AssetDomainListIncludeHandler(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AssetDomainListIncludeHandler();

  // Load the specified `include_url` and return the domain names found in "web"
  // targets. The URL is assumed to point to a JSON file with the syntax
  // described here:
  // https://developers.google.com/digital-asset-links/v1/statements.
  // This method will make a network call to fetch the file.
  // The `callback` will be executed on the calling sequence.
  void LoadAppDefinedDomainIncludes(const GURL& include_url,
                                    LoadCallback callback);

 private:
  // Callback for the network response.
  void OnNetworkRequestComplete(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      LoadCallback callback,
      std::unique_ptr<std::string> response_body);

  // Callback for the parse result of the JSON file.
  void OnJsonParseResult(LoadCallback callback,
                         data_decoder::DataDecoder::ValueOrError result);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<AssetDomainListIncludeHandler> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ASSET_DOMAIN_LIST_INCLUDE_HANDLER_H_
