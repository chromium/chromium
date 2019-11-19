// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DIGITAL_ASSET_LINKS_DIGITAL_ASSET_LINKS_HANDLER_H_
#define CHROME_BROWSER_ANDROID_DIGITAL_ASSET_LINKS_DIGITAL_ASSET_LINKS_HANDLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace digital_asset_links {

extern const char kDigitalAssetLinksCheckResponseKeyLinked[];

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browserservices
enum class RelationshipCheckResult {
  SUCCESS = 0,
  FAILURE,
  NO_CONNECTION
};

using RelationshipCheckResultCallback =
  base::OnceCallback<void(RelationshipCheckResult)>;

// A handler class for sending REST API requests to DigitalAssetLinks web
// end point. See
// https://developers.google.com/digital-asset-links/v1/getting-started
// for details of usage and APIs. These APIs are used to verify declared
// relationships between different asset types like web domains or Android apps.
// The lifecycle of this handler will be governed by the owner.
// The WebContents are used for logging console messages.
class DigitalAssetLinksHandler : public content::WebContentsObserver {
 public:
  DigitalAssetLinksHandler(
      content::WebContents* web_contents,
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  ~DigitalAssetLinksHandler() override;

  // Checks whether the given "relationship" has been declared by the target
  // |web_domain| for the source Android app which is uniquely defined by the
  // |package| and SHA256 |fingerprint| (a string with 32 hexadecimals with :
  // between) given. Any error in the string params here will result in a bad
  // request and a nullptr response to the callback.
  //
  // Calling this multiple times on the same handler will cancel the previous
  // checks.
  // See
  // https://developers.google.com/digital-asset-links/reference/rest/v1/assetlinks/check
  // for details.
  bool CheckDigitalAssetLinkRelationship(
      RelationshipCheckResultCallback callback,
      const std::string& web_domain,
      const std::string& package,
      const std::string& fingerprint,
      const std::string& relationship);

 private:
  void OnURLLoadComplete(const std::string& package,
                         const std::string& fingerprint,
                         const std::string& relationship,
                         std::unique_ptr<std::string> response_body);

  // Callback for the DataDecoder.
  void OnJSONParseResult(const std::string& package,
                         const std::string& fingerprint,
                         const std::string& relationship,
                         data_decoder::DataDecoder::ValueOrError result);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The per request callback for receiving a URLFetcher result. This gets
  // reset every time we get a new CheckDigitalAssetLinkRelationship call.
  RelationshipCheckResultCallback callback_;

  base::WeakPtrFactory<DigitalAssetLinksHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DigitalAssetLinksHandler);
};

}  // namespace digital_asset_links

#endif  // CHROME_BROWSER_ANDROID_DIGITAL_ASSET_LINKS_DIGITAL_ASSET_LINKS_HANDLER_H_
