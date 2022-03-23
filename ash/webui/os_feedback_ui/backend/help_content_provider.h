// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_

#include <string>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace ash {
namespace feedback {

using GetHelpContentsCallback =
    base::OnceCallback<void(os_feedback_ui::mojom::SearchResponsePtr)>;

// Convert a search request to a JSON string as the payload to be sent to the
// search API.
std::string ConvertSearchRequestToJson(
    const std::string& app_locale,
    const os_feedback_ui::mojom::SearchRequestPtr& request);

// Convert the result_type string to HelpContentType.
os_feedback_ui::mojom::HelpContentType ToHelpContentType(
    const std::string& result_type);

// Parse the |json| string and populate |search_response| with HelpContents and
// totalResults.
//
// Sample json string:
//  {
//   "resource": [
//     {
//       "url":
//       "/chromebook/thread/110208459?hl=en-gb",
//       "title": "Bluetooth Headphones",
//       "snippet": "I have ...",
//       "resultType": "CT_SUPPORT_FORUM_THREAD",
//       ...
//     },
//   ],
//   "searchSessionId": "637823722854740455-2849874850",
//   "totalResults": "2415"
// }
void PopulateSearchResponse(
    const base::Value& search_result,
    os_feedback_ui::mojom::SearchResponsePtr& search_response);

// HelpContentProvider is responsible for handling the mojo call for
// GetHelpContents.
class HelpContentProvider : os_feedback_ui::mojom::HelpContentProvider {
 public:
  HelpContentProvider(const std::string& app_locale,
                      content::BrowserContext* browser_context);
  HelpContentProvider(
      const std::string& app_locale,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  HelpContentProvider(const HelpContentProvider&) = delete;
  HelpContentProvider& operator=(const HelpContentProvider&) = delete;
  ~HelpContentProvider() override;

  // os_feedback_ui::mojom::HelpContentProvider:
  void GetHelpContents(os_feedback_ui::mojom::SearchRequestPtr request,
                       GetHelpContentsCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
          receiver);

 private:
  // Call when the |url_loader| receives response from the search service.
  void OnHelpContentSearchResponse(
      GetHelpContentsCallback callback,
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      std::unique_ptr<std::string> response_body);
  // Called when the data decoder service provides parsed JSON data for a
  // server response.
  void OnResponseJsonParsed(GetHelpContentsCallback callback,
                            data_decoder::DataDecoder::ValueOrError result);

  std::string app_locale_;
  // Decoder for data decoding service.
  data_decoder::DataDecoder data_decoder_;
  // URLLoaderFactory used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  mojo::Receiver<os_feedback_ui::mojom::HelpContentProvider> receiver_{this};
  base::WeakPtrFactory<HelpContentProvider> weak_ptr_factory_{this};
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
