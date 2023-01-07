// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/intervention_policy_database.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/resource_coordinator/utils.h"

namespace resource_coordinator {

InterventionPolicyDatabase::OriginInterventionPolicies::
    OriginInterventionPolicies(InterventionPolicy discarding_policy,
                               InterventionPolicy freezing_policy)
    : discarding_policy(discarding_policy), freezing_policy(freezing_policy) {}

InterventionPolicyDatabase::InterventionPolicyDatabase() {}
InterventionPolicyDatabase::~InterventionPolicyDatabase() = default;

InterventionPolicyDatabase::InterventionPolicy
InterventionPolicyDatabase::GetDiscardingPolicy(
    const url::Origin& origin) const {
  const auto iter = database_.find(SerializeOriginIntoDatabaseKey(origin));
  if (iter == database_.end())
    return OriginInterventions::DEFAULT;
  return iter->second.discarding_policy;
}

InterventionPolicyDatabase::InterventionPolicy
InterventionPolicyDatabase::GetFreezingPolicy(const url::Origin& origin) const {
  const auto iter = database_.find(SerializeOriginIntoDatabaseKey(origin));
  if (iter == database_.end())
    return OriginInterventions::DEFAULT;
  return iter->second.freezing_policy;
}

void InterventionPolicyDatabase::InitializeDatabaseWithProtoFile(
    const base::FilePath& proto_location,
    const base::Version& version,
    base::Value::Dict manifest) {
  // TODO(sebmarchand): Validate the version and the manifest?
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(
          &InterventionPolicyDatabase::ReadDatabaseFromProtoFileOnSequence,
          proto_location),
      base::BindOnce(&InterventionPolicyDatabase::OnReadDatabaseProtoFromFile,
                     weak_factory_.GetWeakPtr()));
}

void InterventionPolicyDatabase::AddOriginPoliciesForTesting(
    const url::Origin& origin,
    OriginInterventionPolicies policies) {
  database_.emplace(SerializeOriginIntoDatabaseKey(origin),
                    std::move(policies));
}

// static
InterventionPolicyDatabase::InterventionsMap
InterventionPolicyDatabase::ReadDatabaseFromProtoFileOnSequence(
    const base::FilePath& proto_location) {
  DCHECK(base::PathExists(proto_location));

  InterventionsMap database;

  std::string proto_str;
  if (!base::ReadFileToString(proto_location, &proto_str)) {
    DLOG(ERROR) << "Failed to read the interventon policy database.";
    return database;
  }

  OriginInterventionsDatabase proto;
  if (!proto.ParseFromString(proto_str)) {
    DLOG(ERROR) << "Unable to parse the intervention policy database proto.";
    return database;
  }

  database.reserve(proto.origin_interventions_size());
  for (int i = 0; i < proto.origin_interventions_size(); ++i) {
    const OriginInterventions& origin_interventions_proto =
        proto.origin_interventions(i);
    OriginInterventionPolicies origin_intervention_policies(
        origin_interventions_proto.discarding_policy(),
        origin_interventions_proto.freezing_policy());
    database.emplace(origin_interventions_proto.host_hash(),
                     std::move(origin_intervention_policies));
  }
  return database;
}

void InterventionPolicyDatabase::OnReadDatabaseProtoFromFile(
    InterventionsMap database) {
  database_ = std::move(database);
}

}  // namespace resource_coordinator
