// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/allocation_trace.h"
#include "base/allocator/dispatcher/dispatcher.h"
#include "base/debug/stack_trace.h"

#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

using base::allocator::dispatcher::AllocationSubsystem;
using testing::AssertionResult;
using testing::Test;

namespace {

template <typename Iterator>
std::string MakeString(Iterator begin, Iterator end) {
  using value_type = decltype(*begin);
  std::ostringstream oss;
  oss << '[';
  if (begin != end) {
    auto last_element = end - 1;
    std::copy(begin, last_element, std::ostream_iterator<value_type>(oss, ","));
    oss << *last_element;
  }
  oss << ']';

  return oss.str();
}

template <typename C>
std::string MakeString(const C& data) {
  return MakeString(std::begin(data), std::end(data));
}

}  // namespace

namespace base::debug::tracer {

using base::allocator::dispatcher::AllocationSubsystem;

struct AllocationTraceRecorderTest : public Test {
  AllocationTraceRecorder& GetSubjectUnderTest() const {
    return *subject_under_test_;
  }

 protected:
  // During test, Buffer will hold a binary copy of the AllocationTraceRecorder
  // under test.
  struct Buffer {
    alignas(
        AllocationTraceRecorder) uint8_t data[sizeof(AllocationTraceRecorder)];
  };

 private:
  // The recorder under test. Depending on number and size of traces, it
  // requires quite a lot of space. Therefore, we create it on heap to avoid any
  // out-of-stack scenarios.
  std::unique_ptr<AllocationTraceRecorder> const subject_under_test_ =
      std::make_unique<AllocationTraceRecorder>();
};

TEST_F(AllocationTraceRecorderTest, VerifyIsValid) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  auto const buffer = std::make_unique<Buffer>();

  ASSERT_TRUE(buffer);

  auto* const buffered_recorder =
      reinterpret_cast<AllocationTraceRecorder*>(&(buffer->data[0]));

  // Verify IsValid returns true on the copied image.
  {
    memcpy(buffered_recorder, &subject_under_test,
           sizeof(AllocationTraceRecorder));
    EXPECT_TRUE(buffered_recorder->IsValid());
  }

  // Verify IsValid returns false when the prologue has been altered on the
  // copied image.
  {
    memcpy(buffered_recorder, &subject_under_test,
           sizeof(AllocationTraceRecorder));
    buffer->data[2] ^= 0xff;
    EXPECT_FALSE(buffered_recorder->IsValid());
  }

  // Verify IsValid returns false when the epilogue has been altered on the
  // copied image.
  {
    memcpy(buffered_recorder, &subject_under_test,
           sizeof(AllocationTraceRecorder));
    buffer->data[sizeof(AllocationTraceRecorder) - 2] ^= 0xff;
    EXPECT_FALSE(buffered_recorder->IsValid());
  }
}

TEST_F(AllocationTraceRecorderTest, VerifySingleAllocation) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  subject_under_test.OnAllocation(
      &subject_under_test, sizeof(subject_under_test),
      AllocationSubsystem::kPartitionAllocator, nullptr);

  EXPECT_EQ(1ul, subject_under_test.size());

  const auto& record_data = subject_under_test[0];
  const auto& stack_trace = record_data.GetStackTrace();

  EXPECT_EQ(OperationType::kAllocation, record_data.GetOperationType());
  EXPECT_EQ(&subject_under_test, record_data.GetAddress());
  EXPECT_EQ(sizeof(subject_under_test), record_data.GetSize());
  EXPECT_NE(nullptr, stack_trace.at(0));
}

TEST_F(AllocationTraceRecorderTest, VerifySingleFree) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  subject_under_test.OnFree(&subject_under_test);

  EXPECT_EQ(1ul, subject_under_test.size());

  const auto& record_data = subject_under_test[0];
  const auto& stack_trace = record_data.GetStackTrace();

  EXPECT_EQ(OperationType::kFree, record_data.GetOperationType());
  EXPECT_EQ(&subject_under_test, record_data.GetAddress());
  EXPECT_EQ(0ul, record_data.GetSize());
  EXPECT_NE(nullptr, stack_trace.at(0));
}

TEST_F(AllocationTraceRecorderTest, VerifyMultipleOperations) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  // We perform a number of operations.
  subject_under_test.OnAllocation(this, 1 * sizeof(*this),
                                  AllocationSubsystem::kPartitionAllocator,
                                  nullptr);

  subject_under_test.OnFree(this + 2);
  subject_under_test.OnAllocation(this + 3, 3 * sizeof(*this),
                                  AllocationSubsystem::kPartitionAllocator,
                                  nullptr);
  subject_under_test.OnAllocation(this + 4, 4 * sizeof(*this),
                                  AllocationSubsystem::kPartitionAllocator,
                                  nullptr);
  subject_under_test.OnFree(this + 5);
  subject_under_test.OnFree(this + 6);

  ASSERT_EQ(subject_under_test.size(), 6ul);

  // Verify that the stored operations match the expected.
  {
    const auto& entry = subject_under_test[0];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), this);
    ASSERT_EQ(entry.GetSize(), 1 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[1];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), (this + 2));
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[2];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), (this + 3));
    ASSERT_EQ(entry.GetSize(), 3 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[3];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), (this + 4));
    ASSERT_EQ(entry.GetSize(), 4 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[4];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), (this + 5));
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[5];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), (this + 6));
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
}

TEST_F(AllocationTraceRecorderTest, VerifyOverflowOfOperations) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  decltype(subject_under_test.GetMaximumNumberOfTraces()) idx;
  for (idx = 0; idx < subject_under_test.GetMaximumNumberOfTraces(); ++idx) {
    ASSERT_EQ(subject_under_test.size(), idx);
    const bool is_allocation = !(idx & 0x1);

    // Record an allocation or free.
    if (is_allocation) {
      subject_under_test.OnAllocation(
          this + idx, idx, AllocationSubsystem::kPartitionAllocator, nullptr);
    } else {
      subject_under_test.OnFree(this + idx);
    }

    // Some verifications.
    {
      ASSERT_EQ(subject_under_test.size(), (idx + 1));

      // Some verification on the added entry.
      {
        const auto& last_entry = subject_under_test[idx];
        ASSERT_EQ(last_entry.GetAddress(), (this + idx));
        // No full verification intended, just a check that something has been
        // written.
        ASSERT_NE(last_entry.GetStackTrace()[0], nullptr);
        if (is_allocation) {
          ASSERT_EQ(last_entry.GetOperationType(), OperationType::kAllocation);
          ASSERT_EQ(last_entry.GetSize(), idx);
        } else {
          ASSERT_EQ(last_entry.GetOperationType(), OperationType::kFree);
          ASSERT_EQ(last_entry.GetSize(), 0ul);
        }
      }

      // No changes on the first entry must be done.
      {
        const auto& first_entry = subject_under_test[0];
        ASSERT_EQ(first_entry.GetOperationType(), OperationType::kAllocation);
        ASSERT_EQ(first_entry.GetAddress(), this);
        ASSERT_EQ(first_entry.GetSize(), 0ul);
      }
    }
  }

  // By now we have written all available records including the last one.
  // So the following allocation should overwrite the first record.
  {
    const auto& old_second_entry = subject_under_test[1];

    subject_under_test.OnAllocation(
        this + idx, idx, AllocationSubsystem::kPartitionAllocator, nullptr);
    ASSERT_EQ(subject_under_test.size(),
              subject_under_test.GetMaximumNumberOfTraces());
    const auto& last_entry =
        subject_under_test[subject_under_test.GetMaximumNumberOfTraces() - 1];
    ASSERT_EQ(last_entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(last_entry.GetAddress(), (this + idx));

    // Check that the previous first entry (an allocation) is gone. Accessing
    // the first record now yields what was previously the second record (a free
    // operation).
    const auto& first_entry = subject_under_test[0];

    ASSERT_EQ(&old_second_entry, &first_entry);
    ASSERT_EQ(first_entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(first_entry.GetAddress(), (this + 1));
  }
}

TEST_F(AllocationTraceRecorderTest, VerifyWithHooks) {
  auto& dispatcher = base::allocator::dispatcher::Dispatcher::GetInstance();
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  dispatcher.InitializeForTesting(&subject_under_test);

  // Perform an allocation and free.
  std::make_unique<std::string>(
      "Just enforce an allocation and free to trigger notification of the "
      "subject under test. Hopefully this string is long enough to bypass any "
      "small string optimizations that the STL implementation might use.");

  dispatcher.ResetForTesting();

  // We only test for greater-equal since allocation from other parts of GTest
  // might have interfered.
  EXPECT_GE(subject_under_test.size(), 2ul);
}

class OperationRecordTest : public Test {
 protected:
  using ReferenceStackTrace = std::vector<const void*>;

  ReferenceStackTrace GetReferenceTrace() {
    constexpr size_t max_trace_size = 128;
    const void* frame_pointers[max_trace_size]{nullptr};
    const auto num_frames = base::debug::TraceStackFramePointers(
        &frame_pointers[0], max_trace_size, 0);
    ReferenceStackTrace trace;
    std::copy_n(std::begin(frame_pointers), num_frames,
                std::back_inserter(trace));
    return trace;
  }

  void VerifyStackTrace(
      const ReferenceStackTrace& reference_stack_trace,
      const base::debug::tracer::StackTraceContainer& stack_trace) {
    // Verify we have at least one entry in the stack.
    ASSERT_NE(nullptr, stack_trace.at(0));
    ASSERT_GT(stack_trace.size(), 0ul);

    // Although functions are marked ALWAYS_INLINE, the compiler may choose not
    // to inline, depending i.e. on the optimization level. Therefore, we search
    // for the first common frame in both stack-traces. From there on, both must
    // be equal for the remaining number of frames.
    auto* const* const it_stack_trace_begin = std::begin(stack_trace);
    auto* const* const it_stack_trace_end =
        std::find(it_stack_trace_begin, std::end(stack_trace), nullptr);
    auto const it_reference_stack_trace_end = std::end(reference_stack_trace);

    auto* const* it_stack_trace = std::find_first_of(
        it_stack_trace_begin, it_stack_trace_end,
        std::begin(reference_stack_trace), it_reference_stack_trace_end);

    ASSERT_NE(it_stack_trace, it_stack_trace_end)
        << "stack-trace and reference-stack-trace share no common frame!\n"
        << "stack trace = " << MakeString(stack_trace) << '\n'
        << "reference stack trace = " << MakeString(reference_stack_trace);

    // Find the common frame in the reference-stack-trace.
    const auto it_reference_stack_trace =
        std::find(std::begin(reference_stack_trace),
                  it_reference_stack_trace_end, *it_stack_trace);

    const auto number_of_expected_common_frames = std::min(
        std::distance(it_stack_trace, it_stack_trace_end),
        std::distance(it_reference_stack_trace, it_reference_stack_trace_end));

    // Check if we have any difference within the section of frames that we
    // expect to be equal.
    const auto mismatch = std::mismatch(
        it_reference_stack_trace,
        it_reference_stack_trace + number_of_expected_common_frames,
        it_stack_trace);

    ASSERT_EQ(mismatch.first,
              (it_reference_stack_trace + number_of_expected_common_frames))
        << "found difference in the range of frames expected to be equal!\n"
        << "position = "
        << std::distance(it_reference_stack_trace, mismatch.first) << '\n'
        << "stack trace = "
        << MakeString(it_stack_trace,
                      it_stack_trace + number_of_expected_common_frames)
        << '\n'
        << "reference stack trace = "
        << MakeString(
               it_reference_stack_trace,
               it_reference_stack_trace + number_of_expected_common_frames);
  }
};

TEST_F(OperationRecordTest, VerifyConstructor) {
  OperationRecord subject_under_test;

  EXPECT_EQ(subject_under_test.GetOperationType(), OperationType::kNone);
  EXPECT_EQ(subject_under_test.GetAddress(), nullptr);
  EXPECT_EQ(subject_under_test.GetSize(), 0ul);
  EXPECT_FALSE(subject_under_test.IsRecording());

  // The stack trace is not initialized by the constructor. Therefore, we do not
  // check here.
}

TEST_F(OperationRecordTest, VerifyRecordAllocation) {
  const ReferenceStackTrace reference_trace = GetReferenceTrace();

  void* const address = this;
  size_t const size = sizeof(*this);

  OperationRecord subject_under_test;

  ASSERT_TRUE(subject_under_test.InitializeAllocation(address, size));

  EXPECT_EQ(OperationType::kAllocation, subject_under_test.GetOperationType());
  EXPECT_EQ(address, subject_under_test.GetAddress());
  EXPECT_EQ(size, subject_under_test.GetSize());
  EXPECT_FALSE(subject_under_test.IsRecording());

  VerifyStackTrace(reference_trace, subject_under_test.GetStackTrace());
}

TEST_F(OperationRecordTest, VerifyRecordFree) {
  const ReferenceStackTrace reference_trace = GetReferenceTrace();

  void* const address = this;
  size_t const size = 0;

  OperationRecord subject_under_test;

  ASSERT_TRUE(subject_under_test.InitializeFree(address));

  EXPECT_EQ(OperationType::kFree, subject_under_test.GetOperationType());
  EXPECT_EQ(address, subject_under_test.GetAddress());
  EXPECT_EQ(size, subject_under_test.GetSize());
  EXPECT_FALSE(subject_under_test.IsRecording());

  VerifyStackTrace(reference_trace, subject_under_test.GetStackTrace());
}

}  // namespace base::debug::tracer
