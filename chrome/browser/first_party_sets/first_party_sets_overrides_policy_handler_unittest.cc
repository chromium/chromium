// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"

namespace first_party_sets {

namespace {

class FirstPartySetsOverridesPolicyHandlerTest
    : public policy::ConfigurationPolicyPrefStoreTest {
 public:
  FirstPartySetsOverridesPolicyHandlerTest() = default;

 protected:
  FirstPartySetsOverridesPolicyHandler* handler() { return handler_; }

  policy::PolicyMap MakePolicyWithInput(const std::string& input) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kFirstPartySetsOverrides,
               policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
               policy::PolicyScope::POLICY_SCOPE_MACHINE,
               policy::PolicySource::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::JSONReader::Read(input),
               /*external_data_fetcher=*/nullptr);
    return policy;
  }

 private:
  void SetUp() override {
    auto handler = std::make_unique<
        first_party_sets::FirstPartySetsOverridesPolicyHandler>(
        policy::Schema::Wrap(policy::GetChromeSchemaData()));
    handler_ = handler.get();
    handler_list_.AddHandler(std::move(handler));
  }

  raw_ptr<FirstPartySetsOverridesPolicyHandler> handler_;
};

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsMissingFields) {
  policy::PolicyErrorMap errors;
  std::string input = R"( { } )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  ASSERT_TRUE(errors.empty());
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides), u"");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsEmptyLists) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": []
      }
    )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsUnknownFields) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [],
        "unknown": "field"
      }
    )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error: Unknown property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsUnknownReplacementSubfields) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "owner": "https://owner.test",
            "members": ["https://member.test"],
            "unknown": "field"
          }
        ],
        "additions": []
      }
    )";

  // CheckPolicySettings will return true, but output an unknown property error.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements.items[0]\": Unknown "
            u"property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsUnknownAdditionSubfields) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "owner": "https://owner.test",
            "members": ["https://member.test"],
            "unknown": "field"
          }
        ]
      }
    )";

  // CheckPolicySettings will return true, but output an unknown property error.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0]\": Unknown "
            u"property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypePolicyInput) {
  policy::PolicyErrorMap errors;
  std::string input = R"( ["123", "456"] )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error: Policy type mismatch: "
            u"expected: \"dictionary\", actual: \"list\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_ChecksReplacementsFieldType) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": 123,
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements\": Policy type "
            u"mismatch: expected: \"list\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_ChecksAdditionsFieldType) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": 123
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions\": Policy type mismatch: "
            u"expected: \"list\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsMissingOwner) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "members": ["member.test"]
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements.items[0]\": Missing or "
            u"invalid required property: owner");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypeOwner) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "owner": 123,
            "members": ["member.test"]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0].owner\": Policy "
            u"type mismatch: expected: \"string\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsMissingMembers) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "owner": "owner.test"
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements.items[0]\": Missing or "
            u"invalid required property: members");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypeMembers) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "owner": "owner.test",
            "members": 123
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0].members\": "
            u"Policy type mismatch: expected: \"list\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypeMembersElement) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "owner": "owner.test",
            "members": ["member1", 123, "member2"]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrors(policy::key::kFirstPartySetsOverrides),
      u"Schema validation error at \"additions.items[0].members.items[1]\": "
      u"Policy type mismatch: expected: \"string\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsSchemaStrictInput) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "owner": "https://owner1.test",
            "members": ["https://member1.test"]
          }
        ],
        "additions": [
          {
            "owner": "https://owner2.test",
            "members": ["https://member2.test"]
          }
        ]
      }
    )";
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsSchemaAllowUnknownInput) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "unknown0": "field0",
        "replacements": [
          {
            "owner": "https://owner1.test",
            "members": ["https://member1.test"],
            "unknown1": "field1"
          }
        ],
        "additions": [
          {
            "owner": "https://owner2.test",
            "members": ["https://member2.test"],
            "unknown2": "field2"
          }
        ],
        "unknown3": "field3"
      }
    )";

  // CheckPolicySettings returns true, and errors on the last unknown property.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error: Unknown property: unknown3");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsInvalidOriginOwner) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "owner": "http://owner.test",
            "members": ["https://member.test"]
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements.items[0]\": This set "
            u"contains an invalid origin.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsInvalidOriginMember) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "owner": "https://owner.test",
            "members": ["https://member1.test", ""]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0]\": This set "
            u"contains an invalid origin.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsSingletonSet) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": []
                  }
                ],
                "additions": []
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrors(policy::key::kFirstPartySetsOverrides),
      u"Schema validation error at \"replacements.items[0]\": This set doesn't "
      u"contain any sites in its members list.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsNonDisjointSetsSameList) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [],
                "additions": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  },
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member1.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[1]\": This set "
            u"contains a domain that also exists in another First-Party Set.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsNonDisjointSetsCrossList) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member1.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0]\": This set "
            u"contains a domain that also exists in another First-Party Set.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsRepeatedDomainInReplacements) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://owner1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"replacements.items[0]\": This set "
            u"contains more than one occurrence of the same domain.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsRepeatedDomainInAdditions) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://owner2.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrors(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error at \"additions.items[0]\": This set "
            u"contains more than one occurrence of the same domain.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_AcceptsAndOutputsLists_JustAdditions) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "additions": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }]
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_AcceptsAndOutputsLists_JustReplacements) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ]
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(
    FirstPartySetsOverridesPolicyHandlerTest,
    CheckPolicySettings_Handler_AcceptsAndOutputsLists_AdditionsAndReplacements) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "owner": "https://owner1.test",
                    "members": ["https://member1.test"]
                  }
                ],
                "additions": [
                  {
                    "owner": "https://owner2.test",
                    "members": ["https://member2.test"]
                  }]
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

}  // namespace

}  // namespace first_party_sets
