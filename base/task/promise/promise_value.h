// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_PROMISE_VALUE_H_
#define BASE_TASK_PROMISE_PROMISE_VALUE_H_

#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/parameter_pack.h"

namespace base {

namespace internal {
class AbstractPromise;
}  // namespace internal

// std::variant, std::tuple and other templates can't contain void but they can
// contain the empty type Void. This is the same idea as std::monospace.
struct Void {};

// Signals that a promise doesn't resolve.  E.g. Promise<NoResolve, int>
struct NoResolve {};

// Signals that a promise doesn't reject.  E.g. Promise<int, NoReject>
struct NoReject {};

// Internally Resolved<> is used to store the result of a promise callback that
// resolved. This lets us disambiguate promises with the same resolve and reject
// type.
template <typename T>
struct Resolved {
  using Type = T;

  static_assert(!std::is_same<T, NoReject>::value,
                "Can't have Resolved<NoReject>");

  Resolved() {
    static_assert(!std::is_same<T, NoResolve>::value,
                  "Can't have Resolved<NoResolve>");
  }

  // Conversion constructor accepts any arguments except Resolved<T>.
  template <
      typename... Args,
      std::enable_if_t<!all_of(
          {std::is_same<Resolved, std::decay_t<Args>>::value...})>* = nullptr>
  Resolved(Args&&... args) noexcept : value(std::forward<Args>(args)...) {}

  T value;
};

template <>
struct Resolved<void> {
  using Type = void;
  Void value;
};

// Internally Rejected<> is used to store the result of a promise callback that
// rejected. This lets us disambiguate promises with the same resolve and reject
// type.
template <typename T>
struct Rejected {
  using Type = T;
  T value;

  static_assert(!std::is_same<T, NoResolve>::value,
                "Can't have Rejected<NoResolve>");

  Rejected() {
    static_assert(!std::is_same<T, NoReject>::value,
                  "Can't have Rejected<NoReject>");
  }

  // Conversion constructor accepts any arguments except Rejected<T>.
  template <
      typename... Args,
      std::enable_if_t<!all_of(
          {std::is_same<Rejected, std::decay_t<Args>>::value...})>* = nullptr>
  Rejected(Args&&... args) noexcept : value(std::forward<Args>(args)...) {
    static_assert(!std::is_same<T, NoReject>::value,
                  "Can't have Rejected<NoReject>");
  }
};

template <>
struct Rejected<void> {
  using Type = void;
  Void value;
};

namespace internal {

class PromiseExecutor;

struct BASE_EXPORT PromiseValueInternal {
  // The state is stored in the bottom three bits of the TypeOps pointer, see
  // TaggedTypeOpsPtr.
  enum State {
    EMPTY,
    PROMISE_EXECUTOR,
    CURRIED_PROMISE,
    RESOLVED,
    REJECTED,

    // This value is never stored and is used internally for error checking.
    INVALID
  };

  // Where possible we use the small object allocation optimization to avoid
  // heap allocations.
  struct OutlineAlloc {
    void* value;  // Holds a T

    template <typename T>
    T& value_as() {
      return *static_cast<T*>(value);
    }

    template <typename T>
    const T& value_as() const {
      return *static_cast<const T*>(value);
    }
  };

  struct alignas(sizeof(void*)) InlineAlloc {
    // Holds a T if small. Tweaked to hold a promise executor inline.
    char bytes[sizeof(void*) * 3];

    template <typename T>
    T& value_as() {
      return *reinterpret_cast<T*>(bytes);
    }

    template <typename T>
    const T& value_as() const {
      return *reinterpret_cast<const T*>(bytes);
    }
  };

  template <typename T>
  struct InlineStorageHelper {
    static constexpr bool kUseInlineStorage =
        (sizeof(T) <= sizeof(InlineAlloc));

    static_assert(
        std::alignment_of<T>::value <= sizeof(T),
        "Type T has alignment requirements that preclude it's storage inline.");
  };

  template <typename T>
  constexpr T* GetStorage() {
    return static_cast<T*>(
        GetStorageHelper<InlineStorageHelper<T>::kUseInlineStorage>::GetStorage(
            *this));
  }

  template <typename T>
  constexpr const T* GetStorage() const {
    return static_cast<const T*>(
        GetStorageHelper<InlineStorageHelper<T>::kUseInlineStorage>::GetStorage(
            *this));
  }

  template <typename T, bool UseInlineStorage>
  struct ConstructHelper;

  template <bool UseInlineStorage>
  struct GetStorageHelper;

  template <typename T, bool UseInlineStorage, bool HasMoveConstructor>
  struct MoveHelper;

  template <typename T, bool UseInlineStorage>
  struct DeleteHelper;

  template <typename T>
  struct TypeToStateHelper;

  using MoveFunctionPtr = void (*)(PromiseValueInternal* src,
                                   PromiseValueInternal* dest);
  using DeleteFunctionPtr = void (*)(PromiseValueInternal* object);

  // Similar to a virtual function but we don't need a dynamic memory
  // allocation. One possible design alternative would be to fold these methods
  // into T and use T in InlineAlloc (which would now have to
  // be bigger to accommodate the vtable pointer).
  // Eight byte alignment specified to allow TaggedTypeOpsPtr to store the state
  // in the low bits of the pointer.
  struct alignas(8) TypeOps {
#if DCHECK_IS_ON()
    const char* type_name;
#endif
    MoveFunctionPtr move_fn_ptr;
    DeleteFunctionPtr delete_fn_ptr;
  };

  template <typename T>
  struct TypeOpsHelper {
    static constexpr const char* TypeName() { return PRETTY_FUNCTION; }

    static constexpr TypeOps type_ops = {
#if DCHECK_IS_ON()
        TypeName(),
#endif
        &MoveHelper<T,
                    InlineStorageHelper<T>::kUseInlineStorage,
                    std::is_move_constructible<T>::value>::Move,
        &DeleteHelper<T, InlineStorageHelper<T>::kUseInlineStorage>::Delete};
  };

  static void NopMove(PromiseValueInternal* src, PromiseValueInternal* dest);
  static void NopDelete(PromiseValueInternal* src);

  static constexpr TypeOps null_type_ = {
#if DCHECK_IS_ON()
      "EMPTY!",
#endif
      &NopMove, &NopDelete};

  union {
    OutlineAlloc outline_alloc;
    InlineAlloc inline_alloc;
  } union_;
};

// static
template <typename T>
const PromiseValueInternal::TypeOps
    PromiseValueInternal::TypeOpsHelper<T>::type_ops;

template <typename T>
struct PromiseValueInternal::ConstructHelper<T, /* UseInlineStorage */ true> {
  template <typename... Args>
  static void Construct(PromiseValueInternal* dest, Args&&... args) noexcept {
    new (&dest->union_.inline_alloc.bytes) T(std::forward<Args>(args)...);
  }
};

template <typename T>
struct PromiseValueInternal::ConstructHelper<T, /* UseInlineStorage */ false> {
  template <typename... Args>
  static void Construct(PromiseValueInternal* dest, Args&&... args) noexcept {
    dest->union_.outline_alloc.value = new T(std::forward<Args>(args)...);
  }
};

template <>
struct PromiseValueInternal::GetStorageHelper</* UseInlineStorage */ true> {
  static void* GetStorage(PromiseValueInternal& any) {
    return &any.union_.inline_alloc.bytes;
  }

  static const void* GetStorage(const PromiseValueInternal& any) {
    return &any.union_.inline_alloc.bytes;
  }
};

template <>
struct PromiseValueInternal::GetStorageHelper</* UseInlineStorage */ false> {
  static void* GetStorage(PromiseValueInternal& any) {
    return any.union_.outline_alloc.value;
  }

  static const void* GetStorage(const PromiseValueInternal& any) {
    return any.union_.outline_alloc.value;
  }
};

template <typename T>
struct PromiseValueInternal::
    MoveHelper<T, /* UseInlineStorage */ true, /* HasMoveConstructor */ true> {
  static void Move(PromiseValueInternal* src, PromiseValueInternal* dest) {
    DCHECK_NE(src, dest);
    new (&dest->union_.inline_alloc.bytes)
        T(std::move(src->union_.inline_alloc.value_as<T>()));
  }
};

template <typename T>
struct PromiseValueInternal::
    MoveHelper<T, /* UseInlineStorage */ true, /* HasMoveConstructor */ false> {
  static void Move(PromiseValueInternal* src, PromiseValueInternal* dest) {
    DCHECK_NE(src, dest);
    // Fall back to the copy constructor.
    new (&dest->union_.inline_alloc.bytes)
        T(src->union_.inline_alloc.value_as<T>());
  }
};

template <typename T, bool HasMoveConstructor>
struct PromiseValueInternal::
    MoveHelper<T, /* UseInlineStorage */ false, HasMoveConstructor> {
  static void Move(PromiseValueInternal* src, PromiseValueInternal* dest) {
    DCHECK_NE(src, dest);
    dest->union_.outline_alloc.value = src->union_.outline_alloc.value;
    src->union_.outline_alloc.value = nullptr;
  }
};

template <typename T>
struct PromiseValueInternal::DeleteHelper<T, /* UseInlineStorage */ true> {
  static void Delete(PromiseValueInternal* any) {
    reinterpret_cast<T*>(&any->union_.inline_alloc.bytes)->~T();
  }
};

template <typename T>
struct PromiseValueInternal::DeleteHelper<T, /* UseInlineStorage */ false> {
  static void Delete(PromiseValueInternal* any) {
    delete static_cast<T*>(any->union_.outline_alloc.value);
  }
};

template <typename T>
struct PromiseValueInternal::TypeToStateHelper {
  static constexpr State state = State::INVALID;
};

template <>
struct PromiseValueInternal::TypeToStateHelper<PromiseExecutor> {
  static constexpr State state = State::PROMISE_EXECUTOR;
};

template <>
struct PromiseValueInternal::TypeToStateHelper<scoped_refptr<AbstractPromise>> {
  static constexpr State state = State::CURRIED_PROMISE;
};

template <typename T>
struct PromiseValueInternal::TypeToStateHelper<Resolved<T>> {
  static constexpr State state = State::RESOLVED;
};

template <typename T>
struct PromiseValueInternal::TypeToStateHelper<Rejected<T>> {
  static constexpr State state = State::REJECTED;
};

class TaggedTypeOpsPtr {
 public:
  using State = PromiseValueInternal::State;
  using TypeOps = PromiseValueInternal::TypeOps;

  static_assert(static_cast<int>(State::INVALID) <= alignof(TypeOps),
                "The state enum must fit in the low bits of the TypeOps "
                "address");

  void Set(const TypeOps* type_ops, PromiseValueInternal::State state) {
    DCHECK_EQ(reinterpret_cast<uintptr_t>(type_ops) & kStateMask, 0u)
        << type_ops;
    type_ops_ =
        reinterpret_cast<uintptr_t>(type_ops) | static_cast<uintptr_t>(state);
  }

  TypeOps* get() const {
    return reinterpret_cast<TypeOps*>(type_ops_ & ~kStateMask);
  }

  TypeOps* operator->() const { return get(); }

  State GetState() const { return static_cast<State>(type_ops_ & kStateMask); }

 private:
  static constexpr uintptr_t kStateMask = alignof(TypeOps) - 1;

  uintptr_t type_ops_;
};

// Inspired by std::any<> this container is used to hold a Promise's value which
// can be one of: Empty, PromiseExecutor, scoped_refptr<AbstractPromise>,
// Resolved<> or Rejected<>. Unlike std::any PromiseValue can hold move only
// types and it doesn't require exceptions.
class BASE_EXPORT PromiseValue {
 private:
  using State = PromiseValueInternal::State;
  using TypeOps = PromiseValueInternal::TypeOps;

  template <typename T>
  using TypeOpsHelper = PromiseValueInternal::TypeOpsHelper<T>;

  template <typename T>
  using Construct = PromiseValueInternal::ConstructHelper<
      T,
      PromiseValueInternal::InlineStorageHelper<T>::kUseInlineStorage>;

 public:
  PromiseValue() noexcept { MarkAsEmpty(); }

  // Constructs a PromiseValue containing |value| as long as |VT| isn't INVALID
  // according to TypeToStateHelper.
  // E.g. base::PromiseValue a(Resolved<int>(123));
  template <typename T,
            typename VT = std::decay_t<T>,
            State state = PromiseValueInternal::TypeToStateHelper<VT>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  explicit PromiseValue(T&& value) noexcept {
    Construct<VT>::Construct(&value_, std::move(value));
    type_ops_.Set(&TypeOpsHelper<VT>::type_ops, state);
  }

  // Constructs a PromiseValue containing an object of type T which is
  // initialized by std::forward<Args>(args). E.g.
  // base::unique_any a(base::in_place_type_t<Resolved<int>>(), 123);
  template <typename T,
            typename... Args,
            State state = PromiseValueInternal::TypeToStateHelper<T>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  explicit PromiseValue(in_place_type_t<T> /*tag*/, Args&&... args) noexcept {
    Construct<T>::Construct(&value_, std::forward<Args>(args)...);
    type_ops_.Set(&TypeOpsHelper<T>::type_ops, state);
  }

  // Constructs a PromiseValue with the value contained by |other| moved into
  // it.
  PromiseValue(PromiseValue&& other) noexcept {
    other.type_ops_->move_fn_ptr(&other.value_, &value_);
    type_ops_ = other.type_ops_;
    other.MarkAsEmpty();
  }

  ~PromiseValue() { reset(); }

  void reset() {
    type_ops_->delete_fn_ptr(&value_);
    MarkAsEmpty();
  }

  bool has_value() const noexcept {
    return type_ops_.GetState() != State::EMPTY;
  }

  // Clears the existing value and constructs a an object of type T which is
  // initialized by std::forward<Args>(args).
  template <typename T,
            typename... Args,
            typename VT = std::decay_t<T>,
            State state = PromiseValueInternal::TypeToStateHelper<VT>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  void emplace(in_place_type_t<T> /*tag*/, Args&&... args) noexcept {
    type_ops_->delete_fn_ptr(&value_);
    Construct<VT>::Construct(&value_, std::forward<Args>(args)...);
    type_ops_.Set(&TypeOpsHelper<VT>::type_ops, state);
  }

  // Assigns |t| as long as |VT| isn't INVALID according to TypeToStateHelper.
  template <typename T,
            typename VT = std::decay_t<T>,
            State state = PromiseValueInternal::TypeToStateHelper<VT>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  void operator=(T&& t) noexcept {
    type_ops_->delete_fn_ptr(&value_);
    Construct<VT>::Construct(&value_, std::forward<T>(t));
    type_ops_.Set(&TypeOpsHelper<VT>::type_ops, state);
  }

  void operator=(PromiseValue&& other) noexcept {
    DCHECK_NE(this, &other);
    type_ops_->delete_fn_ptr(&value_);
    other.type_ops_->move_fn_ptr(&other.value_, &value_);
    type_ops_ = other.type_ops_;
    other.MarkAsEmpty();
  }

  bool ContainsPromiseExecutor() const {
    return type_ops_.GetState() == State::PROMISE_EXECUTOR;
  }

  bool ContainsCurriedPromise() const {
    return type_ops_.GetState() == State::CURRIED_PROMISE;
  }

  bool ContainsResolved() const {
    return type_ops_.GetState() == State::RESOLVED;
  }

  bool ContainsRejected() const {
    return type_ops_.GetState() == State::REJECTED;
  }

  template <typename T,
            typename VT = std::decay_t<T>,
            State state = PromiseValueInternal::TypeToStateHelper<VT>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  T* Get() noexcept {
    DCHECK_EQ(state, type_ops_.GetState());
    // Unfortunately we can't rely on the addresses of the TypeOps being the
    // same across .so boundaries unless every part of |VT| is exported so we
    // do a string comparison instead to check the right type is used.
#if DCHECK_IS_ON()
    DCHECK_EQ(type_ops_->type_name,
              std::string(TypeOpsHelper<VT>::type_ops.type_name));
#endif
    return static_cast<T*>(value_.GetStorage<VT>());
  }

  template <typename T,
            typename VT = std::decay_t<T>,
            State state = PromiseValueInternal::TypeToStateHelper<VT>::state,
            std::enable_if_t<state != State::INVALID>* = nullptr>
  const T* Get() const noexcept {
    DCHECK_EQ(state, type_ops_.GetState());
    // Unfortunately we can't rely on the addresses of the TypeOps being the
    // same across .so boundaries unless every part of |VT| is exported so we
    // do a string comparison instead to check the right type is used.
#if DCHECK_IS_ON()
    DCHECK_EQ(type_ops_->type_name,
              std::string(TypeOpsHelper<VT>::type_ops.type_name));
#endif
    return static_cast<const T*>(value_.GetStorage<VT>());
  }

 private:
  void MarkAsEmpty() {
    type_ops_.Set(&PromiseValueInternal::null_type_, State::EMPTY);
  }

  PromiseValueInternal value_;
  TaggedTypeOpsPtr type_ops_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_PROMISE_VALUE_H_
