// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "extensions/buildflags/buildflags.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// This is a class to help dealing with webstore-provided data. It manages
// sending work to the utility process for parsing manifests and
// fetching/decoding icon data. Clients must implement the
// WebstoreInstallHelper::Delegate interface to receive the parsed data.
class WebstoreInstallHelper : public base::RefCounted<WebstoreInstallHelper> {
 public:
  class Delegate {
   public:
    enum InstallHelperResultCode { UNKNOWN_ERROR, ICON_ERROR, kManifestError };

    // Called when we've successfully parsed the manifest and decoded the icon
    // in the utility process.
    virtual void OnWebstoreParseSuccess(const std::string& id,
                                        const SkBitmap& icon,
                                        base::DictValue parsed_manifest) = 0;

    // Called to indicate a parse failure. The `result_code` parameter should
    // indicate whether the problem was with the manifest or icon.
    virtual void OnWebstoreParseFailure(
        const std::string& id,
        InstallHelperResultCode result_code,
        const std::string& error_message) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // It is legal for `icon_url` to be empty.
  WebstoreInstallHelper(Delegate* delegate,
                        const std::string& id,
                        const std::string& manifest,
                        const GURL& icon_url);
  void Start(scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

 private:
  friend class base::RefCounted<WebstoreInstallHelper>;

  ~WebstoreInstallHelper();

  // Callback for the DataDecoder.
  void OnJSONParsed(data_decoder::DataDecoder::ValueOrError result);

  void OnFetchComplete(const gfx::Image& fetched_image,
                       const image_fetcher::RequestMetadata& metadata);

  // This is invoked as a callback holding a retained reference to `this`.
  // The object may be destroyed immediately after this method returns.
  void ReleaseIconFetcher();

  void ReportResultsIfComplete();

  // The client who we'll report results back to.
  raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // The extension id of the manifest we're parsing.
  std::string id_;

  // The manifest to parse.
  std::string manifest_;

  // If `icon_url_` is non-empty, it needs to be fetched and decoded into an
  // SkBitmap.
  GURL icon_url_;
  std::unique_ptr<image_fetcher::ImageFetcher> icon_fetcher_;

  // Flags for whether we're done doing icon decoding and manifest parsing.
  bool icon_decode_complete_;
  bool manifest_parse_complete_;

  // The results of successful decoding/parsing.
  SkBitmap icon_;
  std::optional<base::DictValue> parsed_manifest_;

  // A details string for keeping track of any errors.
  std::string error_;

  // A code to distinguish between an error with the icon, and an error with the
  // manifest.
  Delegate::InstallHelperResultCode parse_error_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_INSTALL_HELPER_H_
