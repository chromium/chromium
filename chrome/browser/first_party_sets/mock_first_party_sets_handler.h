// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_MOCK_FIRST_PARTY_SETS_HANDLER_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_MOCK_FIRST_PARTY_SETS_HANDLER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Version;
class File;
class Value;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace first_party_sets {

// Used to create a dummy FirstPartySetsHandler implementation for testing
// purposes. Enabled by default.
class MockFirstPartySetsHandler : public content::FirstPartySetsHandler {
 public:
  MockFirstPartySetsHandler();
  ~MockFirstPartySetsHandler() override;

  // FirstPartySetsHandler:
  bool IsEnabled() const override;
  void SetPublicFirstPartySets(const base::Version& version,
                               base::File sets_file) override;
  void ResetForTesting() override;
  void SetGlobalSetsForTesting(net::GlobalFirstPartySets global_sets) override;
  absl::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site,
      const net::FirstPartySetsContextConfig& config) const override;
  void GetContextConfigForPolicy(
      const base::Value::Dict* policy,
      base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback)
      override;
  void ClearSiteDataOnChangedSetsForContext(
      base::RepeatingCallback<content::BrowserContext*()>
          browser_context_getter,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config,
      base::OnceCallback<void(net::FirstPartySetsContextConfig,
                              net::FirstPartySetsCacheFilter)> callback)
      override;

  // Helper functions for tests to set up context.
  void SetContextConfig(net::FirstPartySetsContextConfig config);
  void SetCacheFilter(net::FirstPartySetsCacheFilter cache_filter);

 private:
  absl::optional<net::GlobalFirstPartySets> global_sets_;
  absl::optional<net::FirstPartySetsContextConfig> config_;
  absl::optional<net::FirstPartySetsCacheFilter> cache_filter_;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_MOCK_FIRST_PARTY_SETS_HANDLER_H_
