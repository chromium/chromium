// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_container_config.h"

#include <string>
#include <string_view>
#include <variant>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace actor {

namespace {

base::expected<std::string_view, std::string_view> ConvertProtocol(
    optimization_guide::proto::Protocol protocol) {
  switch (protocol) {
    case optimization_guide::proto::Protocol::PROTOCOL_HTTPS:
      return base::ok(url::kHttpsScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_HTTP:
      return base::ok(url::kHttpScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_WSS:
      return base::ok(url::kWssScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_WS:
      return base::ok(url::kWsScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_UNKNOWN:
    default:
      return base::unexpected("Unknown protocol");
  }
}

base::expected<net::SchemefulSite, std::string_view> ConvertSite(
    const optimization_guide::proto::Site& site) {
  if (!site.has_domain()) {
    return base::unexpected("Site message is missing domain");
  }
  ASSIGN_OR_RETURN(std::string_view protocol, ConvertProtocol(site.protocol()));
  net::SchemefulSite converted_site(GURL(
      base::StrCat({protocol, url::kStandardSchemeSeparator, site.domain()})));
  if (converted_site.GetURL().host() != site.domain()) {
    return base::unexpected("SchemefulSite domain does not match the message");
  }
  return converted_site;
}

base::expected<url::Origin, std::string_view> ConvertOrigin(
    const optimization_guide::proto::Origin& origin) {
  if (!origin.has_host()) {
    return base::unexpected("Origin message is missing host");
  }
  ASSIGN_OR_RETURN(std::string_view protocol,
                   ConvertProtocol(origin.protocol()));
  std::string port =
      origin.has_port()
          ? base::StrCat({":", base::NumberToString(origin.port())})
          : "";
  return url::Origin::Create(GURL(base::StrCat(
      {protocol, url::kStandardSchemeSeparator, origin.host(), port})));
}

}  // namespace

ActorContainerConfig::ActorContainerConfig() = default;

ActorContainerConfig::ActorContainerConfig(const ActorContainerConfig&) =
    default;

ActorContainerConfig::ActorContainerConfig(ActorContainerConfig&&) = default;

ActorContainerConfig::~ActorContainerConfig() = default;

void ActorContainerConfig::Assign(
    base::optional_ref<const optimization_guide::proto::AgentContainerConfig>
        config) {
  if (assign_attempted_) {
    return;
  }
  assign_attempted_ = true;
  CHECK(!rules_.has_value());

  if (!config.has_value()) {
    return;
  }

  rules_.emplace();
  for (const auto& rule_proto : config->location_rules()) {
    base::expected<Location, std::string_view> destination_result =
        Location::Create(rule_proto.location());
    if (!destination_result.has_value()) {
      DLOG(ERROR) << destination_result.error();
      continue;
    }
    base::expected<Rule, std::string_view> rule = Rule::Create(rule_proto);
    if (!rule.has_value()) {
      DLOG(ERROR) << rule.error();
      continue;
    }
    const Location& destination = destination_result.value();
    if (auto it = rules_->find(destination); it != rules_->end()) {
      DLOG(ERROR) << "Duplicate rule for " << destination.ToDebugString();
    }
    rules_->insert_or_assign(destination, rule.value());
  }
}

ActorContainerConfig::Location::Location(Wildcard) : data_(Wildcard()) {}

ActorContainerConfig::Location::Location(net::SchemefulSite site)
    : data_(std::move(site)) {}

ActorContainerConfig::Location::Location(url::Origin origin)
    : data_(std::move(origin)) {}

ActorContainerConfig::Location::Location(const Location&) = default;
ActorContainerConfig::Location::Location(Location&&) = default;
ActorContainerConfig::Location& ActorContainerConfig::Location::operator=(
    const Location&) = default;
ActorContainerConfig::Location& ActorContainerConfig::Location::operator=(
    Location&&) = default;

ActorContainerConfig::Location::~Location() = default;

base::expected<ActorContainerConfig::Location, std::string_view>
ActorContainerConfig::Location::Create(
    const optimization_guide::proto::Location& location) {
  switch (location.identifier_oneof_case()) {
    case optimization_guide::proto::Location::kWildcard:
      return Location(Wildcard());
    case optimization_guide::proto::Location::kSite: {
      ASSIGN_OR_RETURN(net::SchemefulSite site, ConvertSite(location.site()));
      return Location(std::move(site));
    }
    case optimization_guide::proto::Location::kOrigin: {
      ASSIGN_OR_RETURN(url::Origin origin, ConvertOrigin(location.origin()));
      return Location(std::move(origin));
    }
    case optimization_guide::proto::Location::IDENTIFIER_ONEOF_NOT_SET:
      return base::unexpected("Location missing value");
    default:
      return base::unexpected("Unknown location type");
  }
}

bool ActorContainerConfig::Location::Matches(const url::Origin& origin) const {
  return std::visit(absl::Overload([](const Wildcard&) { return true; },
                                   [&](const net::SchemefulSite& site) {
                                     return site.IsSameSiteWith(origin);
                                   },
                                   [&](const url::Origin& loc_origin) {
                                     return loc_origin.IsSameOriginWith(origin);
                                   }),
                    data_);
}

std::string ActorContainerConfig::Location::ToDebugString() const {
  return std::visit(
      absl::Overload(
          [](const Wildcard&) -> std::string { return "Wildcard"; },
          [](const net::SchemefulSite& site) {
            return base::StrCat({"Site(", site.GetDebugString(), ")"});
          },
          [](const url::Origin& origin) {
            return base::StrCat({"Origin(", origin.GetDebugString(), ")"});
          }),
      data_);
}

ActorContainerConfig::Rule::Rule() = default;

ActorContainerConfig::Rule::Rule(const Rule&) = default;

ActorContainerConfig::Rule::Rule(Rule&&) = default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(const Rule&) =
    default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(Rule&&) =
    default;

ActorContainerConfig::Rule::Rule(
    std::vector<Location> navigation_sources,
    optimization_guide::proto::RuleMetadata metadata)
    : navigation_sources_(std::move(navigation_sources)),
      metadata_(std::move(metadata)) {}

ActorContainerConfig::Rule::~Rule() = default;

base::expected<ActorContainerConfig::Rule, std::string_view>
ActorContainerConfig::Rule::Create(
    const optimization_guide::proto::LocationRule& location_rule) {
  std::vector<Location> navigation_sources;
  for (const auto& nav_source : location_rule.navigation_sources()) {
    if (!nav_source.has_source()) {
      return base::unexpected("NavigationSource has no source location set");
    }
    ASSIGN_OR_RETURN(Location source, Location::Create(nav_source.source()));
    navigation_sources.emplace_back(source);
  }
  return Rule(navigation_sources, location_rule.metadata());
}

bool ActorContainerConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return source.Matches(source_origin);
         });
}

bool ActorContainerConfig::Rule::CanNavigate() const {
  return std::ranges::contains(
             metadata_.capabilities(),
             optimization_guide::proto::RuleMetadata::CAPABILITY_ALL) &&
         std::ranges::contains(
             metadata_.accessible_resources(),
             optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
}

bool ActorContainerConfig::IsNavigationAllowed(
    const url::Origin& source,
    const url::Origin& destination) const {
  CHECK(IsActive());
  if (const auto* rule =
          base::FindOrNull(rules_.value(), Location(destination));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          rules_.value(), Location(net::SchemefulSite(destination)));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(rules_.value(), Location(Wildcard()));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  return false;
}

bool ActorContainerConfig::IsActuationAllowed(
    const url::Origin& location_origin) const {
  CHECK(IsActive());
  if (const auto* rule =
          base::FindOrNull(rules_.value(), Location(location_origin))) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          rules_.value(), Location(net::SchemefulSite(location_origin)))) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(rules_.value(), Location(Wildcard()))) {
    return rule->CanNavigate();
  }
  return false;
}

}  // namespace actor
