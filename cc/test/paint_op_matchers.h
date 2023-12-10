// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PAINT_OP_MATCHERS_H_
#define CC_TEST_PAINT_OP_MATCHERS_H_

#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include <optional>
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/paint_op_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cc {

// Matcher checking that a PaintOp is equal to a PaintOp constructed with the
// provided parameters.
// Example use:
//   PaintOpBuffer buffer = ...;
//   EXPECT_THAT(buffer,
//               ElementsAre(PaintOpEq<SaveOp>(),
//                           PaintOpEq<SetMatrixOp>(SkM44::Scale(1, 2))));
template <typename OpT>
class PaintOpEq {
 public:
  using is_gtest_matcher = void;

  template <typename... Args>
  explicit PaintOpEq(Args&&... args)
      : expected_op_(base::MakeRefCounted<base::RefCountedData<OpT>>(
            std::in_place,
            std::forward<Args>(args)...)) {}

  bool MatchAndExplain(const PaintOp& op,
                       testing::MatchResultListener* listener) const {
    if (!op.EqualsForTesting(expected_op_->data)) {
      if (listener->IsInterested()) {
        *listener->stream()
            << "\n    Expected: " << PaintOpHelper::ToString(expected_op_->data)
            << "\n    Actual:   " << PaintOpHelper::ToString(op);
      }
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is equal to " << PaintOpHelper::ToString(expected_op_->data);
  }
  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't equal to " << PaintOpHelper::ToString(expected_op_->data);
  }

 private:
  scoped_refptr<base::RefCountedData<OpT>> expected_op_;
};

// Matcher testing whether that a PaintOp is of the specified type, irrespective
// of it's specific value.
//
// Example use:
//   PaintOpBuffer buffer = ...;
//   EXPECT_THAT(buffer,
//               ElementsAre(PaintOpIs<SaveOp>(), PaintOpIs<SetMatrixOp>()));
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
    *os << "isn't a valid " << PaintOpTypeToString(OpT::kType) << " paint op";
  }
};

}  // namespace cc

#endif  // CC_TEST_PAINT_OP_MATCHERS_H_
