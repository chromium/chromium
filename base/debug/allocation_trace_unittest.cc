// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/allocation_trace.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/testing/tools.h"
#include "base/debug/stack_trace.h"
#include "partition_alloc/partition_alloc_allocation_data.h"
#include "partition_alloc/partition_alloc_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::allocator::dispatcher::AllocationNotificationData;
using base::allocator::dispatcher::AllocationSubsystem;
using base::allocator::dispatcher::FreeNotificationData;
using base::allocator::dispatcher::MTEMode;
using testing::Combine;
using testing::ContainerEq;
using testing::Message;
using testing::Test;
using testing::Values;

namespace base::debug::tracer {
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

void AreEqual(const base::debug::tracer::OperationRecord& expected,
              const base::debug::tracer::OperationRecord& is) {
  EXPECT_EQ(is.GetOperationType(), expected.GetOperationType());
  EXPECT_EQ(is.GetAddress(), expected.GetAddress());
  EXPECT_EQ(is.GetSize(), expected.GetSize());
  EXPECT_THAT(is.GetStackTrace(), ContainerEq(expected.GetStackTrace()));
}

}  // namespace

class AllocationTraceRecorderTest : public Test {
 protected:
  AllocationTraceRecorder& GetSubjectUnderTest() const {
    return *subject_under_test_;
  }
  // During test, Buffer will hold a binary copy of the AllocationTraceRecorder
  // under test.
  struct Buffer {
    alignas(AllocationTraceRecorder)
        std::array<uint8_t, sizeof(AllocationTraceRecorder)> data;
  };

 protected:
  AllocationNotificationData CreateAllocationData(
      void* address,
      size_t size,
      MTEMode mte_mode = MTEMode::kUndefined) {
    return AllocationNotificationData(address, size, nullptr,
                                      AllocationSubsystem::kPartitionAllocator)
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
        .SetMteReportingMode(mte_mode)
#endif
        ;
  }

  FreeNotificationData CreateFreeData(void* address,
                                      MTEMode mte_mode = MTEMode::kUndefined) {
    return FreeNotificationData(address,
                                AllocationSubsystem::kPartitionAllocator)
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
        .SetMteReportingMode(mte_mode)
#endif
        ;
  }

 private:
  // The recorder under test. Depending on number and size of traces, it
  // requires quite a lot of space. Therefore, we create it on heap to avoid any
  // out-of-stack scenarios.
  std::unique_ptr<AllocationTraceRecorder> const subject_under_test_ =
      std::make_unique<AllocationTraceRecorder>();
};

TEST_F(AllocationTraceRecorderTest, VerifyBinaryCopy) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  // Fill the recorder with some fake allocations and frees.
  constexpr size_t number_of_records = 100;

  for (size_t index = 0; index < number_of_records; ++index) {
    if (index & 0x1) {
      subject_under_test.OnAllocation(
          CreateAllocationData(this, sizeof(*this)));
    } else {
      subject_under_test.OnFree(CreateFreeData(this));
    }
  }

  ASSERT_EQ(number_of_records, subject_under_test.size());

  // Create a copy of the recorder using buffer as storage for the copy.
  auto const buffer = std::make_unique<Buffer>();

  ASSERT_TRUE(buffer);

  AllocationTraceRecorder* const buffered_recorder =
      new (buffer->data.data()) AllocationTraceRecorder();

  static_assert(std::is_trivially_copyable_v<AllocationTraceRecorder>);
  base::byte_span_from_ref(*buffered_recorder)
      .copy_from(base::byte_span_from_ref(subject_under_test));

  // Verify that the original recorder and the buffered recorder are equal.
  ASSERT_EQ(subject_under_test.size(), buffered_recorder->size());

  for (size_t index = 0; index < subject_under_test.size(); ++index) {
    SCOPED_TRACE(Message("difference detected at index ") << index);
    AreEqual(subject_under_test[index], (*buffered_recorder)[index]);
  }

  buffered_recorder->~AllocationTraceRecorder();
}

TEST_F(AllocationTraceRecorderTest, VerifySingleAllocation) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  subject_under_test.OnAllocation(
      CreateAllocationData(&subject_under_test, sizeof(subject_under_test)));

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

  subject_under_test.OnFree(CreateFreeData(&subject_under_test));

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

  // Some (valid) pointers to use in the allocation operations.
  std::vector<uint8_t> addrs_buf(sizeof(*this) * 7u);
  uint8_t* addr0 = &addrs_buf[0u * sizeof(*this)];
  // uint8_t* addr1 = &addrs_buf[1u * sizeof(*this)];
  uint8_t* addr2 = &addrs_buf[2u * sizeof(*this)];
  uint8_t* addr3 = &addrs_buf[3u * sizeof(*this)];
  uint8_t* addr4 = &addrs_buf[4u * sizeof(*this)];
  uint8_t* addr5 = &addrs_buf[5u * sizeof(*this)];
  uint8_t* addr6 = &addrs_buf[6u * sizeof(*this)];

  // We perform a number of operations.
  subject_under_test.OnAllocation(
      CreateAllocationData(addr0, 1 * sizeof(*this)));
  subject_under_test.OnFree(CreateFreeData(addr2));
  subject_under_test.OnAllocation(
      CreateAllocationData(addr3, 3 * sizeof(*this)));
  subject_under_test.OnAllocation(
      CreateAllocationData(addr4, 4 * sizeof(*this)));
  subject_under_test.OnFree(CreateFreeData(addr5));
  subject_under_test.OnFree(CreateFreeData(addr6));

  ASSERT_EQ(subject_under_test.size(), 6ul);

  // Verify that the stored operations match the expected.
  {
    const auto& entry = subject_under_test[0];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), addr0);
    ASSERT_EQ(entry.GetSize(), 1 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[1];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), addr2);
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[2];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), addr3);
    ASSERT_EQ(entry.GetSize(), 3 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[3];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(entry.GetAddress(), addr4);
    ASSERT_EQ(entry.GetSize(), 4 * sizeof(*this));
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[4];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), addr5);
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
  {
    const auto& entry = subject_under_test[5];
    ASSERT_EQ(entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(entry.GetAddress(), addr6);
    ASSERT_EQ(entry.GetSize(), 0ul);
    ASSERT_NE(entry.GetStackTrace()[0], nullptr);
  }
}

TEST_F(AllocationTraceRecorderTest, VerifyOverflowOfOperations) {
  AllocationTraceRecorder& subject_under_test = GetSubjectUnderTest();

  auto num_traces = subject_under_test.GetMaximumNumberOfTraces();

  // Some (valid) pointers to use in the allocation operations.
  std::vector<uint8_t> addrs_buf(sizeof(*this) * (num_traces + 1));
  auto addr = [&](auto idx) { return &addrs_buf[idx * sizeof(*this)]; };

  decltype(num_traces) idx;
  for (idx = 0; idx < subject_under_test.GetMaximumNumberOfTraces(); ++idx) {
    ASSERT_EQ(subject_under_test.size(), idx);
    const bool is_allocation = !(idx & 0x1);

    // Record an allocation or free.
    if (is_allocation) {
      subject_under_test.OnAllocation(CreateAllocationData(addr(idx), idx));
    } else {
      subject_under_test.OnFree(CreateFreeData(addr(idx)));
    }

    // Some verifications.
    {
      ASSERT_EQ(subject_under_test.size(), (idx + 1));

      // Some verification on the added entry.
      {
        const auto& last_entry = subject_under_test[idx];
        ASSERT_EQ(last_entry.GetAddress(), addr(idx));
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
        ASSERT_EQ(first_entry.GetAddress(), addr(0));
        ASSERT_EQ(first_entry.GetSize(), 0ul);
      }
    }
  }

  // By now we have written all available records including the last one.
  // So the following allocation should overwrite the first record.
  {
    const auto& old_second_entry = subject_under_test[1];

    subject_under_test.OnAllocation(CreateAllocationData(addr(idx), idx));
    ASSERT_EQ(subject_under_test.size(),
              subject_under_test.GetMaximumNumberOfTraces());
    const auto& last_entry =
        subject_under_test[subject_under_test.GetMaximumNumberOfTraces() - 1];
    ASSERT_EQ(last_entry.GetOperationType(), OperationType::kAllocation);
    ASSERT_EQ(last_entry.GetAddress(), addr(idx));

    // Check that the previous first entry (an allocation) is gone. Accessing
    // the first record now yields what was previously the second record (a free
    // operation).
    const auto& first_entry = subject_under_test[0];

    ASSERT_EQ(&old_second_entry, &first_entry);
    ASSERT_EQ(first_entry.GetOperationType(), OperationType::kFree);
    ASSERT_EQ(first_entry.GetAddress(), addr(1));
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
    ReferenceStackTrace frame_pointers(128);
    const auto num_frames =
        base::debug::TraceStackFramePointers(frame_pointers, 0);
    frame_pointers.resize(num_frames);
    frame_pointers.shrink_to_fit();
    return frame_pointers;
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
    auto const it_stack_trace_begin = std::begin(stack_trace);
    auto const it_stack_trace_end =
        std::find(it_stack_trace_begin, std::end(stack_trace), nullptr);
    auto const it_reference_stack_trace_end = std::end(reference_stack_trace);

    auto const it_stack_trace = std::find_first_of(
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
