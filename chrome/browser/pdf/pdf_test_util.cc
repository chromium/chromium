// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_test_util.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "url/gurl.h"

namespace pdf_test_util {

std::unique_ptr<extensions::StreamContainer> GenerateSampleStreamContainer(
    int container_number) {
  const std::string container_number_string =
      base::NumberToString(container_number);
  const GURL handler_url =
      GURL("https://handler_url" + container_number_string);
  const std::string extension_id = "extension_id" + container_number_string;
  const GURL original_url =
      GURL("https://original_url" + container_number_string);

  auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL("stream://url" + container_number_string);
  transferrable_loader->head = network::mojom::URLResponseHead::New();
  transferrable_loader->head->mime_type = "application/pdf";
  transferrable_loader->head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2 200 OK");

  return std::make_unique<extensions::StreamContainer>(
      /*tab_id=*/container_number, /*embedded=*/true, handler_url, extension_id,
      std::move(transferrable_loader), original_url);
}

}  // namespace pdf_test_util
