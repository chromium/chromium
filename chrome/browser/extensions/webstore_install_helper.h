// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

class BitmapFetcher;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace extensions {

// This is a class to help dealing with webstore-provided data. It manages
// sending work to the utility process for parsing manifests and
// fetching/decoding icon data. Clients must implement the
// WebstoreInstallHelper::Delegate interface to receive the parsed data.
class WebstoreInstallHelper : public base::RefCounted<WebstoreInstallHelper>,
                              public BitmapFetcherDelegate {
 public:
  class Delegate {
   public:
    enum InstallHelperResultCode {
      UNKNOWN_ERROR,
      ICON_ERROR,
      MANIFEST_ERROR
    };

    // Called when we've successfully parsed the manifest and decoded the icon
    // in the utility process.
    virtual void OnWebstoreParseSuccess(
        const std::string& id,
        const SkBitmap& icon,
        std::unique_ptr<base::DictionaryValue> parsed_manifest) = 0;

    // Called to indicate a parse failure. The |result_code| parameter should
    // indicate whether the problem was with the manifest or icon.
    virtual void OnWebstoreParseFailure(
        const std::string& id,
        InstallHelperResultCode result_code,
        const std::string& error_message) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // It is legal for |icon_url| to be empty.
  WebstoreInstallHelper(Delegate* delegate,
                        const std::string& id,
                        const std::string& manifest,
                        const GURL& icon_url);
  void Start(network::mojom::URLLoaderFactory* loader_factory);

 private:
  friend class base::RefCounted<WebstoreInstallHelper>;

  ~WebstoreInstallHelper() override;

  // Callback for the DataDecoder.
  void OnJSONParsed(data_decoder::DataDecoder::ValueOrError result);

  // Implementing the BitmapFetcherDelegate interface.
  void OnFetchComplete(const GURL& url, const SkBitmap* image) override;

  void ReportResultsIfComplete();

  // The client who we'll report results back to.
  Delegate* delegate_;

  // The extension id of the manifest we're parsing.
  std::string id_;

  // The manifest to parse.
  std::string manifest_;

  // If |icon_url_| is non-empty, it needs to be fetched and decoded into an
  // SkBitmap.
  GURL icon_url_;
  std::unique_ptr<BitmapFetcher> icon_fetcher_;

  // Flags for whether we're done doing icon decoding and manifest parsing.
  bool icon_decode_complete_;
  bool manifest_parse_complete_;

  // The results of successful decoding/parsing.
  SkBitmap icon_;
  std::unique_ptr<base::DictionaryValue> parsed_manifest_;

  // A details string for keeping track of any errors.
  std::string error_;

  // A code to distinguish between an error with the icon, and an error with the
  // manifest.
  Delegate::InstallHelperResultCode parse_error_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
