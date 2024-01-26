// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_SCOPED_MOCK_FIRST_PARTY_SETS_HANDLER_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_SCOPED_MOCK_FIRST_PARTY_SETS_HANDLER_H_

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"

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
//
// Uses an RAII-pattern to install itself as the global singleton in the ctor,
// and remove itself in the dtor.
class ScopedMockFirstPartySetsHandler : public content::FirstPartySetsHandler {
 public:
  ScopedMockFirstPartySetsHandler();
  ~ScopedMockFirstPartySetsHandler() override;

  // FirstPartySetsHandler:
  bool IsEnabled() const override;
  void SetPublicFirstPartySets(const base::Version& version,
                               base::File sets_file) override;
  std::optional<net::FirstPartySetEntry> FindEntry(
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
  void ComputeFirstPartySetMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const net::FirstPartySetsContextConfig& config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) override;
  bool ForEachEffectiveSetEntry(
      const net::FirstPartySetsContextConfig& config,
      base::FunctionRef<bool(const net::SchemefulSite&,
                             const net::FirstPartySetEntry&)> f) const override;

  // Helper functions for tests to set up context.
  void SetContextConfig(net::FirstPartySetsContextConfig config);

  void SetCacheFilter(net::FirstPartySetsCacheFilter cache_filter);

  void SetGlobalSets(net::GlobalFirstPartySets global_sets);

  void set_invoke_callbacks_asynchronously(bool asynchronous) {
    invoke_callbacks_asynchronously_ = asynchronous;
  }

 private:
  raw_ptr<content::FirstPartySetsHandler> previous_;
  net::GlobalFirstPartySets global_sets_;
  net::FirstPartySetsContextConfig config_;
  net::FirstPartySetsCacheFilter cache_filter_;

  bool invoke_callbacks_asynchronously_ = false;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_SCOPED_MOCK_FIRST_PARTY_SETS_HANDLER_H_
