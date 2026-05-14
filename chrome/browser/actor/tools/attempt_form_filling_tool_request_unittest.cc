// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/shared_types.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

namespace {

using ::testing::IsEmpty;
using ::testing::SizeIs;

struct TestParams {
  bool is_section_label_enabled;
  std::optional<std::string> provided_label;
  std::string expected_label;
};

class AttemptFormFillingToolRequestTest
    : public testing::Test,
      public ::testing::WithParamInterface<TestParams> {
 public:
  AttemptFormFillingToolRequestTest() {
    feature_list_.InitWithFeatureStates({
        {features::kGlicActorAutofill, true},
        {features::kGlicActorAutofillSectionLabel,
         GetParam().is_section_label_enabled},
    });
  }
  ~AttemptFormFillingToolRequestTest() override = default;

  // Helper to create a dummy tab handle.
  tabs::TabHandle GetDummyTabHandle() { return tabs::TabHandle(1); }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AttemptFormFillingToolRequestTest, NameReturnsCorrectString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  // The string returned by Name() is used in histograms.xml to define the
  // ToolRequest variants for metrics collection.
  EXPECT_EQ(tool_request.Name(), AttemptFormFillingToolRequest::kName);
}

TEST_P(AttemptFormFillingToolRequestTest, JournalEventString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  EXPECT_EQ("AttemptFormFilling", tool_request.JournalEvent());
}

TEST_P(AttemptFormFillingToolRequestTest, FormFillingRequestInJournalDetails) {
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests;
  AttemptFormFillingToolRequest::FormFillingRequest request1;
  request1.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kAddress;
  request1.section_label = GetParam().expected_label;
  request1.trigger_fields.emplace_back(
      DomNode{.node_id = 123, .document_identifier = "doc_id_abc"});
  requests.push_back(request1);
  AttemptFormFillingToolRequest::FormFillingRequest request2;
  request2.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kCreditCard;
  request2.section_label = "Section 2";
  request2.trigger_fields.emplace_back(
      DomNode{.node_id = 456, .document_identifier = "doc_id_abc"});
  request2.trigger_fields.emplace_back(gfx::Point(1024, 512));
  requests.push_back(request2);

  std::vector<mojom::JournalDetailsPtr> journal_details =
      JournalDetailsBuilder().Add("requests", requests).Build();
  EXPECT_EQ(
      base::StrCat({"[Request(1, section_label=", GetParam().expected_label,
                    ", DomNode[id=123 doc_id=doc_id_abc]), "
                    "Request(6, section_label=Section 2, DomNode[id=456 "
                    "doc_id=doc_id_abc], Point(1024,512))]"}),
      journal_details[0]->value);
}

TEST_P(AttemptFormFillingToolRequestTest, ReadFromProto) {
  optimization_guide::proto::Actions actions_proto;
  auto* action_proto = actions_proto.add_actions();
  auto* form_filling_proto = action_proto->mutable_attempt_form_filling();
  form_filling_proto->set_tab_id(1);
  auto* request_proto = form_filling_proto->add_form_filling_requests();
  request_proto->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  if (GetParam().provided_label.has_value()) {
    request_proto->set_section_label(*GetParam().provided_label);
  }
  auto* trigger_field = request_proto->add_trigger_fields();
  trigger_field->set_content_node_id(123);
  trigger_field->mutable_document_identifier()->set_serialized_token("doc1");

  BuildToolRequestResult result = BuildToolRequest(actions_proto);
  ASSERT_TRUE(result.has_value());
  ASSERT_THAT(result.value(), SizeIs(1));

  ToolRequest& created_request = *result.value().front();
  ASSERT_EQ(AttemptFormFillingToolRequest::kName, created_request.Name());

  AttemptFormFillingToolRequest& form_filling_request =
      static_cast<AttemptFormFillingToolRequest&>(created_request);
  ASSERT_THAT(form_filling_request.GetRequestsForTesting(), SizeIs(1));

  EXPECT_EQ(form_filling_request.GetRequestsForTesting()[0].section_label,
            GetParam().expected_label);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AttemptFormFillingToolRequestTest,
    ::testing::Values(TestParams{.is_section_label_enabled = true,
                                 .provided_label = "Test Section",
                                 .expected_label = "Test Section"},
                      TestParams{.is_section_label_enabled = false,
                                 .provided_label = "Test Section",
                                 .expected_label = ""},
                      TestParams{.is_section_label_enabled = true,
                                 .provided_label = std::nullopt,
                                 .expected_label = ""}));

}  // namespace
}  // namespace actor
