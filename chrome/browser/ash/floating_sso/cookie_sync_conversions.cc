// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/cookie_sync_conversions.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"

namespace ash::floating_sso {

namespace {

base::Time DeserializeTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

net::CookieSameSite CookieSameSiteFromProtoEnum(
    const sync_pb::CookieSpecifics_CookieSameSite& proto_enum) {
  switch (proto_enum) {
    case sync_pb::CookieSpecifics_CookieSameSite_UNSPECIFIED:
      return net::CookieSameSite::UNSPECIFIED;
    case sync_pb::CookieSpecifics_CookieSameSite_NO_RESTRICTION:
      return net::CookieSameSite::NO_RESTRICTION;
    case sync_pb::CookieSpecifics_CookieSameSite_LAX_MODE:
      return net::CookieSameSite::LAX_MODE;
    case sync_pb::CookieSpecifics_CookieSameSite_STRICT_MODE:
      return net::CookieSameSite::STRICT_MODE;
  }
}

sync_pb::CookieSpecifics_CookieSameSite ProtoEnumFromCookieSameSite(
    const net::CookieSameSite& same_site) {
  switch (same_site) {
    case net::CookieSameSite::UNSPECIFIED:
      return sync_pb::CookieSpecifics_CookieSameSite_UNSPECIFIED;
    case net::CookieSameSite::NO_RESTRICTION:
      return sync_pb::CookieSpecifics_CookieSameSite_NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return sync_pb::CookieSpecifics_CookieSameSite_LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return sync_pb::CookieSpecifics_CookieSameSite_STRICT_MODE;
  }
}

net::CookiePriority CookiePriorityFromProtoEnum(
    const sync_pb::CookieSpecifics_CookiePriority& proto_enum) {
  switch (proto_enum) {
    case sync_pb::CookieSpecifics_CookiePriority_UNSPECIFIED_PRIORITY:
      return net::CookiePriority::COOKIE_PRIORITY_DEFAULT;
    case sync_pb::CookieSpecifics_CookiePriority_LOW:
      return net::CookiePriority::COOKIE_PRIORITY_LOW;
    case sync_pb::CookieSpecifics_CookiePriority_MEDIUM:
      return net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
    case sync_pb::CookieSpecifics_CookiePriority_HIGH:
      return net::CookiePriority::COOKIE_PRIORITY_HIGH;
  }
}

sync_pb::CookieSpecifics_CookiePriority ProtoEnumFromCookiePriority(
    const net::CookiePriority& priority) {
  switch (priority) {
    case net::CookiePriority::COOKIE_PRIORITY_LOW:
      return sync_pb::CookieSpecifics_CookiePriority_LOW;
    case net::CookiePriority::COOKIE_PRIORITY_MEDIUM:
      return sync_pb::CookieSpecifics_CookiePriority_MEDIUM;
    case net::CookiePriority::COOKIE_PRIORITY_HIGH:
      return sync_pb::CookieSpecifics_CookiePriority_HIGH;
  }
}

net::CookieSourceScheme CookieSourceSchemeFromProtoEnum(
    const sync_pb::CookieSpecifics_CookieSourceScheme& proto_enum) {
  switch (proto_enum) {
    case sync_pb::CookieSpecifics_CookieSourceScheme_UNSET:
      return net::CookieSourceScheme::kUnset;
    case sync_pb::CookieSpecifics_CookieSourceScheme_NON_SECURE:
      return net::CookieSourceScheme::kNonSecure;
    case sync_pb::CookieSpecifics_CookieSourceScheme_SECURE:
      return net::CookieSourceScheme::kSecure;
  }
}

sync_pb::CookieSpecifics_CookieSourceScheme ProtoEnumFromCookieSourceScheme(
    const net::CookieSourceScheme& source_scheme) {
  switch (source_scheme) {
    case net::CookieSourceScheme::kUnset:
      return sync_pb::CookieSpecifics_CookieSourceScheme_UNSET;
    case net::CookieSourceScheme::kNonSecure:
      return sync_pb::CookieSpecifics_CookieSourceScheme_NON_SECURE;
    case net::CookieSourceScheme::kSecure:
      return sync_pb::CookieSpecifics_CookieSourceScheme_SECURE;
  }
}

net::CookieSourceType CookieSourceTypeFromProtoEnum(
    const sync_pb::CookieSpecifics_CookieSourceType& proto_enum) {
  switch (proto_enum) {
    case sync_pb::CookieSpecifics_CookieSourceType_UNKNOWN:
      return net::CookieSourceType::kUnknown;
    case sync_pb::CookieSpecifics_CookieSourceType_HTTP:
      return net::CookieSourceType::kHTTP;
    case sync_pb::CookieSpecifics_CookieSourceType_SCRIPT:
      return net::CookieSourceType::kScript;
    case sync_pb::CookieSpecifics_CookieSourceType_OTHER:
      return net::CookieSourceType::kOther;
  }
}

sync_pb::CookieSpecifics_CookieSourceType ProtoEnumFromCookieSourceType(
    const net::CookieSourceType& source_type) {
  switch (source_type) {
    case net::CookieSourceType::kUnknown:
      return sync_pb::CookieSpecifics_CookieSourceType_UNKNOWN;
    case net::CookieSourceType::kHTTP:
      return sync_pb::CookieSpecifics_CookieSourceType_HTTP;
    case net::CookieSourceType::kScript:
      return sync_pb::CookieSpecifics_CookieSourceType_SCRIPT;
    case net::CookieSourceType::kOther:
      return sync_pb::CookieSpecifics_CookieSourceType_OTHER;
  }
}

}  // namespace

int64_t ToMicrosSinceWindowsEpoch(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

std::unique_ptr<net::CanonicalCookie> FromSyncProto(
    const sync_pb::CookieSpecifics& proto) {
  base::expected<std::optional<net::CookiePartitionKey>, std::string>
      partition_key = net::CookiePartitionKey::FromStorage(
          proto.partition_key().top_level_site(),
          proto.partition_key().has_cross_site_ancestor());
  if (!partition_key.has_value()) {
    return nullptr;
  }

  // Returns nullptr if the resulting cookie is not canonical.
  // ATTENTION: If you change this code after changing something in
  // `CanonicalCookie`, make sure that the changes are fully reflected in
  // components/sync/protocol/cookie_specifics.proto and in `ToSyncProto`
  // function below.
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::FromStorage(
          proto.name(),                                                    //
          proto.value(),                                                   //
          proto.domain(),                                                  //
          proto.path(),                                                    //
          DeserializeTime(proto.creation_time_windows_epoch_micros()),     //
          DeserializeTime(proto.expiry_time_windows_epoch_micros()),       //
          DeserializeTime(proto.last_access_time_windows_epoch_micros()),  //
          DeserializeTime(proto.last_update_time_windows_epoch_micros()),  //
          proto.secure(),                                                  //
          proto.httponly(),                                                //
          CookieSameSiteFromProtoEnum(proto.site_restrictions()),          //
          CookiePriorityFromProtoEnum(proto.priority()),                   //
          std::move(partition_key.value()),                                //
          CookieSourceSchemeFromProtoEnum(proto.source_scheme()),          //
          proto.source_port(),                                             //
          CookieSourceTypeFromProtoEnum(proto.source_type()));             //

  return cookie;
}

std::optional<sync_pb::CookieSpecifics> ToSyncProto(
    const net::CanonicalCookie& cookie) {
  base::expected<net::CookiePartitionKey::SerializedCookiePartitionKey,
                 std::string>
      serialized_partition_key =
          net::CookiePartitionKey::Serialize(cookie.PartitionKey());
  if (!serialized_partition_key.has_value()) {
    return std::nullopt;
  }

  // Serialize StrictlyUniqueKey: it will be written to specifics proto
  // and used as a client tag.
  // TODO(b/318391357): move serialization of the key elsewhere when
  // implementing CookieSyncBridge. We should guarantee that we don't update it
  // for existing entities.
  net::CookieBase::StrictlyUniqueCookieKey key = cookie.StrictlyUniqueKey();
  const auto& [partition_key, name, domain, path, source_scheme, source_port] =
      key;
  std::string serialized_key = base::StrCat(
      {serialized_partition_key->TopLevelSite(),
       (serialized_partition_key->has_cross_site_ancestor() ? "true" : "false"),
       name, domain, path,
       base::NumberToString(static_cast<int>(source_scheme)),
       base::NumberToString(source_port)});

  sync_pb::CookieSpecifics proto;
  proto.set_unique_key(serialized_key);
  proto.set_name(cookie.Name());
  proto.set_value(cookie.Value());
  proto.set_domain(cookie.Domain());
  proto.set_path(cookie.Path());
  proto.set_creation_time_windows_epoch_micros(
      ToMicrosSinceWindowsEpoch(cookie.CreationDate()));
  proto.set_expiry_time_windows_epoch_micros(
      ToMicrosSinceWindowsEpoch(cookie.ExpiryDate()));
  proto.set_last_access_time_windows_epoch_micros(
      ToMicrosSinceWindowsEpoch(cookie.LastAccessDate()));
  proto.set_last_update_time_windows_epoch_micros(
      ToMicrosSinceWindowsEpoch(cookie.LastUpdateDate()));
  proto.set_secure(cookie.IsSecure());
  proto.set_httponly(cookie.IsHttpOnly());
  proto.set_site_restrictions(ProtoEnumFromCookieSameSite(cookie.SameSite()));
  proto.set_priority(ProtoEnumFromCookiePriority(cookie.Priority()));
  proto.set_source_scheme(
      ProtoEnumFromCookieSourceScheme(cookie.SourceScheme()));
  proto.mutable_partition_key()->set_top_level_site(
      serialized_partition_key->TopLevelSite());
  proto.mutable_partition_key()->set_has_cross_site_ancestor(
      serialized_partition_key->has_cross_site_ancestor());
  proto.set_source_port(cookie.SourcePort());
  proto.set_source_type(ProtoEnumFromCookieSourceType(cookie.SourceType()));

  return proto;
}

}  // namespace ash::floating_sso
