// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_WEB_PAGE_INFO_LACROS_H_
#define CHROME_BROWSER_LACROS_WEB_PAGE_INFO_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// This class receives the web page info api calls from ash, and send lacros
// web page info to ash. It can only be used on the main thread.
class WebPageInfoProviderLacros : public mojom::WebPageInfoProvider {
 public:
  WebPageInfoProviderLacros();
  WebPageInfoProviderLacros(const WebPageInfoProviderLacros&) = delete;
  WebPageInfoProviderLacros& operator=(const WebPageInfoProviderLacros&) =
      delete;
  ~WebPageInfoProviderLacros() override;

 private:
  // mojom::WebPageInfoProvider:
  void RequestCurrentWebPageInfo(
      RequestCurrentWebPageInfoCallback callback) override;

  mojo::Receiver<mojom::WebPageInfoProvider> receiver_{this};

  base::WeakPtrFactory<WebPageInfoProviderLacros> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_WEB_PAGE_INFO_LACROS_H_
