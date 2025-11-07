// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

namespace {

class AttemptFormFillingToolRequestTest : public testing::Test {
 public:
  AttemptFormFillingToolRequestTest() = default;
  ~AttemptFormFillingToolRequestTest() override = default;

  // Helper to create a dummy tab handle.
  tabs::TabHandle GetDummyTabHandle() { return tabs::TabHandle(1); }
};

TEST_F(AttemptFormFillingToolRequestTest, NameReturnsCorrectString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  // The string returned by Name() is used in histograms.xml to define the
  // ToolRequest variants for metrics collection.
  EXPECT_EQ(tool_request.Name(), "AttemptFormFilling");
}

TEST_F(AttemptFormFillingToolRequestTest, JournalEventString) {
  AttemptFormFillingToolRequest tool_request(GetDummyTabHandle(),
                                             /*requests=*/{});

  EXPECT_EQ("AttemptFormFilling", tool_request.JournalEvent());
}

TEST_F(AttemptFormFillingToolRequestTest, FormFillingRequestInJournalDetails) {
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests;
  AttemptFormFillingToolRequest::FormFillingRequest request1;
  request1.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kAddress;
  request1.trigger_fields.emplace_back(
      DomNode{.node_id = 123, .document_identifier = "doc_id_abc"});
  requests.push_back(request1);
  AttemptFormFillingToolRequest::FormFillingRequest request2;
  request2.requested_data =
      AttemptFormFillingToolRequest::RequestedData::kCreditCard;
  request2.trigger_fields.emplace_back(
      DomNode{.node_id = 456, .document_identifier = "doc_id_abc"});
  request2.trigger_fields.emplace_back(gfx::Point(1024, 512));
  requests.push_back(request2);

  std::vector<mojom::JournalDetailsPtr> journal_details =
      JournalDetailsBuilder().Add("requests", requests).Build();
  EXPECT_EQ(
      "[Request(1, DomNode[id=123 doc_id=doc_id_abc]), "
      "Request(6, DomNode[id=456 doc_id=doc_id_abc], Point(1024,512))]",
      journal_details[0]->value);
}

}  // namespace
}  // namespace actor
