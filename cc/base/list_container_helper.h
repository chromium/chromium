// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_LIST_CONTAINER_HELPER_H_
#define CC_BASE_LIST_CONTAINER_HELPER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "cc/base/base_export.h"

namespace cc {

// Helper class for ListContainer non-templated logic. All methods are private,
// and only exposed to friend classes.
// For usage, see comments in ListContainer (list_container.h).
class CC_BASE_EXPORT ListContainerHelper final {
 private:
  template <typename T>
  friend class ListContainer;

  explicit ListContainerHelper(size_t alignment,
                               size_t max_size_for_derived_class,
                               size_t num_of_elements_to_reserve_for);
  ListContainerHelper(const ListContainerHelper&) = delete;
  ~ListContainerHelper();

  ListContainerHelper& operator=(const ListContainerHelper&) = delete;

  // This class deals only with char* and void*. It does allocation and passing
  // out raw pointers, as well as memory deallocation when being destroyed.
  class CharAllocator;

  // This class points to a certain position inside memory of
  // CharAllocator. It is a base class for ListContainer iterators.
  struct CC_BASE_EXPORT PositionInCharAllocator {
    // `ptr_to_container` is not a raw_ptr<...> for performance reasons (based
    // on analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION CharAllocator* ptr_to_container = nullptr;

    size_t vector_index = 0;

    // `item_iterator` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION char* item_iterator = nullptr;

    PositionInCharAllocator() = default;

    PositionInCharAllocator(const PositionInCharAllocator& other);
    PositionInCharAllocator& operator=(const PositionInCharAllocator& other);

    PositionInCharAllocator(CharAllocator* container,
                            size_t vector_ind,
                            char* item_iter);

    bool operator==(const PositionInCharAllocator& other) const;
    bool operator!=(const PositionInCharAllocator& other) const;

    PositionInCharAllocator Increment();
    PositionInCharAllocator ReverseIncrement();
  };

  // Iterator classes that can be used to access data.
  /////////////////////////////////////////////////////////////////
  class CC_BASE_EXPORT Iterator : public PositionInCharAllocator {
    // This class is only defined to forward iterate through
    // CharAllocator.
   public:
    Iterator() = default;

    Iterator(CharAllocator* container,
             size_t vector_ind,
             char* item_iter,
             size_t index);
    ~Iterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_ = 0;
  };

  class CC_BASE_EXPORT ConstIterator : public PositionInCharAllocator {
    // This class is only defined to forward iterate through
    // CharAllocator.
   public:
    ConstIterator() = default;

    ConstIterator(CharAllocator* container,
                  size_t vector_ind,
                  char* item_iter,
                  size_t index);
    ConstIterator(const Iterator& other);  // NOLINT
    ~ConstIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since begin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For iterator this means begin() corresponds to index 0 and end()
    // corresponds to index |size|.
    size_t index_ = 0;
  };

  class CC_BASE_EXPORT ReverseIterator : public PositionInCharAllocator {
    // This class is only defined to reverse iterate through
    // CharAllocator.
   public:
    ReverseIterator() = default;

    ReverseIterator(CharAllocator* container,
                    size_t vector_ind,
                    char* item_iter,
                    size_t index);
    ~ReverseIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_ = 0;
  };

  class CC_BASE_EXPORT ConstReverseIterator : public PositionInCharAllocator {
    // This class is only defined to reverse iterate through
    // CharAllocator.
   public:
    ConstReverseIterator() = default;

    ConstReverseIterator(CharAllocator* container,
                         size_t vector_ind,
                         char* item_iter,
                         size_t index);
    ConstReverseIterator(const ReverseIterator& other);  // NOLINT
    ~ConstReverseIterator();

    size_t index() const;

   protected:
    // This is used to track how many increment has happened since rbegin(). It
    // is used to avoid double increment at places an index reference is
    // needed. For reverse iterator this means rbegin() corresponds to index 0
    // and rend() corresponds to index |size|.
    size_t index_ = 0;
  };

  // Unlike the ListContainer methods, these do not invoke element destructors.
  void RemoveLast();
  void EraseAndInvalidateAllPointers(Iterator* position);
  void InsertBeforeAndInvalidateAllPointers(Iterator* position,
                                            size_t number_of_elements);

  ConstReverseIterator crbegin() const;
  ConstReverseIterator crend() const;
  ReverseIterator rbegin();
  ReverseIterator rend();
  ConstIterator cbegin() const;
  ConstIterator cend() const;
  Iterator begin();
  Iterator end();

  Iterator IteratorAt(size_t index);
  ConstIterator IteratorAt(size_t index) const;

  size_t size() const;
  bool empty() const;

  size_t MaxSizeForDerivedClass() const;

  size_t GetCapacityInBytes() const;

  // Unlike the ListContainer method, this one does not invoke element
  // destructors.
  void clear();

  size_t AvailableSizeWithoutAnotherAllocationForTesting() const;

  // Hands out memory location for an element at the end of data structure.
  void* Allocate(size_t alignment, size_t size_of_actual_element_in_bytes);

  std::unique_ptr<CharAllocator> data_;
};

}  // namespace cc

#endif  // CC_BASE_LIST_CONTAINER_HELPER_H_
