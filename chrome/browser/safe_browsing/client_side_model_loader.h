// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class loads models for client-side phishing detection
// from the the SafeBrowsing backends.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_MODEL_LOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_MODEL_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {
class ClientSideModel;

// Class which owns and loads a single client-Side detection model.
// The ClientSideDetectionService uses this.
class ModelLoader {
 public:
  static const size_t kMaxModelSizeBytes;
  static const int kClientModelFetchIntervalMs;
  static const char kClientModelFinchExperiment[];
  static const char kClientModelFinchParam[];
  static const char kClientModelUrlPrefix[];
  static const char kClientModelNamePattern[];

  // Constructs a model loader to fetch a model using |url_loader_factory|.
  // When ScheduleFetch is called, |update_renderers| will be called on the
  // same sequence if the fetch is successful.
  ModelLoader(base::Closure update_renderers,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              bool is_extended_reporting);
  virtual ~ModelLoader();

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Schedules the next fetch of the model.
  virtual void ScheduleFetch(int64_t delay_ms);

  // Cancels any pending model fetch. This must be called from the same
  // sequence as ScheduleFetch.
  virtual void CancelFetcher();

  const std::string& model_str() const { return model_str_; }
  const std::string& name() const { return name_; }

 protected:
  // Enum used to keep stats about why we fail to get the client model.
  enum ClientModelStatus {
    MODEL_SUCCESS,
    MODEL_NOT_CHANGED,
    MODEL_FETCH_FAILED,
    MODEL_EMPTY,
    MODEL_TOO_LARGE,
    MODEL_PARSE_ERROR,
    MODEL_MISSING_FIELDS,
    MODEL_INVALID_VERSION_NUMBER,
    MODEL_BAD_HASH_IDS,
    MODEL_STATUS_MAX  // Always add new values before this one.
  };

  // For testing only.
  ModelLoader(base::Closure update_renderers,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const std::string& model_name);

  // This is called periodically to check whether a new client model is
  // available for download.
  virtual void StartFetch();

  // This method is called when we're done fetching the model either because
  // we hit an error somewhere or because we're actually done fetch and
  // validating the model.  If |max_age| is not 0, it's used to schedule the
  // next fetch.
  virtual void EndFetch(ClientModelStatus status, base::TimeDelta max_age);

 private:
  // Use Finch to pick a model number.
  static int GetModelNumber();

  // Construct a model name from parameters.
  static std::string FillInModelName(bool is_extended_reporting,
                                     int model_number);

  // Returns true iff all the hash id's in the client-side model point to
  // valid hashes in the model.
  static bool ModelHasValidHashIds(const ClientSideModel& model);

  // The name of the model is the last component of the URL path.
  const std::string name_;
  // Full URL of the model.
  const GURL url_;

  // If the model isn't yet loaded, model_str_ will be empty.
  std::string model_str_;
  std::unique_ptr<ClientSideModel> model_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Callback to invoke when we've got a new model.  CSD will send it around.
  base::Closure update_renderers_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to check that ScheduleFetch and CancelFetcher are called on the same
  // sequence.
  base::SequenceChecker fetch_sequence_checker_;

  // Used to protect the delayed callback to StartFetchModel()
  base::WeakPtrFactory<ModelLoader> weak_factory_{this};

  friend class ClientSideDetectionServiceTest;
  friend class ModelLoaderTest;
  FRIEND_TEST_ALL_PREFIXES(ModelLoaderTest, FetchModelTest);
  FRIEND_TEST_ALL_PREFIXES(ModelLoaderTest, ModelHasValidHashIds);
  FRIEND_TEST_ALL_PREFIXES(ModelLoaderTest, ModelNamesTest);
  FRIEND_TEST_ALL_PREFIXES(ModelLoaderTest, RescheduleFetchTest);
  FRIEND_TEST_ALL_PREFIXES(ModelLoaderTest, UpdateRenderersTest);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SetEnabledAndRefreshState);
  DISALLOW_COPY_AND_ASSIGN(ModelLoader);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_MODEL_LOADER_H_
