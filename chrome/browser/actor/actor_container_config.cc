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
      return base::unexpected("Protocol not set");
    // `default` is used because Protocol C++ proto has min/max sentinel values
    // as well as unknown.
    default:
      NOTREACHED();
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
    base::expected<ActorContainerConfig::LocationType, std::string_view>
        destination_result = ConvertLocation(rule_proto.location());
    if (!destination_result.has_value()) {
      DLOG(ERROR) << destination_result.error();
      continue;
    }
    base::expected<ActorContainerConfig::Rule, std::string_view> rule =
        ActorContainerConfig::Rule::Create(rule_proto);
    if (!rule.has_value()) {
      DLOG(ERROR) << rule.error();
      continue;
    }
    const ActorContainerConfig::LocationType& destination =
        destination_result.value();
    if (auto it = rules_->find(destination); it != rules_->end()) {
      DLOG(ERROR) << "Duplicate rule for " << LocationTypeToString(destination);
    }
    rules_->insert_or_assign(destination, rule.value());
  }
}

base::expected<ActorContainerConfig::LocationType, std::string_view>
ActorContainerConfig::ConvertLocation(
    const optimization_guide::proto::Location& location) {
  switch (location.identifier_oneof_case()) {
    case optimization_guide::proto::Location::kWildcard:
      return Wildcard();
    case optimization_guide::proto::Location::kSite:
      return ConvertSite(location.site());
    case optimization_guide::proto::Location::kOrigin:
      return ConvertOrigin(location.origin());
    case optimization_guide::proto::Location::IDENTIFIER_ONEOF_NOT_SET:
      return base::unexpected("Location missing value");
  }
}

bool ActorContainerConfig::MatchesLocationType(
    const ActorContainerConfig::LocationType& location,
    const url::Origin& origin) {
  return std::visit(
      absl::Overload([](const Wildcard& unused_wildcard) { return true; },
                     [&](const net::SchemefulSite& site) {
                       return site.IsSameSiteWith(origin);
                     },
                     [&](const url::Origin& loc_origin) {
                       return loc_origin.IsSameOriginWith(origin);
                     }),
      location);
}

std::string ActorContainerConfig::LocationTypeToString(
    const ActorContainerConfig::LocationType& location) {
  return std::visit(
      absl::Overload(
          [](const Wildcard& unused_wildcard) -> std::string {
            return "Wildcard";
          },
          [](const net::SchemefulSite& site) {
            return base::StrCat({"Site(", site.GetDebugString(), ")"});
          },
          [](const url::Origin& origin) {
            return base::StrCat({"Origin(", origin.GetDebugString(), ")"});
          }),
      location);
}

ActorContainerConfig::Rule::Rule() = default;

ActorContainerConfig::Rule::Rule(const ActorContainerConfig::Rule&) = default;

ActorContainerConfig::Rule::Rule(ActorContainerConfig::Rule&&) = default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(
    const ActorContainerConfig::Rule&) = default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(
    ActorContainerConfig::Rule&&) = default;

ActorContainerConfig::Rule::Rule(
    std::vector<ActorContainerConfig::LocationType> navigation_sources,
    optimization_guide::proto::RuleMetadata metadata)
    : navigation_sources_(std::move(navigation_sources)),
      metadata_(std::move(metadata)) {}

ActorContainerConfig::Rule::~Rule() = default;

base::expected<ActorContainerConfig::Rule, std::string_view>
ActorContainerConfig::Rule::Create(
    const optimization_guide::proto::LocationRule& location_rule) {
  std::vector<LocationType> navigation_sources;
  for (const auto& nav_source : location_rule.navigation_sources()) {
    if (!nav_source.has_source()) {
      return base::unexpected("NavigationSource has no source location set");
    }
    ASSIGN_OR_RETURN(LocationType source, ConvertLocation(nav_source.source()));
    navigation_sources.emplace_back(source);
  }
  return ActorContainerConfig::Rule(navigation_sources,
                                    location_rule.metadata());
}

bool ActorContainerConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return MatchesLocationType(source, source_origin);
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
          base::FindOrNull(rules_.value(), LocationType(destination));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          rules_.value(), LocationType(net::SchemefulSite(destination)));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(rules_.value(), LocationType(Wildcard()));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  return false;
}

bool ActorContainerConfig::IsActuationAllowed(
    const url::Origin& location_origin) const {
  CHECK(IsActive());
  if (const auto* rule =
          base::FindOrNull(rules_.value(), LocationType(location_origin))) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          rules_.value(), LocationType(net::SchemefulSite(location_origin)))) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(rules_.value(), LocationType(Wildcard()))) {
    return rule->CanNavigate();
  }
  return false;
}

}  // namespace actor
