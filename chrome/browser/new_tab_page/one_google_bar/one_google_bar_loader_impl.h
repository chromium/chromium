// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_IMPL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

struct OneGoogleBarData;

class OneGoogleBarLoaderImpl : public OneGoogleBarLoader {
 public:
  OneGoogleBarLoaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& application_locale,
      bool account_consistency_mirror_required);

  OneGoogleBarLoaderImpl(const OneGoogleBarLoaderImpl&) = delete;
  OneGoogleBarLoaderImpl& operator=(const OneGoogleBarLoaderImpl&) = delete;

  ~OneGoogleBarLoaderImpl() override;

  void Load(OneGoogleCallback callback) override;

  GURL GetLoadURLForTesting() const override;

  bool SetAdditionalQueryParams(const std::string& value) override;

 private:
  class AuthenticatedURLLoader;

  GURL GetApiUrl() const;

  void LoadDone(const network::SimpleURLLoader* simple_loader,
                std::unique_ptr<std::string> response_body);

  void JsonParsed(data_decoder::DataDecoder::ValueOrError result);

  void Respond(Status status, const std::optional<OneGoogleBarData>& data);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string application_locale_;
  const bool account_consistency_mirror_required_;

  std::vector<OneGoogleCallback> callbacks_;
  std::unique_ptr<AuthenticatedURLLoader> pending_request_;
  std::string additional_query_params_;

  base::WeakPtrFactory<OneGoogleBarLoaderImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_LOADER_IMPL_H_
