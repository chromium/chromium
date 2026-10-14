// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_timing_buffer.h"

#include <array>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ScrollTimingBufferTest : public testing::Test {
 protected:
  void SetUp() override {
    buffer_ = ScrollTimingBuffer::Create();
    ASSERT_TRUE(buffer_);
    consumer_ =
        ScrollTimingBufferConsumer::Create(buffer_->DuplicateReadOnlyRegion());
    ASSERT_TRUE(consumer_);
  }

  static ScrollTimingInfo Record(uint64_t index) {
    return {
        .start_time = base::TimeTicks() + base::Microseconds(index),
        .end_time = base::TimeTicks() + base::Microseconds(index + 1),
        .input_type = ui::ScrollInputType::kWheel,
        .element_id = ElementId(index),
    };
  }

  std::unique_ptr<ScrollTimingBuffer> buffer_;
  std::unique_ptr<ScrollTimingBufferConsumer> consumer_;
};

TEST_F(ScrollTimingBufferTest, CapacityOverwritesOldest) {
  constexpr uint32_t kCapacity = ScrollTimingBuffer::kCapacity;
  for (const uint32_t count :
       {0u, kCapacity - 1, kCapacity, kCapacity + 1, kCapacity + 10}) {
    SCOPED_TRACE(count);
    buffer_ = ScrollTimingBuffer::Create();
    ASSERT_TRUE(buffer_);
    consumer_ =
        ScrollTimingBufferConsumer::Create(buffer_->DuplicateReadOnlyRegion());
    ASSERT_TRUE(consumer_);
    std::vector<ScrollTimingInfo> expected;
    for (uint32_t i = 1; i <= count; ++i) {
      buffer_->Push(Record(i));
      if (i + kCapacity > count) {
        expected.push_back(Record(i));
      }
    }
    const auto snapshot = consumer_->ReadRecords();
    EXPECT_EQ(snapshot.records, expected);
    EXPECT_EQ(snapshot.overwritten_count, count - expected.size());
    const auto empty = consumer_->ReadRecords();
    EXPECT_THAT(empty.records, testing::IsEmpty());
    EXPECT_EQ(empty.overwritten_count, 0u);
  }
}

TEST_F(ScrollTimingBufferTest, WrapsAround) {
  // One less than the capacity, so each round starts at a different slot.
  constexpr uint32_t kRecordsPerRound = ScrollTimingBuffer::kCapacity - 1;
  for (uint32_t round = 0; round < 20; ++round) {
    std::vector<ScrollTimingInfo> expected;
    for (uint32_t i = 1; i <= kRecordsPerRound; ++i) {
      expected.push_back(Record(round * kRecordsPerRound + i));
      buffer_->Push(expected.back());
    }
    const auto snapshot = consumer_->ReadRecords();
    EXPECT_EQ(snapshot.records, expected);
    EXPECT_EQ(snapshot.overwritten_count, 0u);
  }
}

TEST_F(ScrollTimingBufferTest, CountsRecordsOverwrittenSincePreviousRead) {
  constexpr uint32_t kCapacity = ScrollTimingBuffer::kCapacity;
  for (uint32_t i = 1; i <= 10; ++i) {
    buffer_->Push(Record(i));
  }
  EXPECT_THAT(consumer_->ReadRecords().records, testing::SizeIs(10));
  std::vector<ScrollTimingInfo> expected;
  // Overwrites records 11 to 15 before they're read.
  for (uint32_t i = 11; i <= kCapacity + 15; ++i) {
    buffer_->Push(Record(i));
    if (i > 15) {
      expected.push_back(Record(i));
    }
  }
  const auto snapshot = consumer_->ReadRecords();
  EXPECT_EQ(snapshot.records, expected);
  EXPECT_EQ(snapshot.overwritten_count, 5u);
  const auto empty = consumer_->ReadRecords();
  EXPECT_THAT(empty.records, testing::IsEmpty());
  EXPECT_EQ(empty.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest, ConcurrentPushAndReadRecords) {
  constexpr uint64_t kRecords = 20000;
  base::TestWaitableEvent start;
  std::atomic<bool> finished{false};
  base::Thread producer("ScrollTimingProducer");
  ASSERT_TRUE(producer.Start());
  producer.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&start, &finished, buffer = std::move(buffer_)] {
                       start.Wait();
                       for (uint64_t i = 1; i <= kRecords; ++i) {
                         buffer->Push(Record(i));
                       }
                       finished.store(true, std::memory_order_release);
                     }));
  std::vector<ScrollTimingInfo> consumed;
  uint64_t overwritten_count = 0;
  const auto read_records = [&] {
    const auto snapshot = consumer_->ReadRecords();
    EXPECT_LE(snapshot.records.size(), ScrollTimingBuffer::kCapacity);
    consumed.insert(consumed.end(), snapshot.records.begin(),
                    snapshot.records.end());
    overwritten_count += snapshot.overwritten_count;
  };
  start.Signal();
  do {
    read_records();
    base::PlatformThread::YieldCurrentThread();
  } while (!finished.load(std::memory_order_acquire));
  producer.Stop();
  read_records();
  EXPECT_EQ(consumed.size() + overwritten_count, kRecords);
  // Records must be intact, oldest-first, and end with the last push.
  uint64_t previous_index = 0;
  for (const auto& record : consumed) {
    const uint64_t index = record.element_id.GetInternalValue();
    ASSERT_GT(index, previous_index);
    ASSERT_EQ(record, Record(index));
    previous_index = index;
  }
  EXPECT_EQ(previous_index, kRecords);
}

TEST_F(ScrollTimingBufferTest,
       ConsumerKeepsMappingAliveAfterProducerDestruction) {
  const auto record = Record(1);
  buffer_->Push(record);
  buffer_ = nullptr;
  const auto snapshot = consumer_->ReadRecords();
  EXPECT_THAT(snapshot.records, testing::ElementsAre(record));
  EXPECT_EQ(snapshot.overwritten_count, 0u);
  EXPECT_THAT(consumer_->ReadRecords().records, testing::IsEmpty());
}

TEST_F(ScrollTimingBufferTest, ProducerKeepsStorageAlive) {
  buffer_->Push(Record(1));
  EXPECT_THAT(consumer_->ReadRecords().records,
              testing::ElementsAre(Record(1)));
  consumer_.reset();
  buffer_->Push(Record(2));
  consumer_ =
      ScrollTimingBufferConsumer::Create(buffer_->DuplicateReadOnlyRegion());
  ASSERT_TRUE(consumer_);
  const auto snapshot = consumer_->ReadRecords();
  EXPECT_THAT(snapshot.records, testing::ElementsAre(Record(1), Record(2)));
  EXPECT_EQ(snapshot.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest,
       SnapshotSurvivesSlotReuseAndEndpointDestruction) {
  std::vector<ScrollTimingInfo> expected;
  for (uint32_t i = 1; i <= ScrollTimingBuffer::kCapacity; ++i) {
    expected.push_back(Record(i));
    buffer_->Push(expected.back());
  }
  const auto snapshot = consumer_->ReadRecords();
  for (uint32_t i = 1; i <= ScrollTimingBuffer::kCapacity; ++i) {
    buffer_->Push(Record(ScrollTimingBuffer::kCapacity + i));
  }
  consumer_.reset();
  buffer_ = nullptr;
  EXPECT_EQ(snapshot.records, expected);
  EXPECT_EQ(snapshot.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest, ConsumersHaveIndependentReadPositions) {
  const auto other_consumer =
      ScrollTimingBufferConsumer::Create(buffer_->DuplicateReadOnlyRegion());
  ASSERT_TRUE(other_consumer);
  buffer_->Push(Record(1));
  EXPECT_THAT(consumer_->ReadRecords().records,
              testing::ElementsAre(Record(1)));
  buffer_->Push(Record(2));
  EXPECT_THAT(consumer_->ReadRecords().records,
              testing::ElementsAre(Record(2)));
  const auto snapshot = other_consumer->ReadRecords();
  EXPECT_THAT(snapshot.records, testing::ElementsAre(Record(1), Record(2)));
  EXPECT_EQ(snapshot.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest, ConsumerRejectsInvalidRegion) {
  EXPECT_FALSE(
      ScrollTimingBufferConsumer::Create(base::ReadOnlySharedMemoryRegion()));
}

TEST_F(ScrollTimingBufferTest, RejectsRegressingWriteCount) {
  auto storage =
      base::StructuredSharedMemory<ScrollTimingBuffer::SharedState>::Create();
  ASSERT_TRUE(storage);
  consumer_ =
      ScrollTimingBufferConsumer::Create(storage->DuplicateReadOnlyRegion());
  ASSERT_TRUE(consumer_);
  auto& state = storage->WritableRef();
  state.slots[0].record = Record(1);
  state.slots[0].sequence.store(2, std::memory_order_relaxed);
  state.write_count.store(1, std::memory_order_relaxed);
  EXPECT_THAT(consumer_->ReadRecords().records,
              testing::ElementsAre(Record(1)));

  state.write_count.store(0, std::memory_order_relaxed);
  const auto regressed = consumer_->ReadRecords();
  EXPECT_THAT(regressed.records, testing::IsEmpty());
  EXPECT_EQ(regressed.overwritten_count, 0u);

  state.write_count.store(1, std::memory_order_relaxed);
  const auto restored = consumer_->ReadRecords();
  EXPECT_THAT(restored.records, testing::IsEmpty());
  EXPECT_EQ(restored.overwritten_count, 0u);

  state.slots[1].record = Record(2);
  state.slots[1].sequence.store(4, std::memory_order_relaxed);
  state.write_count.store(2, std::memory_order_relaxed);
  const auto next = consumer_->ReadRecords();
  EXPECT_THAT(next.records, testing::ElementsAre(Record(2)));
  EXPECT_EQ(next.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest, ReadsAllInputTypes) {
  constexpr std::array kInputTypes{
      ui::ScrollInputType::kTouchscreen,
      ui::ScrollInputType::kWheel,
      ui::ScrollInputType::kAutoscroll,
      ui::ScrollInputType::kScrollbar,
  };
  static_assert(kInputTypes.size() ==
                static_cast<int>(ui::ScrollInputType::kMaxValue) + 1);
  std::vector<ScrollTimingInfo> expected;
  for (const auto input_type : kInputTypes) {
    auto record = Record(expected.size() + 1);
    record.input_type = input_type;
    EXPECT_EQ(record.reserved, 0u);
    buffer_->Push(record);
    expected.push_back(record);
  }
  const auto snapshot = consumer_->ReadRecords();
  EXPECT_EQ(snapshot.records, expected);
  EXPECT_EQ(snapshot.overwritten_count, 0u);
}

TEST_F(ScrollTimingBufferTest, RejectsInvalidInputTypes) {
  for (const int input_type :
       {-1, static_cast<int>(ui::ScrollInputType::kMaxValue) + 1}) {
    SCOPED_TRACE(input_type);
    auto invalid_record = Record(1);
    invalid_record.input_type = static_cast<ui::ScrollInputType>(input_type);
    buffer_->Push(invalid_record);
    buffer_->Push(Record(2));
    const auto snapshot = consumer_->ReadRecords();
    EXPECT_THAT(snapshot.records, testing::ElementsAre(Record(2)));
    EXPECT_EQ(snapshot.overwritten_count, 0u);
    const auto empty = consumer_->ReadRecords();
    EXPECT_THAT(empty.records, testing::IsEmpty());
    EXPECT_EQ(empty.overwritten_count, 0u);
  }
}

}  // namespace cc
