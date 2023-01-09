// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_OP_MATCHERS_H_
#define CC_TEST_PAINT_OP_MATCHERS_H_

#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/paint_op_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

// Matcher checking that two PaintOp objects are equal.
// Example use:
//   PaintOpBuffer buffer = ...;
//   EXPECT_THAT(buffer,
//               ElementsAre(PaintOpEq(SaveOp()),
//                           PaintOpEq(SetMatrixOp(SkM44::Scale(1, 2)))));
MATCHER_P(PaintOpEq,
          op,
          base::StringPrintf("%s equal to %s",
                             negation ? "isn't" : "is",
                             PaintOpHelper::ToString(&op).c_str())) {
  *result_listener << "\n    Expected: " << PaintOpHelper::ToString(&op)
                   << "\n    Actual: " << PaintOpHelper::ToString(&arg);
  const PaintOp& op_ref = op;  // To unpack std::reference_wrapper, if needed.
  return op_ref == arg;
}

// Matcher checking that a PaintOpBuffer contains the specified PaintOps.
// This is a shorthand for ElementsAre, when all arguments are PaintOpEq. For
// instance, these two expectations are equivalent:
//
//   EXPECT_THAT(buffer,
//               ElementsAre(PaintOpEq(SaveOp()),
//                           PaintOpEq(SetMatrixOp(SkM44::Scale(1, 2)))));
//   EXPECT_THAT(buffer,
//               PaintOpsAreEq(SaveOp(), SetMatrixOp(SkM44::Scale(1, 2))));
template <typename... OpType>
auto PaintOpsAreEq(OpType... op) {
  return testing::ElementsAre(PaintOpEq(std::move(op))...);
}

// Matcher testing whether that a PaintOp is of the specified type, irrespective
// of it's specific value.
//
// Example use:
//   PaintOpBuffer buffer = ...;
//   EXPECT_THAT(buffer,
//               ElementsAre(PaintOpIs<SaveOp>(), PaintOpIs<SetMatrixOp()));
template <typename OpT>
class PaintOpIs {
 public:
  using is_gtest_matcher = void;

  bool MatchAndExplain(const PaintOp& op,
                       testing::MatchResultListener* listener) const {
    if (op.GetType() != OpT::kType) {
      if (listener->IsInterested()) {
        *listener->stream()
            << "Unexpected PaintOp type. Expected: "
            << PaintOpTypeToString(OpT::kType)
            << ", Actual: " << PaintOpTypeToString(op.GetType());
      }
      return false;
    }
    if (!static_cast<const OpT&>(op).IsValid()) {
      if (listener->IsInterested()) {
        *listener->stream() << "PaintOp is not valid";
      }
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is a valid " << PaintOpTypeToString(OpT::kType) << " paint op";
  }
  void DescribeNegationTo(std::ostream* os) const {
    *os << "is't a valid " << PaintOpTypeToString(OpT::kType) << " paint op";
  }
};

}  // namespace cc

#endif  // CC_TEST_PAINT_OP_MATCHERS_H_
