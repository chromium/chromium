// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_overrides_policy_handler.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"

namespace first_party_sets {

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
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"");
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
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error: Unknown property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsUnknownReplacementSubfields) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "primary": "https://primary.test",
            "associatedSites": ["https://associatedsite.test"],
            "unknown": "field"
          }
        ],
        "additions": []
      }
    )";

  // CheckPolicySettings will return true, but output an unknown property error.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.replacements[0]: Schema validation "
      u"error: Unknown property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsUnknownAdditionSubfields) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "primary": "https://primary.test",
            "associatedSites": ["https://associatedsite.test"],
            "unknown": "field"
          }
        ]
      }
    )";

  // CheckPolicySettings will return true, but output an unknown property error.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.additions[0]: Schema validation "
            u"error: Unknown property: unknown");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypePolicyInput) {
  policy::PolicyErrorMap errors;
  std::string input = R"( ["123", "456"] )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
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
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.replacements: Schema validation "
      u"error: Policy type mismatch: expected: \"list\", actual: \"integer\".");
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
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.additions: Schema validation error: "
      u"Policy type mismatch: expected: \"list\", actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsMissingPrimary) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "associatedSites": ["associatedsite.test"]
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.replacements[0]: Schema validation "
      u"error: Missing or invalid required property: primary");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypePrimary) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "primary": 123,
            "associatedSites": ["associatedsite.test"]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.additions[0].primary: Schema "
            u"validation error: Policy type mismatch: expected: \"string\", "
            u"actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsMissingAssociatedSites) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "primary": "primary.test"
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.replacements[0]: Schema validation "
      u"error: Missing or invalid required property: associatedSites");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_RejectsWrongTypeAssociatedSites) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "primary": "primary.test",
            "associatedSites": 123
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.additions[0].associatedSites: "
            u"Schema validation error: Policy type mismatch: expected: "
            u"\"list\", actual: \"integer\".");
}

TEST_F(
    FirstPartySetsOverridesPolicyHandlerTest,
    CheckPolicySettings_SchemaValidator_RejectsWrongTypeAssociatedSitesElement) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "primary": "primary.test",
            "associatedSites": ["associatedsite1", 123, "associatedsite2"]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at "
            u"FirstPartySetsOverrides.additions[0].associatedSites[1]: Schema "
            u"validation error: Policy type mismatch: expected: \"string\", "
            u"actual: \"integer\".");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_SchemaValidator_AcceptsSchemaStrictInput) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "primary": "https://primary1.test",
            "associatedSites": ["https://associatedsite1.test"]
          }
        ],
        "additions": [
          {
            "primary": "https://primary2.test",
            "associatedSites": ["https://associatedsite2.test"]
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
            "primary": "https://primary1.test",
            "associatedSites": ["https://associatedsite1.test"],
            "unknown1": "field1"
          }
        ],
        "additions": [
          {
            "primary": "https://primary2.test",
            "associatedSites": ["https://associatedsite2.test"],
            "unknown2": "field2"
          }
        ],
        "unknown3": "field3"
      }
    )";

  // CheckPolicySettings returns true, and errors on the last unknown property.
  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Schema validation error: Unknown property: unknown3");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsInvalidOriginPrimary) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [
          {
            "primary": "http://primary.test",
            "associatedSites": ["https://associatedsite.test"]
          }
        ],
        "additions": []
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.replacements[0].primary: Schema "
            u"validation "
            u"error: This set contains a non-HTTPS origin.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsInvalidOriginAssociatedSite) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
      {
        "replacements": [],
        "additions": [
          {
            "primary": "https://primary.test",
            "associatedSites": ["https://associatedsite1.test", ""]
          }
        ]
      }
    )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.additions[0].associatedSites[1]: "
      u"Schema validation "
      u"error: This set contains an invalid origin.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_AcceptsSingletonSet) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": []
                  }
                ],
                "additions": []
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsNonDisjointSetsSameList) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [],
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  },
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.additions[1].associatedSites[0]: "
      u"Schema validation "
      u"error: This set contains a domain that also exists in another "
      u"First-Party Set.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsNonDisjointSetsCrossList) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.additions[0].associatedSites[0]: "
      u"Schema validation "
      u"error: This set contains a domain that also exists in another "
      u"First-Party Set.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsRepeatedDomainInReplacements) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://primary1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.replacements[0].associatedSites[0]: "
      u"Schema validation "
      u"error: This set contains more than one occurrence of the same domain.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_RejectsRepeatedDomainInAdditions) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://primary2.test"]
                  }]
              }
            )";

  EXPECT_FALSE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(
      errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
      u"Error at FirstPartySetsOverrides.additions[0].associatedSites[0]: "
      u"Schema validation "
      u"error: This set contains more than one occurrence of the same domain.");
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_AcceptsAndOutputsLists_JustAdditions) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
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
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }]
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_WarnsWhenIgnoringNonCanonicalCctldKey) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associate1.test"],
                    "ccTLDs": {
                      "https://not_in_set.test": ["https://primary1.test"]
                    }
                  }
                ],
                "additions": []
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.replacements[0].ccTLDs.https://"
            u"not_in_set.test: Schema validation error: This \"ccTLDs\" entry "
            u"is ignored since this key is not in the set.");
  EXPECT_FALSE(errors.HasFatalError(policy::key::kFirstPartySetsOverrides));
}

TEST_F(FirstPartySetsOverridesPolicyHandlerTest,
       CheckPolicySettings_Handler_WarnsWhenAliasIsntCctldVariant) {
  policy::PolicyErrorMap errors;
  std::string input = R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associate1.test"],
                    "ccTLDs": {
                      "https://primary1.test": ["https://primary1-diff.cctld"]
                    }
                  }
                ],
                "additions": []
              }
            )";

  EXPECT_TRUE(
      handler()->CheckPolicySettings(MakePolicyWithInput(input), &errors));
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kFirstPartySetsOverrides),
            u"Error at FirstPartySetsOverrides.replacements[0].ccTLDs.https://"
            u"primary1.test[0]: Schema validation error: This \"ccTLD\" is "
            u"ignored since it differs from its key by more than eTLD.");
  EXPECT_FALSE(errors.HasFatalError(policy::key::kFirstPartySetsOverrides));
}

}  // namespace first_party_sets
