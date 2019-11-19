// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_IMPL_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

struct SearchSuggestData;

class SearchSuggestLoaderImpl : public SearchSuggestLoader {
 public:
  SearchSuggestLoaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& application_locale);
  ~SearchSuggestLoaderImpl() override;

  void Load(const std::string& blocklist,
            SearchSuggestionsCallback callback) override;

  GURL GetLoadURLForTesting() const override;

 private:
  class AuthenticatedURLLoader;

  GURL GetApiUrl(const std::string& blocklist) const;

  void LoadDone(const network::SimpleURLLoader* simple_loader,
                std::unique_ptr<std::string> response_body);

  void JsonParsed(data_decoder::DataDecoder::ValueOrError result);

  void Respond(Status status, const base::Optional<SearchSuggestData>& data);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::string application_locale_;

  std::vector<SearchSuggestionsCallback> callbacks_;
  std::unique_ptr<AuthenticatedURLLoader> pending_request_;

  base::WeakPtrFactory<SearchSuggestLoaderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchSuggestLoaderImpl);
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_LOADER_IMPL_H_
