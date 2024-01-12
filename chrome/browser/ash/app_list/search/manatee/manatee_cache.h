// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_MANATEE_MANATEE_CACHE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_MANATEE_MANATEE_CACHE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace app_list {

using EmbeddingsList = std::vector<std::vector<double>>;

class ManateeCache {
 public:
  explicit ManateeCache(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factor);
  virtual ~ManateeCache();

  ManateeCache(const ManateeCache&) = delete;
  ManateeCache& operator=(const ManateeCache&) = delete;

  using OnResultsCallback = base::OnceCallback<void(EmbeddingsList&)>;

  // Registers a callback to be run whenever the results are updated.
  void RegisterCallback(OnResultsCallback callback);

  std::string GetRequestBody(std::string message);

  std::string VectorToString(std::vector<std::string> messages);

  virtual void UrlLoader(std::vector<std::string> messages);

  EmbeddingsList GetResponse();

 protected:
  EmbeddingsList response_;
  // Callback to run when results are updated.
  OnResultsCallback results_callback_;

 private:
  void OnJsonReceived(const std::unique_ptr<std::string> json_response);

  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  std::unique_ptr<network::SimpleURLLoader> MakeRequestLoader();

  raw_ptr<Profile> profile_;
  // URL below is a placeholder.
  const GURL server_url_{GURL("http://example/url")};
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ManateeCache> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_MANATEE_MANATEE_CACHE_H_
