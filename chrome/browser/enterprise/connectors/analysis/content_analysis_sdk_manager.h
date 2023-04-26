// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_SDK_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_SDK_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"

#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {

// Manages different content analysis client connections for different
// service providers.  Clients are shared by all profiles running in this
// browser.  This class should only be accessed on the UI thread.
class ContentAnalysisSdkManager {
 public:
  // A wrapper class to ref count std::unique_ptr<content_analysis::sdk::Client>
  // returned from the SDK.
  class WrappedClient : public base::RefCountedThreadSafe<WrappedClient> {
   public:
    explicit WrappedClient(
        std::unique_ptr<content_analysis::sdk::Client> client);

    content_analysis::sdk::Client* client() { return client_.get(); }

   private:
    friend class RefCountedThreadSafe<WrappedClient>;
    ~WrappedClient();

    std::unique_ptr<content_analysis::sdk::Client> client_;
  };

  // This comparator is required in order to use Config in sets or as keys in
  // maps.
  constexpr static auto CompareConfig =
      [](const content_analysis::sdk::Client::Config& c1,
         const content_analysis::sdk::Client::Config& c2) {
        return (c1.name < c2.name) ||
               (c1.name == c2.name && !c1.user_specific && c2.user_specific);
      };

  static ContentAnalysisSdkManager* Get();

  ContentAnalysisSdkManager(const ContentAnalysisSdkManager& other) = delete;
  ContentAnalysisSdkManager(ContentAnalysisSdkManager&& other) = delete;
  ContentAnalysisSdkManager& operator=(const ContentAnalysisSdkManager& other) =
      delete;
  ContentAnalysisSdkManager& operator=(ContentAnalysisSdkManager&& other) =
      delete;

  // Returns an SDK client that matches the given `config`.  When calling
  // this function with the same `config`, a scoped reference to the same
  // `WrapperClient` is returned.  A returned scoped reference remains
  // valid even if the client with the given `config` is reset with a call
  // to ResetClient().
  scoped_refptr<WrappedClient> GetClient(
      content_analysis::sdk::Client::Config config);

  // Called when an error is detected with the given client.  New calls
  // to GetClient() will cause a new SDK client to be created and returned.
  // Existing wrapped clients will continue to use the old client until
  // they release their references.
  // Virtual to be overridden in tests.
  virtual void ResetClient(const content_analysis::sdk::Client::Config& config);

  virtual void ResetAllClients();

  bool HasClientForTesting(
      const content_analysis::sdk::Client::Config& config) {
    return clients_.count(config) > 0;
  }

 protected:
  static void SetManagerForTesting(ContentAnalysisSdkManager* manager);

  // Protected for testing.
  ContentAnalysisSdkManager();
  ~ContentAnalysisSdkManager();

 private:
  friend class base::NoDestructor<ContentAnalysisSdkManager>;

  // Creates a new SDK client.  Virtual to be overridden in tests.
  virtual std::unique_ptr<content_analysis::sdk::Client> CreateClient(
      const content_analysis::sdk::Client::Config& config);

  std::map<content_analysis::sdk::Client::Config,
           scoped_refptr<WrappedClient>,
           decltype(CompareConfig)>
      clients_{CompareConfig};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_SDK_MANAGER_H_
