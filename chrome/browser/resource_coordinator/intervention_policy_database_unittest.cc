// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/intervention_policy_database.h"

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/resource_coordinator/intervention_policy_database.pb.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

namespace {

using InterventionPolicy = InterventionPolicyDatabase::InterventionPolicy;
using OriginInterventionPolicies =
    InterventionPolicyDatabase::OriginInterventionPolicies;

// Initialize a protobuf in |path| with the content of |data_map|.
void WriteProtoToFile(
    const base::FilePath& path,
    std::map<url::Origin, OriginInterventionPolicies> data_map) {
  OriginInterventionsDatabase proto;

  for (const auto& iter : data_map) {
    OriginInterventions* origin_interventions =
        proto.add_origin_interventions();
    EXPECT_TRUE(origin_interventions);
    origin_interventions->set_host_hash(
        SerializeOriginIntoDatabaseKey(iter.first));
    origin_interventions->set_discarding_policy(iter.second.discarding_policy);
    origin_interventions->set_freezing_policy(iter.second.freezing_policy);
  }
  std::string serialized_proto;
  EXPECT_TRUE(proto.SerializeToString(&serialized_proto));
  EXPECT_TRUE(base::WriteFile(path, serialized_proto));
}

}  // namespace

class InterventionPolicyDatabaseTest : public ::testing::Test {
 protected:
  InterventionPolicyDatabaseTest() = default;

  void WaitForDatabaseToBeInitialized() {
    while (intervention_policy_database_.database_for_testing().empty())
      test_env_.RunUntilIdle();
  }

  InterventionPolicyDatabase* GetDatabase() {
    return &intervention_policy_database_;
  }

 private:
  base::test::TaskEnvironment test_env_;
  InterventionPolicyDatabase intervention_policy_database_;
};

TEST_F(InterventionPolicyDatabaseTest, EndToEnd) {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath proto_path;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &proto_path));

  std::map<url::Origin, OriginInterventionPolicies> policy_map;
  policy_map.emplace(url::Origin::Create(GURL("https://a.com")),
                     OriginInterventionPolicies(OriginInterventions::OPT_IN,
                                                OriginInterventions::OPT_IN));
  policy_map.emplace(url::Origin::Create(GURL("https://b.com")),
                     OriginInterventionPolicies(OriginInterventions::OPT_IN,
                                                OriginInterventions::OPT_OUT));
  policy_map.emplace(url::Origin::Create(GURL("https://c.com")),
                     OriginInterventionPolicies(OriginInterventions::OPT_OUT,
                                                OriginInterventions::OPT_OUT));
  policy_map.emplace(url::Origin::Create(GURL("https://d.com")),
                     OriginInterventionPolicies(OriginInterventions::OPT_IN,
                                                OriginInterventions::DEFAULT));
  WriteProtoToFile(proto_path, policy_map);

  GetDatabase()->InitializeDatabaseWithProtoFile(proto_path, base::Version(),
                                                 base::Value::Dict());

  WaitForDatabaseToBeInitialized();

  for (const auto& iter : policy_map) {
    EXPECT_EQ(iter.second.discarding_policy,
              GetDatabase()->GetDiscardingPolicy(iter.first));
    EXPECT_EQ(iter.second.freezing_policy,
              GetDatabase()->GetFreezingPolicy(iter.first));
  }
}

}  // namespace resource_coordinator
