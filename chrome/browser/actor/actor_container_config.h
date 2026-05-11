// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_H_
#define CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_H_

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace actor {

// ActorContainerConfig is the name of the object in Chrome which manages
// client-side security boundaries for the actor codebase based on the
// AgentContainerConfig proto we receive from the server. The value from the
// server is ingested into the actor task using the `Assign` method, which only
// mutates its caller the first time it is invoked.
class ActorContainerConfig {
 public:
  ActorContainerConfig();
  ActorContainerConfig(const ActorContainerConfig&);
  ActorContainerConfig(ActorContainerConfig&&);
  ActorContainerConfig& operator=(const ActorContainerConfig& other) = delete;
  ActorContainerConfig& operator=(ActorContainerConfig&& other) = delete;
  explicit ActorContainerConfig(
      const optimization_guide::proto::AgentContainerConfig& config);
  ~ActorContainerConfig();

  // Assign the ActorContainerConfig's value to `other`'s by copying `other`'s
  // data. Will only work when `IsActive` returns false, and may
  // cause `IsActive` to return true after. If `other` does not have a
  // config set this is a no-op. Only can modify this object the first
  // time it is called.
  void Assign(const ActorContainerConfig& other);

  bool IsActive() const { return rules_.has_value(); }

  // Indicates whether or not navigation from `source` to `destination` is
  // allowed according to this config.
  // Must not be called when `IsActive` is false.
  bool IsNavigationAllowed(const url::Origin& source,
                           const url::Origin& destination) const;

  // Indicates whether or not the actor can actuate when the browser is
  // navigated to `location_origin`.
  bool IsActuationAllowed(const url::Origin& location_origin) const;

 private:
  // Type alias representing different possible location types that we expect to
  // receive.
  using Wildcard = std::monostate;
  using LocationType = std::variant<Wildcard, net::SchemefulSite, url::Origin>;

  static base::expected<LocationType, std::string_view> ConvertLocation(
      const optimization_guide::proto::Location& location);
  static bool MatchesLocationType(
      const ActorContainerConfig::LocationType& location,
      const url::Origin& origin);
  static std::string LocationTypeToString(
      const ActorContainerConfig::LocationType& location);

  class Rule {
   public:
    Rule();
    Rule(const Rule&);
    Rule(Rule&&);
    Rule& operator=(const Rule&);
    Rule& operator=(Rule&&);
    explicit Rule(std::vector<LocationType> navigation_sources,
                  optimization_guide::proto::RuleMetadata metadata);
    ~Rule();

    static base::expected<Rule, std::string_view> Create(
        const optimization_guide::proto::LocationRule& location_rule);

    bool MatchesNavigationSource(const url::Origin& source_origin) const;
    bool CanNavigate() const;

   private:
    std::vector<LocationType> navigation_sources_;
    optimization_guide::proto::RuleMetadata metadata_;
  };

  bool assign_attempted_ = false;
  // If `rules_` is not set, i.e. is `nullopt`, then the ActorContainerConfig is
  // "empty" and it should not gate any type of behavior. When it is set to a
  // non-null value, only actions allowed by the resulting Rules are allowed.
  std::optional<base::flat_map<LocationType, Rule>> rules_ = std::nullopt;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_H_
