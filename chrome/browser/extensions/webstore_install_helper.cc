// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/gfx/image/image.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace {

const char kImageDecodeError[] = "Image decode failed";

}  // namespace

namespace extensions {

WebstoreInstallHelper::WebstoreInstallHelper(Delegate* delegate,
                                             const std::string& id,
                                             const std::string& manifest,
                                             const GURL& icon_url)
    : delegate_(delegate),
      id_(id),
      manifest_(manifest),
      icon_url_(icon_url),
      icon_decode_complete_(false),
      manifest_parse_complete_(false),
      parse_error_(Delegate::UNKNOWN_ERROR) {}

WebstoreInstallHelper::~WebstoreInstallHelper() = default;

void WebstoreInstallHelper::Start(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  data_decoder::DataDecoder::ParseJsonIsolated(
      manifest_, base::BindOnce(&WebstoreInstallHelper::OnJSONParsed, this));

  if (icon_url_.is_empty()) {
    icon_decode_complete_ = true;
  } else {
    CHECK(!icon_fetcher_.get());

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("webstore_install_helper", R"(
          semantics {
            sender: "Webstore Install Helper"
            description:
              "Fetches the bitmap corresponding to an extension icon."
            trigger:
              "This can happen in a few different circumstances: "
              "1-User initiated an install from the Chrome Web Store."
              "2-User initiated an inline installation from another website."
              "3-Loading of kiosk app data on Chrome OS (provided that the "
              "kiosk app is a Web Store app)."
            data:
              "The url of the icon for the extension, which includes the "
              "extension id."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "There's no direct Chromium's setting to disable this, but you "
              "could uninstall all extensions and not install (or begin the "
              "installation flow for) any more."
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");

    icon_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::make_unique<ImageDecoderImpl>(), loader_factory);
    image_fetcher::ImageFetcherParams params(traffic_annotation,
                                             "WebstoreInstallHelper");
    icon_fetcher_->FetchImage(
        icon_url_,
        base::BindOnce(&WebstoreInstallHelper::OnFetchComplete, this),
        std::move(params));
  }
}

void WebstoreInstallHelper::OnFetchComplete(
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // OnFetchComplete should only be invoked as a callback of `icon_fetcher_`.
  CHECK(icon_fetcher_.get());

  if (metadata.http_response_code ==
          image_fetcher::RequestMetadata::ResponseCode::RESPONSE_CODE_INVALID ||
      fetched_image.IsEmpty()) {
    error_ = kImageDecodeError;
    parse_error_ = Delegate::ICON_ERROR;
  } else {
    icon_ = fetched_image.AsBitmap();
  }

  icon_decode_complete_ = true;

  ReportResultsIfComplete();

  // `icon_fetcher_` must remain valid after returning to the caller so that
  // any ongoing work can complete safely.
  // Keep this object a reference to `this` until ReleaseIconFetcher runs.
  // Otherwise, this object could be destroyed immediately after this point.
  // Note that it may be deleted as soon as the callback executes.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebstoreInstallHelper::ReleaseIconFetcher, this));
}

void WebstoreInstallHelper::ReleaseIconFetcher() {
  icon_fetcher_.reset();
}

void WebstoreInstallHelper::OnJSONParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  manifest_parse_complete_ = true;
  if (result.has_value() && result->is_dict()) {
    parsed_manifest_ = std::move(*result).TakeDict();
  } else {
    error_ = (!result.has_value() || result.error().empty())
                 ? "Invalid JSON response"
                 : result.error();
    parse_error_ = Delegate::kManifestError;
  }
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::ReportResultsIfComplete() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!icon_decode_complete_ || !manifest_parse_complete_)
    return;

  if (error_.empty() && parsed_manifest_)
    delegate_->OnWebstoreParseSuccess(id_, icon_, std::move(*parsed_manifest_));
  else
    delegate_->OnWebstoreParseFailure(id_, parse_error_, error_);
}

}  // namespace extensions
