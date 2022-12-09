// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_INTERVENTION_POLICY_DATABASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_INTERVENTION_POLICY_DATABASE_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/resource_coordinator/intervention_policy_database.pb.h"
#include "url/origin.h"

namespace resource_coordinator {

// Intervention policy database, this should receive data from the
// InterventionPolicyDatabaseComponentInstallerPolicy component once it's ready.
//
// It is meant to be used to assist intervention decisions made for the
// LifecycleUnits.
class InterventionPolicyDatabase {
 public:
  using InterventionPolicy = OriginInterventions::InterventionPolicy;

  // The intervention policies to use for a given origin.
  struct OriginInterventionPolicies {
    OriginInterventionPolicies(InterventionPolicy discarding_policy,
                               InterventionPolicy freezing_policy);

    InterventionPolicy discarding_policy;
    InterventionPolicy freezing_policy;
  };

  InterventionPolicyDatabase();

  InterventionPolicyDatabase(const InterventionPolicyDatabase&) = delete;
  InterventionPolicyDatabase& operator=(const InterventionPolicyDatabase&) =
      delete;

  ~InterventionPolicyDatabase();

  InterventionPolicy GetDiscardingPolicy(const url::Origin& origin) const;
  InterventionPolicy GetFreezingPolicy(const url::Origin& origin) const;

  // Initialize the database with the OriginInterventionsDatabase protobuf
  // stored in |proto_location|.
  void InitializeDatabaseWithProtoFile(const base::FilePath& proto_location,
                                       const base::Version& version,
                                       base::Value::Dict manifest);

  void AddOriginPoliciesForTesting(const url::Origin& origin,
                                   OriginInterventionPolicies policies);

 protected:
  // Map that associates the MD5 hash of an origin to its polices.
  using InterventionsMap =
      base::flat_map<std::string, OriginInterventionPolicies>;

  friend class InterventionPolicyDatabaseTest;

  const InterventionsMap& database_for_testing() { return database_; }

 private:
  // Asynchronously initialize an |InterventionsMap| object with the content of
  // the OriginInterventionsDatabase proto stored at |proto_location|.
  static InterventionsMap ReadDatabaseFromProtoFileOnSequence(
      const base::FilePath& proto_location);

  // Needs to be called to initialize |database_| with the data read in
  // InitializeDatabaseWithProtoAsync.
  void OnReadDatabaseProtoFromFile(InterventionsMap database);

  // The map that stores all the per-origin intervention policies.
  InterventionsMap database_;

  base::WeakPtrFactory<InterventionPolicyDatabase> weak_factory_{this};
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_INTERVENTION_POLICY_DATABASE_H_
