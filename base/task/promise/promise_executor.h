// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_PROMISE_EXECUTOR_H_
#define BASE_TASK_PROMISE_PROMISE_EXECUTOR_H_

#include "base/base_export.h"
#include "base/logging.h"
#include "base/task/promise/promise_value.h"

namespace base {
namespace internal {
class AbstractPromise;

// Unresolved promises have an executor which invokes one of the callbacks
// associated with the promise. Once the callback has been invoked the
// Executor is destroyed.
//
// Ideally Executor would be a pure virtual class, but we want to store these
// inline to reduce the number of memory allocations (small object
// optimization). The problem is even though placement new returns the same
// address it was allocated at, you have to use the returned pointer.  Casting
// the buffer to the derived class is undefined behavior. STL implementations
// usually store an extra pointer, but there we have opted for implementing
// our own VTable to save a little bit of memory.
class BASE_EXPORT PromiseExecutor {
 private:
  static constexpr size_t MaxSize = sizeof(void*) * 2;
  struct VTable;

 public:
  // We could just construct Executor in place, but that means templates need
  // to inline the AbstractPromise constructor which we'd like to avoid due to
  // binary size concerns. Despite containing refcounted objects, Data is
  // intended to be memcopied into the Executor and it deliberately does not
  // have a destructor. The type erasure provided by Executor allows us to
  // move the AbstractPromise construction out of line.
  class Data {
   public:
    // Constructs |Derived| in place.
    template <typename Derived, typename... Args>
    explicit Data(in_place_type_t<Derived>, Args&&... args) {
      static_assert(sizeof(Derived) <= MaxSize, "Derived is too big");
      static_assert(
          sizeof(PromiseExecutor) <= sizeof(PromiseValueInternal::InlineAlloc),
          "Executor is too big");
      vtable_ = &VTableHelper<Derived>::vtable_;
      new (storage_.array) Derived(std::forward<Args>(args)...);
    }

    Data(Data&& other) noexcept
        : vtable_(other.vtable_), storage_(other.storage_) {
#if DCHECK_IS_ON()
      other.vtable_ = nullptr;
#endif
    }

    Data(const Data& other) = delete;

    ~Data() { DCHECK_EQ(vtable_, nullptr); }

   private:
    friend class PromiseExecutor;

    const VTable* vtable_;
    struct {
      char array[MaxSize];
    } storage_;
  };

  // Caution it's an error to use |data| after this.
  explicit PromiseExecutor(Data&& data) : data_(std::move(data)) {}

  PromiseExecutor(PromiseExecutor&& other) noexcept
      : data_(std::move(other.data_)) {
    other.data_.vtable_ = nullptr;
  }

  PromiseExecutor(const PromiseExecutor& other) = delete;

  ~PromiseExecutor();

  PromiseExecutor& operator=(const PromiseExecutor& other) = delete;

  // Controls whether or not a promise should wait for its prerequisites
  // before becoming eligible for execution.
  enum class PrerequisitePolicy : uint8_t {
    // Wait for all prerequisites to resolve (or any to reject) before
    // becoming eligible for execution. If any prerequisites are canceled it
    // will be canceled too.
    kAll,

    // Wait for any prerequisite to resolve or reject before becoming eligible
    // for execution. If all prerequisites are canceled it will be canceled
    // too.
    kAny,

    // Never become eligible for execution. Cancellation is ignored.
    kNever,
  };

  // Returns the associated PrerequisitePolicy.
  PrerequisitePolicy GetPrerequisitePolicy() const {
    return data_.vtable_->prerequsite_policy;
  }

  // NB if there is both a resolve and a reject executor we require them to
  // be both canceled at the same time.
  bool IsCancelled() const {
    return data_.vtable_->is_cancelled(data_.storage_.array);
  }

  // Describes an executor callback.
  enum class ArgumentPassingType : uint8_t {
    // No callback. E.g. the RejectArgumentPassingType in a promise with a
    // resolve callback but no reject callback.
    kNoCallback,

    // Executor callback argument passed by value or by reference.
    kNormal,

    // Executor callback argument passed by r-value reference.
    kMove,
  };

#if DCHECK_IS_ON()
  // Returns details of the resolve and reject executor callbacks if any. This
  // data is used to diagnose double moves and missing catches.
  ArgumentPassingType ResolveArgumentPassingType() const;
  ArgumentPassingType RejectArgumentPassingType() const;
  bool CanResolve() const;
  bool CanReject() const;
#endif

  // Invokes the associate callback for |promise|. If the callback was
  // cancelled it should call |promise->OnCanceled()|. If the callback
  // resolved it should store the resolve result via |promise->emplace()|. If
  // the callback was rejected it should store the reject result in
  // |promise->state()|. Caution the Executor will be destructed when
  // |promise->state()| is written to.
  void Execute(AbstractPromise* promise) {
    return data_.vtable_->execute(data_.storage_.array, promise);
  }

 private:
  struct VTable {
    void (*destructor)(void* self);
    PrerequisitePolicy prerequsite_policy;
    bool (*is_cancelled)(const void* self);
#if DCHECK_IS_ON()
    ArgumentPassingType (*resolve_argument_passing_type)(const void* self);
    ArgumentPassingType (*reject_argument_passing_type)(const void* self);
    bool (*can_resolve)(const void* self);
    bool (*can_reject)(const void* self);
#endif
    void (*execute)(void* self, AbstractPromise* promise);

   private:
    DISALLOW_COPY_AND_ASSIGN(VTable);
  };

  template <typename DerivedType>
  struct VTableHelper {
    VTableHelper(const VTableHelper& other) = delete;
    VTableHelper& operator=(const VTableHelper& other) = delete;

    static void Destructor(void* self) {
      static_cast<DerivedType*>(self)->~DerivedType();
    }

    static constexpr PromiseExecutor::PrerequisitePolicy kPrerequisitePolicy =
        DerivedType::kPrerequisitePolicy;

    static PrerequisitePolicy GetPrerequisitePolicy(const void* self) {
      return static_cast<const DerivedType*>(self)->GetPrerequisitePolicy();
    }

    static bool IsCancelled(const void* self) {
      return static_cast<const DerivedType*>(self)->IsCancelled();
    }

#if DCHECK_IS_ON()
    static ArgumentPassingType ResolveArgumentPassingType(const void* self) {
      return static_cast<const DerivedType*>(self)
          ->ResolveArgumentPassingType();
    }

    static ArgumentPassingType RejectArgumentPassingType(const void* self) {
      return static_cast<const DerivedType*>(self)->RejectArgumentPassingType();
    }

    static bool CanResolve(const void* self) {
      return static_cast<const DerivedType*>(self)->CanResolve();
    }

    static bool CanReject(const void* self) {
      return static_cast<const DerivedType*>(self)->CanReject();
    }
#endif

    static void Execute(void* self, AbstractPromise* promise) {
      return static_cast<DerivedType*>(self)->Execute(promise);
    }

    static constexpr VTable vtable_ = {
        &VTableHelper::Destructor,
        VTableHelper::kPrerequisitePolicy,
        &VTableHelper::IsCancelled,
#if DCHECK_IS_ON()
        &VTableHelper::ResolveArgumentPassingType,
        &VTableHelper::RejectArgumentPassingType,
        &VTableHelper::CanResolve,
        &VTableHelper::CanReject,
#endif
        &VTableHelper::Execute,
    };
  };

  Data data_;
};

// static
template <typename T>
const PromiseExecutor::VTable PromiseExecutor::VTableHelper<T>::vtable_;

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_PROMISE_EXECUTOR_H_
