// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/base/list_container_helper.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr_exclusion.h"

namespace {
const size_t kDefaultNumElementTypesToReserve = 32;
}  // namespace

namespace cc {

// CharAllocator
////////////////////////////////////////////////////
// This class deals only with char* and void*. It does allocation and passing
// out raw pointers, as well as memory deallocation when being destroyed.
class ListContainerHelper::CharAllocator {
 public:
  // CharAllocator::InnerList
  /////////////////////////////////////////////
  // This class holds the raw memory chunk, as well as information about its
  // size and availability.
  struct InnerList {
    InnerList(size_t capacity, size_t element_size, size_t alignment)
        : data(static_cast<char*>(
              base::AlignedAlloc(capacity * element_size, alignment))),
          capacity(capacity),
          size(0),
          step(element_size) {}
    InnerList(InnerList&& other) = default;
    InnerList& operator=(InnerList&& other) = default;

    std::unique_ptr<char[], base::AlignedFreeDeleter> data;
    // The number of elements in total the memory can hold. The difference
    // between capacity and size is the how many more elements this list can
    // hold.
    size_t capacity;
    // The number of elements have been put into this list.
    size_t size;
    // The size of each element is in bytes. This is used to move from between
    // elements' memory locations.
    size_t step;

    void Erase(char* position) {
      // Confident that destructor is called by caller of this function. Since
      // CharAllocator does not handle construction after
      // allocation, it doesn't handle desctrution before deallocation.
      DCHECK_LE(position, LastElement());
      DCHECK_GE(position, Begin());
      char* start = position + step;
      std::copy(start, End(), position);

      --size;
      // Decrease capacity to avoid creating not full not last InnerList.
      --capacity;
    }

    void InsertBefore(size_t alignment, char** position, size_t count) {
      DCHECK_LE(*position, LastElement() + step);
      DCHECK_GE(*position, Begin());

      // Adjust the size and capacity
      size_t old_size = size;
      size += count;
      capacity = size;

      // Allocate the new data and update the iterator's pointer.
      std::unique_ptr<char[], base::AlignedFreeDeleter> new_data(
          static_cast<char*>(base::AlignedAlloc(size * step, alignment)));
      size_t position_offset = *position - Begin();
      *position = new_data.get() + position_offset;

      // Copy the data before the inserted segment
      memcpy(new_data.get(), data.get(), position_offset);
      // Copy the data after the inserted segment.
      memcpy(new_data.get() + position_offset + count * step,
             data.get() + position_offset, old_size * step - position_offset);
      data = std::move(new_data);
    }

    bool IsEmpty() const { return !size; }
    bool IsFull() { return capacity == size; }
    size_t NumElementsAvailable() const { return capacity - size; }

    void* AddElement() {
      DCHECK_LT(size, capacity);
      ++size;
      return LastElement();
    }

    void RemoveLast() {
      DCHECK(!IsEmpty());
      --size;
    }

    char* Begin() const { return data.get(); }
    char* End() const { return data.get() + size * step; }
    char* LastElement() const { return data.get() + (size - 1) * step; }
    char* ElementAt(size_t index) const { return data.get() + index * step; }
  };

  CharAllocator(size_t alignment, size_t element_size, size_t element_count)
      // base::AlignedAlloc does not accept alignment less than sizeof(void*).
      : alignment_(std::max(sizeof(void*), alignment)),
        element_size_(element_size),
        size_(0),
        last_list_index_(0),
        last_list_(nullptr) {
    // If this fails, then alignment of elements after the first could be wrong,
    // and we need to pad sizes to fix that.
    DCHECK_EQ(element_size % alignment, 0u);
    AllocateNewList(element_count > 0 ? element_count
                                      : kDefaultNumElementTypesToReserve);
    last_list_ = &storage_[last_list_index_];
  }

  CharAllocator(const CharAllocator&) = delete;
  ~CharAllocator() = default;

  CharAllocator& operator=(const CharAllocator&) = delete;

  void* Allocate() {
    if (last_list_->IsFull()) {
      // Only allocate a new list if there isn't a spare one still there from
      // previous usage.
      if (last_list_index_ + 1 >= storage_.size())
        AllocateNewList(last_list_->capacity * 2);

      ++last_list_index_;
      last_list_ = &storage_[last_list_index_];
    }

    ++size_;
    return last_list_->AddElement();
  }

  size_t alignment() const { return alignment_; }
  size_t element_size() const { return element_size_; }
  size_t list_count() const { return storage_.size(); }
  size_t size() const { return size_; }
  bool IsEmpty() const { return size() == 0; }

  size_t Capacity() const {
    size_t capacity_sum = 0;
    for (const auto& inner_list : storage_)
      capacity_sum += inner_list.capacity;
    return capacity_sum;
  }

  void Clear() {
    // Remove all except for the first InnerList.
    DCHECK(!storage_.empty());
    storage_.erase(storage_.begin() + 1, storage_.end());
    last_list_index_ = 0;
    last_list_ = &storage_[0];
    last_list_->size = 0;
    size_ = 0;
  }

  void RemoveLast() {
    DCHECK(!IsEmpty());
    last_list_->RemoveLast();
    if (last_list_->IsEmpty() && last_list_index_ > 0) {
      --last_list_index_;
      last_list_ = &storage_[last_list_index_];

      // If there are now two empty inner lists, free one of them.
      if (last_list_index_ + 2 < storage_.size())
        storage_.pop_back();
    }
    --size_;
  }

  void Erase(PositionInCharAllocator* position) {
    DCHECK_EQ(this, position->ptr_to_container);

    // Update |position| to point to the element after the erased element.
    InnerList& list = storage_[position->vector_index];
    char* item_iterator = position->item_iterator;
    if (item_iterator == list.LastElement())
      position->Increment();

    list.Erase(item_iterator);
    // TODO(weiliangc): Free the InnerList if it is empty.
    --size_;
  }

  void InsertBefore(ListContainerHelper::Iterator* position, size_t count) {
    if (!count)
      return;

    // If |position| is End(), then append |count| elements at the end. This
    // will happen to not invalidate any iterators or memory.
    if (!position->item_iterator) {
      // Set |position| to be the first inserted element.
      Allocate();
      position->vector_index = storage_.size() - 1;
      position->item_iterator = storage_[position->vector_index].LastElement();
      // Allocate the rest.
      for (size_t i = 1; i < count; ++i)
        Allocate();
    } else {
      storage_[position->vector_index].InsertBefore(
          alignment_, &position->item_iterator, count);
      size_ += count;
    }
  }

  const InnerList& InnerListById(size_t id) const {
    DCHECK_LT(id, storage_.size());
    return storage_[id];
  }

  size_t FirstInnerListId() const {
    // |size_| > 0 means that at least one vector in |storage_| will be
    // non-empty.
    DCHECK_GT(size_, 0u);
    size_t id = 0;
    while (storage_[id].size == 0)
      ++id;
    return id;
  }

  size_t LastInnerListId() const {
    // |size_| > 0 means that at least one vector in |storage_| will be
    // non-empty.
    DCHECK_GT(size_, 0u);
    size_t id = storage_.size() - 1;
    while (storage_[id].size == 0)
      --id;
    return id;
  }

  size_t NumAvailableElementsInLastList() const {
    return last_list_->NumElementsAvailable();
  }

 private:
  void AllocateNewList(size_t list_size) {
    storage_.emplace_back(list_size, element_size_, alignment_);
  }

  std::vector<InnerList> storage_;
  const size_t alignment_;
  const size_t element_size_;

  // The number of elements in the list.
  size_t size_;

  // The index of the last list to have had elements added to it, or the only
  // list if the container has not had elements added since being cleared.
  size_t last_list_index_;

  // This is equivalent to |storage_[last_list_index_]|.
  //
  // `last_list_` is not a raw_ptr<...> for performance reasons (based on
  // analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION InnerList* last_list_;
};

// PositionInCharAllocator
//////////////////////////////////////////////////////
ListContainerHelper::PositionInCharAllocator::PositionInCharAllocator(
    const ListContainerHelper::PositionInCharAllocator& other) = default;

ListContainerHelper::PositionInCharAllocator&
ListContainerHelper::PositionInCharAllocator::operator=(
    const ListContainerHelper::PositionInCharAllocator& other) = default;

ListContainerHelper::PositionInCharAllocator::PositionInCharAllocator(
    ListContainerHelper::CharAllocator* container,
    size_t vector_ind,
    char* item_iter)
    : ptr_to_container(container),
      vector_index(vector_ind),
      item_iterator(item_iter) {}

bool ListContainerHelper::PositionInCharAllocator::operator==(
    const ListContainerHelper::PositionInCharAllocator& other) const {
  DCHECK_EQ(ptr_to_container, other.ptr_to_container);
  return vector_index == other.vector_index &&
         item_iterator == other.item_iterator;
}

bool ListContainerHelper::PositionInCharAllocator::operator!=(
    const ListContainerHelper::PositionInCharAllocator& other) const {
  return !(*this == other);
}

ListContainerHelper::PositionInCharAllocator
ListContainerHelper::PositionInCharAllocator::Increment() {
  const auto& list = ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list.LastElement()) {
    ++vector_index;
    while (vector_index < ptr_to_container->list_count()) {
      if (ptr_to_container->InnerListById(vector_index).size != 0)
        break;
      ++vector_index;
    }
    if (vector_index < ptr_to_container->list_count())
      item_iterator = ptr_to_container->InnerListById(vector_index).Begin();
    else
      item_iterator = nullptr;
  } else {
    item_iterator += list.step;
  }
  return *this;
}

ListContainerHelper::PositionInCharAllocator
ListContainerHelper::PositionInCharAllocator::ReverseIncrement() {
  const auto& list = ptr_to_container->InnerListById(vector_index);
  if (item_iterator == list.Begin()) {
    --vector_index;
    // Since |vector_index| is unsigned, we compare < list_count() instead of
    // comparing >= 0, as the variable will wrap around when it goes out of
    // range (below 0).
    while (vector_index < ptr_to_container->list_count()) {
      if (ptr_to_container->InnerListById(vector_index).size != 0)
        break;
      --vector_index;
    }
    if (vector_index < ptr_to_container->list_count()) {
      item_iterator =
          ptr_to_container->InnerListById(vector_index).LastElement();
    } else {
      item_iterator = nullptr;
    }
  } else {
    item_iterator -= list.step;
  }
  return *this;
}

// ListContainerHelper
////////////////////////////////////////////
ListContainerHelper::ListContainerHelper(size_t alignment,
                                         size_t max_size_for_derived_class,
                                         size_t num_of_elements_to_reserve_for)
    : data_(std::make_unique<CharAllocator>(alignment,
                                            max_size_for_derived_class,
                                            num_of_elements_to_reserve_for)) {}

ListContainerHelper::~ListContainerHelper() = default;

void ListContainerHelper::RemoveLast() {
  data_->RemoveLast();
}

void ListContainerHelper::EraseAndInvalidateAllPointers(
    ListContainerHelper::Iterator* position) {
  data_->Erase(position);
}

void ListContainerHelper::InsertBeforeAndInvalidateAllPointers(
    ListContainerHelper::Iterator* position,
    size_t count) {
  data_->InsertBefore(position, count);
}

ListContainerHelper::ConstReverseIterator ListContainerHelper::crbegin() const {
  if (data_->IsEmpty())
    return crend();

  size_t id = data_->LastInnerListId();
  return ConstReverseIterator(data_.get(), id,
                              data_->InnerListById(id).LastElement(), 0);
}

ListContainerHelper::ConstReverseIterator ListContainerHelper::crend() const {
  return ConstReverseIterator(data_.get(), static_cast<size_t>(-1), nullptr,
                              size());
}

ListContainerHelper::ReverseIterator ListContainerHelper::rbegin() {
  if (data_->IsEmpty())
    return rend();

  size_t id = data_->LastInnerListId();
  return ReverseIterator(data_.get(), id,
                         data_->InnerListById(id).LastElement(), 0);
}

ListContainerHelper::ReverseIterator ListContainerHelper::rend() {
  return ReverseIterator(data_.get(), static_cast<size_t>(-1), nullptr, size());
}

ListContainerHelper::ConstIterator ListContainerHelper::cbegin() const {
  if (data_->IsEmpty())
    return cend();

  size_t id = data_->FirstInnerListId();
  return ConstIterator(data_.get(), id, data_->InnerListById(id).Begin(), 0);
}

ListContainerHelper::ConstIterator ListContainerHelper::cend() const {
  if (data_->IsEmpty())
    return ConstIterator(data_.get(), 0, nullptr, size());

  size_t id = data_->list_count();
  return ConstIterator(data_.get(), id, nullptr, size());
}

ListContainerHelper::Iterator ListContainerHelper::begin() {
  if (data_->IsEmpty())
    return end();

  size_t id = data_->FirstInnerListId();
  return Iterator(data_.get(), id, data_->InnerListById(id).Begin(), 0);
}

ListContainerHelper::Iterator ListContainerHelper::end() {
  if (data_->IsEmpty())
    return Iterator(data_.get(), 0, nullptr, size());

  size_t id = data_->list_count();
  return Iterator(data_.get(), id, nullptr, size());
}

ListContainerHelper::ConstIterator ListContainerHelper::IteratorAt(
    size_t index) const {
  DCHECK_LT(index, size());
  size_t original_index = index;
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index).size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return ConstIterator(data_.get(), list_index,
                       data_->InnerListById(list_index).ElementAt(index),
                       original_index);
}

ListContainerHelper::Iterator ListContainerHelper::IteratorAt(size_t index) {
  DCHECK_LT(index, size());
  size_t original_index = index;
  size_t list_index;
  for (list_index = 0; list_index < data_->list_count(); ++list_index) {
    size_t current_size = data_->InnerListById(list_index).size;
    if (index < current_size)
      break;
    index -= current_size;
  }
  return Iterator(data_.get(), list_index,
                  data_->InnerListById(list_index).ElementAt(index),
                  original_index);
}

void* ListContainerHelper::Allocate(size_t alignment,
                                    size_t size_of_actual_element_in_bytes) {
  DCHECK_LE(alignment, data_->alignment());
  DCHECK_LE(size_of_actual_element_in_bytes, data_->element_size());
  return data_->Allocate();
}

size_t ListContainerHelper::size() const {
  return data_->size();
}

bool ListContainerHelper::empty() const {
  return data_->IsEmpty();
}

size_t ListContainerHelper::MaxSizeForDerivedClass() const {
  return data_->element_size();
}

size_t ListContainerHelper::GetCapacityInBytes() const {
  return data_->Capacity() * data_->element_size();
}

void ListContainerHelper::clear() {
  data_->Clear();
}

size_t ListContainerHelper::AvailableSizeWithoutAnotherAllocationForTesting()
    const {
  return data_->NumAvailableElementsInLastList();
}

// ListContainerHelper::Iterator
/////////////////////////////////////////////////
ListContainerHelper::Iterator::Iterator(CharAllocator* container,
                                        size_t vector_ind,
                                        char* item_iter,
                                        size_t index)
    : PositionInCharAllocator(container, vector_ind, item_iter),
      index_(index) {}

ListContainerHelper::Iterator::~Iterator() = default;

size_t ListContainerHelper::Iterator::index() const {
  return index_;
}

// ListContainerHelper::ConstIterator
/////////////////////////////////////////////////
ListContainerHelper::ConstIterator::ConstIterator(
    const ListContainerHelper::Iterator& other)
    : PositionInCharAllocator(other), index_(other.index()) {}

ListContainerHelper::ConstIterator::ConstIterator(CharAllocator* container,
                                                  size_t vector_ind,
                                                  char* item_iter,
                                                  size_t index)
    : PositionInCharAllocator(container, vector_ind, item_iter),
      index_(index) {}

ListContainerHelper::ConstIterator::~ConstIterator() = default;

size_t ListContainerHelper::ConstIterator::index() const {
  return index_;
}

// ListContainerHelper::ReverseIterator
/////////////////////////////////////////////////
ListContainerHelper::ReverseIterator::ReverseIterator(CharAllocator* container,
                                                      size_t vector_ind,
                                                      char* item_iter,
                                                      size_t index)
    : PositionInCharAllocator(container, vector_ind, item_iter),
      index_(index) {}

ListContainerHelper::ReverseIterator::~ReverseIterator() = default;

size_t ListContainerHelper::ReverseIterator::index() const {
  return index_;
}

// ListContainerHelper::ConstReverseIterator
/////////////////////////////////////////////////
ListContainerHelper::ConstReverseIterator::ConstReverseIterator(
    const ListContainerHelper::ReverseIterator& other)
    : PositionInCharAllocator(other), index_(other.index()) {}

ListContainerHelper::ConstReverseIterator::ConstReverseIterator(
    CharAllocator* container,
    size_t vector_ind,
    char* item_iter,
    size_t index)
    : PositionInCharAllocator(container, vector_ind, item_iter),
      index_(index) {}

ListContainerHelper::ConstReverseIterator::~ConstReverseIterator() = default;

size_t ListContainerHelper::ConstReverseIterator::index() const {
  return index_;
}

}  // namespace cc
