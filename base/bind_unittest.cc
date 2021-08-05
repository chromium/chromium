// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace base {
namespace {

class IncompleteType;

class NoRef {
 public:
  NoRef() = default;
  NoRef(const NoRef&) = delete;
  // Particularly important in this test to ensure no copies are made.
  NoRef& operator=(const NoRef&) = delete;

  MOCK_METHOD0(VoidMethod0, void());
  MOCK_CONST_METHOD0(VoidConstMethod0, void());

  MOCK_METHOD0(IntMethod0, int());
  MOCK_CONST_METHOD0(IntConstMethod0, int());

  MOCK_METHOD1(VoidMethodWithIntArg, void(int));
  MOCK_METHOD0(UniquePtrMethod0, std::unique_ptr<int>());

};

class HasRef : public NoRef {
 public:
  HasRef() = default;
  HasRef(const HasRef&) = delete;
  // Particularly important in this test to ensure no copies are made.
  HasRef& operator=(const HasRef&) = delete;

  MOCK_CONST_METHOD0(AddRef, void());
  MOCK_CONST_METHOD0(Release, bool());
  MOCK_CONST_METHOD0(HasAtLeastOneRef, bool());
};

class HasRefPrivateDtor : public HasRef {
 private:
  ~HasRefPrivateDtor() = default;
};

static const int kParentValue = 1;
static const int kChildValue = 2;

class Parent {
 public:
  void AddRef() const {}
  void Release() const {}
  bool HasAtLeastOneRef() const { return true; }
  virtual void VirtualSet() { value = kParentValue; }
  void NonVirtualSet() { value = kParentValue; }
  int value;
};

class Child : public Parent {
 public:
  void VirtualSet() override { value = kChildValue; }
  void NonVirtualSet() { value = kChildValue; }
};

class NoRefParent {
 public:
  virtual void VirtualSet() { value = kParentValue; }
  void NonVirtualSet() { value = kParentValue; }
  int value;
};

class NoRefChild : public NoRefParent {
  void VirtualSet() override { value = kChildValue; }
  void NonVirtualSet() { value = kChildValue; }
};

// Used for probing the number of copies and moves that occur if a type must be
// coerced during argument forwarding in the Run() methods.
struct DerivedCopyMoveCounter {
  DerivedCopyMoveCounter(int* copies,
                         int* assigns,
                         int* move_constructs,
                         int* move_assigns)
      : copies_(copies),
        assigns_(assigns),
        move_constructs_(move_constructs),
        move_assigns_(move_assigns) {}
  int* copies_;
  int* assigns_;
  int* move_constructs_;
  int* move_assigns_;
};

// Used for probing the number of copies and moves in an argument.
class CopyMoveCounter {
 public:
  CopyMoveCounter(int* copies,
                  int* assigns,
                  int* move_constructs,
                  int* move_assigns)
      : copies_(copies),
        assigns_(assigns),
        move_constructs_(move_constructs),
        move_assigns_(move_assigns) {}

  CopyMoveCounter(const CopyMoveCounter& other)
      : copies_(other.copies_),
        assigns_(other.assigns_),
        move_constructs_(other.move_constructs_),
        move_assigns_(other.move_assigns_) {
    (*copies_)++;
  }

  CopyMoveCounter(CopyMoveCounter&& other)
      : copies_(other.copies_),
        assigns_(other.assigns_),
        move_constructs_(other.move_constructs_),
        move_assigns_(other.move_assigns_) {
    (*move_constructs_)++;
  }

  // Probing for copies from coercion.
  explicit CopyMoveCounter(const DerivedCopyMoveCounter& other)
      : copies_(other.copies_),
        assigns_(other.assigns_),
        move_constructs_(other.move_constructs_),
        move_assigns_(other.move_assigns_) {
    (*copies_)++;
  }

  // Probing for moves from coercion.
  explicit CopyMoveCounter(DerivedCopyMoveCounter&& other)
      : copies_(other.copies_),
        assigns_(other.assigns_),
        move_constructs_(other.move_constructs_),
        move_assigns_(other.move_assigns_) {
    (*move_constructs_)++;
  }

  const CopyMoveCounter& operator=(const CopyMoveCounter& rhs) {
    copies_ = rhs.copies_;
    assigns_ = rhs.assigns_;
    move_constructs_ = rhs.move_constructs_;
    move_assigns_ = rhs.move_assigns_;

    (*assigns_)++;

    return *this;
  }

  const CopyMoveCounter& operator=(CopyMoveCounter&& rhs) {
    copies_ = rhs.copies_;
    assigns_ = rhs.assigns_;
    move_constructs_ = rhs.move_constructs_;
    move_assigns_ = rhs.move_assigns_;

    (*move_assigns_)++;

    return *this;
  }

  int copies() const {
    return *copies_;
  }

 private:
  int* copies_;
  int* assigns_;
  int* move_constructs_;
  int* move_assigns_;
};

// Used for probing the number of copies in an argument. The instance is a
// copyable and non-movable type.
class CopyCounter {
 public:
  CopyCounter(int* copies, int* assigns)
      : counter_(copies, assigns, nullptr, nullptr) {}
  CopyCounter(const CopyCounter& other) = default;
  CopyCounter& operator=(const CopyCounter& other) = default;

  explicit CopyCounter(const DerivedCopyMoveCounter& other) : counter_(other) {}

  int copies() const { return counter_.copies(); }

 private:
  CopyMoveCounter counter_;
};

// Used for probing the number of moves in an argument. The instance is a
// non-copyable and movable type.
class MoveCounter {
 public:
  MoveCounter(int* move_constructs, int* move_assigns)
      : counter_(nullptr, nullptr, move_constructs, move_assigns) {}
  MoveCounter(MoveCounter&& other) : counter_(std::move(other.counter_)) {}
  MoveCounter& operator=(MoveCounter&& other) {
    counter_ = std::move(other.counter_);
    return *this;
  }

  explicit MoveCounter(DerivedCopyMoveCounter&& other)
      : counter_(std::move(other)) {}

 private:
  CopyMoveCounter counter_;
};

class DeleteCounter {
 public:
  explicit DeleteCounter(int* deletes)
      : deletes_(deletes) {
  }

  ~DeleteCounter() {
    (*deletes_)++;
  }

  void VoidMethod0() {}

 private:
  int* deletes_;
};

template <typename T>
T PassThru(T scoper) {
  return scoper;
}

// Some test functions that we can Bind to.
template <typename T>
T PolymorphicIdentity(T t) {
  return t;
}

template <typename... Ts>
struct VoidPolymorphic {
  static void Run(Ts... t) {}
};

int Identity(int n) {
  return n;
}

int ArrayGet(const int array[], int n) {
  return array[n];
}

int Sum(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

const char* CStringIdentity(const char* s) {
  return s;
}

int GetCopies(const CopyMoveCounter& counter) {
  return counter.copies();
}

int UnwrapNoRefParent(NoRefParent p) {
  return p.value;
}

int UnwrapNoRefParentPtr(NoRefParent* p) {
  return p->value;
}

int UnwrapNoRefParentConstRef(const NoRefParent& p) {
  return p.value;
}

void RefArgSet(int &n) {
  n = 2;
}

void PtrArgSet(int *n) {
  *n = 2;
}

int FunctionWithWeakFirstParam(WeakPtr<NoRef> o, int n) {
  return n;
}

int FunctionWithScopedRefptrFirstParam(const scoped_refptr<HasRef>& o, int n) {
  return n;
}

void TakesACallback(const RepeatingClosure& callback) {
  callback.Run();
}

int Noexcept() noexcept {
  return 42;
}

class BindTest : public ::testing::Test {
 public:
  BindTest() {
    const_has_ref_ptr_ = &has_ref_;
    const_no_ref_ptr_ = &no_ref_;
    static_func_mock_ptr = &static_func_mock_;
  }
  BindTest(const BindTest&) = delete;
  BindTest& operator=(const BindTest&) = delete;
  ~BindTest() override = default;

  static void VoidFunc0() {
    static_func_mock_ptr->VoidMethod0();
  }

  static int IntFunc0() { return static_func_mock_ptr->IntMethod0(); }
  int NoexceptMethod() noexcept { return 42; }
  int ConstNoexceptMethod() const noexcept { return 42; }

 protected:
  StrictMock<NoRef> no_ref_;
  StrictMock<HasRef> has_ref_;
  const HasRef* const_has_ref_ptr_;
  const NoRef* const_no_ref_ptr_;
  StrictMock<NoRef> static_func_mock_;

  // Used by the static functions to perform expectations.
  static StrictMock<NoRef>* static_func_mock_ptr;
};

StrictMock<NoRef>* BindTest::static_func_mock_ptr;
StrictMock<NoRef>* g_func_mock_ptr;

void VoidFunc0() {
  g_func_mock_ptr->VoidMethod0();
}

int IntFunc0() {
  return g_func_mock_ptr->IntMethod0();
}

TEST_F(BindTest, BasicTest) {
  RepeatingCallback<int(int, int, int)> cb = BindRepeating(&Sum, 32, 16, 8);
  EXPECT_EQ(92, cb.Run(13, 12, 11));

  RepeatingCallback<int(int, int, int, int, int, int)> c1 = BindRepeating(&Sum);
  EXPECT_EQ(69, c1.Run(14, 13, 12, 11, 10, 9));

  RepeatingCallback<int(int, int, int)> c2 = BindRepeating(c1, 32, 16, 8);
  EXPECT_EQ(86, c2.Run(11, 10, 9));

  RepeatingCallback<int()> c3 = BindRepeating(c2, 4, 2, 1);
  EXPECT_EQ(63, c3.Run());
}

// Test that currying the rvalue result of another BindRepeating() works
// correctly.
//   - rvalue should be usable as argument to BindRepeating().
//   - multiple runs of resulting RepeatingCallback remain valid.
TEST_F(BindTest, CurryingRvalueResultOfBind) {
  int n = 0;
  RepeatingClosure cb = BindRepeating(&TakesACallback,
                                      BindRepeating(&PtrArgSet, &n));

  // If we implement BindRepeating() such that the return value has
  // auto_ptr-like semantics, the second call here will fail because ownership
  // of the internal BindState<> would have been transferred to a *temporary*
  // construction of a RepeatingCallback object on the first call.
  cb.Run();
  EXPECT_EQ(2, n);

  n = 0;
  cb.Run();
  EXPECT_EQ(2, n);
}

TEST_F(BindTest, RepeatingCallbackBasicTest) {
  RepeatingCallback<int(int)> c0 = BindRepeating(&Sum, 1, 2, 4, 8, 16);

  // RepeatingCallback can run via a lvalue-reference.
  EXPECT_EQ(63, c0.Run(32));

  // It is valid to call a RepeatingCallback more than once.
  EXPECT_EQ(54, c0.Run(23));

  // BindRepeating can handle a RepeatingCallback as the target functor.
  RepeatingCallback<int()> c1 = BindRepeating(c0, 11);

  // RepeatingCallback can run via a rvalue-reference.
  EXPECT_EQ(42, std::move(c1).Run());

  // BindRepeating can handle a rvalue-reference of RepeatingCallback.
  EXPECT_EQ(32, BindRepeating(std::move(c0), 1).Run());
}

TEST_F(BindTest, OnceCallbackBasicTest) {
  OnceCallback<int(int)> c0 = BindOnce(&Sum, 1, 2, 4, 8, 16);

  // OnceCallback can run via a rvalue-reference.
  EXPECT_EQ(63, std::move(c0).Run(32));

  // After running via the rvalue-reference, the value of the OnceCallback
  // is undefined. The implementation simply clears the instance after the
  // invocation.
  EXPECT_TRUE(c0.is_null());

  c0 = BindOnce(&Sum, 2, 3, 5, 7, 11);

  // BindOnce can handle a rvalue-reference of OnceCallback as the target
  // functor.
  OnceCallback<int()> c1 = BindOnce(std::move(c0), 13);
  EXPECT_EQ(41, std::move(c1).Run());

  RepeatingCallback<int(int)> c2 = BindRepeating(&Sum, 2, 3, 5, 7, 11);
  EXPECT_EQ(41, BindOnce(c2, 13).Run());
}

// IgnoreResult adapter test.
//   - Function with return value.
//   - Method with return value.
//   - Const Method with return.
//   - Method with return value bound to WeakPtr<>.
//   - Const Method with return bound to WeakPtr<>.
TEST_F(BindTest, IgnoreResultForRepeating) {
  EXPECT_CALL(static_func_mock_, IntMethod0()).WillOnce(Return(1337));
  EXPECT_CALL(has_ref_, AddRef()).Times(2);
  EXPECT_CALL(has_ref_, Release()).Times(2);
  EXPECT_CALL(has_ref_, HasAtLeastOneRef()).WillRepeatedly(Return(true));
  EXPECT_CALL(has_ref_, IntMethod0()).WillOnce(Return(10));
  EXPECT_CALL(has_ref_, IntConstMethod0()).WillOnce(Return(11));
  EXPECT_CALL(no_ref_, IntMethod0()).WillOnce(Return(12));
  EXPECT_CALL(no_ref_, IntConstMethod0()).WillOnce(Return(13));

  RepeatingClosure normal_func_cb = BindRepeating(IgnoreResult(&IntFunc0));
  normal_func_cb.Run();

  RepeatingClosure non_void_method_cb =
      BindRepeating(IgnoreResult(&HasRef::IntMethod0), &has_ref_);
  non_void_method_cb.Run();

  RepeatingClosure non_void_const_method_cb =
      BindRepeating(IgnoreResult(&HasRef::IntConstMethod0), &has_ref_);
  non_void_const_method_cb.Run();

  WeakPtrFactory<NoRef> weak_factory(&no_ref_);
  WeakPtrFactory<const NoRef> const_weak_factory(const_no_ref_ptr_);

  RepeatingClosure non_void_weak_method_cb  =
      BindRepeating(IgnoreResult(&NoRef::IntMethod0),
                    weak_factory.GetWeakPtr());
  non_void_weak_method_cb.Run();

  RepeatingClosure non_void_weak_const_method_cb =
      BindRepeating(IgnoreResult(&NoRef::IntConstMethod0),
                    weak_factory.GetWeakPtr());
  non_void_weak_const_method_cb.Run();

  weak_factory.InvalidateWeakPtrs();
  non_void_weak_const_method_cb.Run();
  non_void_weak_method_cb.Run();
}

TEST_F(BindTest, IgnoreResultForOnce) {
  EXPECT_CALL(static_func_mock_, IntMethod0()).WillOnce(Return(1337));
  EXPECT_CALL(has_ref_, AddRef()).Times(2);
  EXPECT_CALL(has_ref_, Release()).Times(2);
  EXPECT_CALL(has_ref_, HasAtLeastOneRef()).WillRepeatedly(Return(true));
  EXPECT_CALL(has_ref_, IntMethod0()).WillOnce(Return(10));
  EXPECT_CALL(has_ref_, IntConstMethod0()).WillOnce(Return(11));

  OnceClosure normal_func_cb = BindOnce(IgnoreResult(&IntFunc0));
  std::move(normal_func_cb).Run();

  OnceClosure non_void_method_cb =
      BindOnce(IgnoreResult(&HasRef::IntMethod0), &has_ref_);
  std::move(non_void_method_cb).Run();

  OnceClosure non_void_const_method_cb =
      BindOnce(IgnoreResult(&HasRef::IntConstMethod0), &has_ref_);
  std::move(non_void_const_method_cb).Run();

  WeakPtrFactory<NoRef> weak_factory(&no_ref_);
  WeakPtrFactory<const NoRef> const_weak_factory(const_no_ref_ptr_);

  OnceClosure non_void_weak_method_cb  =
      BindOnce(IgnoreResult(&NoRef::IntMethod0),
                  weak_factory.GetWeakPtr());
  OnceClosure non_void_weak_const_method_cb =
      BindOnce(IgnoreResult(&NoRef::IntConstMethod0),
                  weak_factory.GetWeakPtr());

  weak_factory.InvalidateWeakPtrs();
  std::move(non_void_weak_const_method_cb).Run();
  std::move(non_void_weak_method_cb).Run();
}

TEST_F(BindTest, IgnoreResultForRepeatingCallback) {
  std::string s;
  RepeatingCallback<int(int)> cb = BindRepeating(
      [](std::string* s, int i) {
        *s += "Run" + base::NumberToString(i);
        return 5;
      },
      &s);
  RepeatingCallback<void(int)> noreturn = BindRepeating(IgnoreResult(cb));
  noreturn.Run(2);
  EXPECT_EQ(s, "Run2");
}

TEST_F(BindTest, IgnoreResultForOnceCallback) {
  std::string s;
  OnceCallback<int(int)> cb = BindOnce(
      [](std::string* s, int i) {
        *s += "Run" + base::NumberToString(i);
        return 5;
      },
      &s);
  OnceCallback<void(int)> noreturn = BindOnce(IgnoreResult(std::move(cb)));
  std::move(noreturn).Run(2);
  EXPECT_EQ(s, "Run2");
}

void SetFromRef(int& ref) {
  EXPECT_EQ(ref, 1);
  ref = 2;
  EXPECT_EQ(ref, 2);
}

TEST_F(BindTest, BindOnceWithNonConstRef) {
  int v = 1;

  // Mutates `v` because it's not bound to callback instead it's forwarded by
  // Run().
  auto cb1 = BindOnce(SetFromRef);
  std::move(cb1).Run(v);
  EXPECT_EQ(v, 2);
  v = 1;

  // Mutates `v` through std::reference_wrapper bound to callback.
  auto cb2 = BindOnce(SetFromRef, std::ref(v));
  std::move(cb2).Run();
  EXPECT_EQ(v, 2);
  v = 1;

  // Everything past here following will make a copy of the argument. The copy
  // will be mutated and leave `v` unmodified.
  auto cb3 = BindOnce(SetFromRef, base::OwnedRef(v));
  std::move(cb3).Run();
  EXPECT_EQ(v, 1);

  int& ref = v;
  auto cb4 = BindOnce(SetFromRef, base::OwnedRef(ref));
  std::move(cb4).Run();
  EXPECT_EQ(v, 1);

  const int cv = 1;
  auto cb5 = BindOnce(SetFromRef, base::OwnedRef(cv));
  std::move(cb5).Run();
  EXPECT_EQ(cv, 1);

  const int& cref = v;
  auto cb6 = BindOnce(SetFromRef, base::OwnedRef(cref));
  std::move(cb6).Run();
  EXPECT_EQ(cref, 1);

  auto cb7 = BindOnce(SetFromRef, base::OwnedRef(1));
  std::move(cb7).Run();
}

TEST_F(BindTest, BindRepeatingWithNonConstRef) {
  int v = 1;

  // Mutates `v` because it's not bound to callback instead it's forwarded by
  // Run().
  auto cb1 = BindRepeating(SetFromRef);
  std::move(cb1).Run(v);
  EXPECT_EQ(v, 2);
  v = 1;

  // Mutates `v` through std::reference_wrapper bound to callback.
  auto cb2 = BindRepeating(SetFromRef, std::ref(v));
  std::move(cb2).Run();
  EXPECT_EQ(v, 2);
  v = 1;

  // Everything past here following will make a copy of the argument. The copy
  // will be mutated and leave `v` unmodified.
  auto cb3 = BindRepeating(SetFromRef, base::OwnedRef(v));
  std::move(cb3).Run();
  EXPECT_EQ(v, 1);

  int& ref = v;
  auto cb4 = BindRepeating(SetFromRef, base::OwnedRef(ref));
  std::move(cb4).Run();
  EXPECT_EQ(v, 1);

  const int cv = 1;
  auto cb5 = BindRepeating(SetFromRef, base::OwnedRef(cv));
  std::move(cb5).Run();
  EXPECT_EQ(cv, 1);

  const int& cref = v;
  auto cb6 = BindRepeating(SetFromRef, base::OwnedRef(cref));
  std::move(cb6).Run();
  EXPECT_EQ(cref, 1);

  auto cb7 = BindRepeating(SetFromRef, base::OwnedRef(1));
  std::move(cb7).Run();
}

// Functions that take reference parameters.
//  - Forced reference parameter type still stores a copy.
//  - Forced const reference parameter type still stores a copy.
TEST_F(BindTest, ReferenceArgumentBindingForRepeating) {
  int n = 1;
  int& ref_n = n;
  const int& const_ref_n = n;

  RepeatingCallback<int()> ref_copies_cb = BindRepeating(&Identity, ref_n);
  EXPECT_EQ(n, ref_copies_cb.Run());
  n++;
  EXPECT_EQ(n - 1, ref_copies_cb.Run());

  RepeatingCallback<int()> const_ref_copies_cb =
      BindRepeating(&Identity, const_ref_n);
  EXPECT_EQ(n, const_ref_copies_cb.Run());
  n++;
  EXPECT_EQ(n - 1, const_ref_copies_cb.Run());
}

TEST_F(BindTest, ReferenceArgumentBindingForOnce) {
  int n = 1;
  int& ref_n = n;
  const int& const_ref_n = n;

  OnceCallback<int()> ref_copies_cb = BindOnce(&Identity, ref_n);
  n++;
  EXPECT_EQ(n - 1, std::move(ref_copies_cb).Run());

  OnceCallback<int()> const_ref_copies_cb =
      BindOnce(&Identity, const_ref_n);
  n++;
  EXPECT_EQ(n - 1, std::move(const_ref_copies_cb).Run());
}

// Check that we can pass in arrays and have them be stored as a pointer.
//  - Array of values stores a pointer.
//  - Array of const values stores a pointer.
TEST_F(BindTest, ArrayArgumentBindingForRepeating) {
  int array[4] = {1, 1, 1, 1};
  const int (*const_array_ptr)[4] = &array;

  RepeatingCallback<int()> array_cb = BindRepeating(&ArrayGet, array, 1);
  EXPECT_EQ(1, array_cb.Run());

  RepeatingCallback<int()> const_array_cb =
      BindRepeating(&ArrayGet, *const_array_ptr, 1);
  EXPECT_EQ(1, const_array_cb.Run());

  array[1] = 3;
  EXPECT_EQ(3, array_cb.Run());
  EXPECT_EQ(3, const_array_cb.Run());
}

TEST_F(BindTest, ArrayArgumentBindingForOnce) {
  int array[4] = {1, 1, 1, 1};
  const int (*const_array_ptr)[4] = &array;

  OnceCallback<int()> array_cb = BindOnce(&ArrayGet, array, 1);
  OnceCallback<int()> const_array_cb =
      BindOnce(&ArrayGet, *const_array_ptr, 1);

  array[1] = 3;
  EXPECT_EQ(3, std::move(array_cb).Run());
  EXPECT_EQ(3, std::move(const_array_cb).Run());
}

// WeakPtr() support.
//   - Method bound to WeakPtr<> to non-const object.
//   - Const method bound to WeakPtr<> to non-const object.
//   - Const method bound to WeakPtr<> to const object.
//   - Normal Function with WeakPtr<> as P1 can have return type and is
//     not canceled.
TEST_F(BindTest, WeakPtrForRepeating) {
  EXPECT_CALL(no_ref_, VoidMethod0());
  EXPECT_CALL(no_ref_, VoidConstMethod0()).Times(2);

  WeakPtrFactory<NoRef> weak_factory(&no_ref_);
  WeakPtrFactory<const NoRef> const_weak_factory(const_no_ref_ptr_);

  RepeatingClosure method_cb =
      BindRepeating(&NoRef::VoidMethod0, weak_factory.GetWeakPtr());
  method_cb.Run();

  RepeatingClosure const_method_cb =
      BindRepeating(&NoRef::VoidConstMethod0, const_weak_factory.GetWeakPtr());
  const_method_cb.Run();

  RepeatingClosure const_method_const_ptr_cb =
      BindRepeating(&NoRef::VoidConstMethod0, const_weak_factory.GetWeakPtr());
  const_method_const_ptr_cb.Run();

  RepeatingCallback<int(int)> normal_func_cb =
      BindRepeating(&FunctionWithWeakFirstParam, weak_factory.GetWeakPtr());
  EXPECT_EQ(1, normal_func_cb.Run(1));

  weak_factory.InvalidateWeakPtrs();
  const_weak_factory.InvalidateWeakPtrs();

  method_cb.Run();
  const_method_cb.Run();
  const_method_const_ptr_cb.Run();

  // Still runs even after the pointers are invalidated.
  EXPECT_EQ(2, normal_func_cb.Run(2));
}

TEST_F(BindTest, WeakPtrForOnce) {
  WeakPtrFactory<NoRef> weak_factory(&no_ref_);
  WeakPtrFactory<const NoRef> const_weak_factory(const_no_ref_ptr_);

  OnceClosure method_cb =
      BindOnce(&NoRef::VoidMethod0, weak_factory.GetWeakPtr());
  OnceClosure const_method_cb =
      BindOnce(&NoRef::VoidConstMethod0, const_weak_factory.GetWeakPtr());
  OnceClosure const_method_const_ptr_cb =
      BindOnce(&NoRef::VoidConstMethod0, const_weak_factory.GetWeakPtr());
  OnceCallback<int(int)> normal_func_cb =
      BindOnce(&FunctionWithWeakFirstParam, weak_factory.GetWeakPtr());

  weak_factory.InvalidateWeakPtrs();
  const_weak_factory.InvalidateWeakPtrs();

  std::move(method_cb).Run();
  std::move(const_method_cb).Run();
  std::move(const_method_const_ptr_cb).Run();

  // Still runs even after the pointers are invalidated.
  EXPECT_EQ(2, std::move(normal_func_cb).Run(2));
}

// std::cref() wrapper support.
//   - Binding w/o std::cref takes a copy.
//   - Binding a std::cref takes a reference.
//   - Binding std::cref to a function std::cref does not copy on invoke.
TEST_F(BindTest, StdCrefForRepeating) {
  int n = 1;

  RepeatingCallback<int()> copy_cb = BindRepeating(&Identity, n);
  RepeatingCallback<int()> const_ref_cb =
      BindRepeating(&Identity, std::cref(n));
  EXPECT_EQ(n, copy_cb.Run());
  EXPECT_EQ(n, const_ref_cb.Run());
  n++;
  EXPECT_EQ(n - 1, copy_cb.Run());
  EXPECT_EQ(n, const_ref_cb.Run());

  int copies = 0;
  int assigns = 0;
  int move_constructs = 0;
  int move_assigns = 0;
  CopyMoveCounter counter(&copies, &assigns, &move_constructs, &move_assigns);
  RepeatingCallback<int()> all_const_ref_cb =
      BindRepeating(&GetCopies, std::cref(counter));
  EXPECT_EQ(0, all_const_ref_cb.Run());
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(0, move_constructs);
  EXPECT_EQ(0, move_assigns);
}

TEST_F(BindTest, StdCrefForOnce) {
  int n = 1;

  OnceCallback<int()> copy_cb = BindOnce(&Identity, n);
  OnceCallback<int()> const_ref_cb = BindOnce(&Identity, std::cref(n));
  n++;
  EXPECT_EQ(n - 1, std::move(copy_cb).Run());
  EXPECT_EQ(n, std::move(const_ref_cb).Run());

  int copies = 0;
  int assigns = 0;
  int move_constructs = 0;
  int move_assigns = 0;
  CopyMoveCounter counter(&copies, &assigns, &move_constructs, &move_assigns);
  OnceCallback<int()> all_const_ref_cb =
      BindOnce(&GetCopies, std::cref(counter));
  EXPECT_EQ(0, std::move(all_const_ref_cb).Run());
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(0, move_constructs);
  EXPECT_EQ(0, move_assigns);
}

// Test Owned() support.
TEST_F(BindTest, OwnedForRepeatingRawPtr) {
  int deletes = 0;
  DeleteCounter* counter = new DeleteCounter(&deletes);

  // If we don't capture, delete happens on Callback destruction/reset.
  // return the same value.
  RepeatingCallback<DeleteCounter*()> no_capture_cb =
      BindRepeating(&PolymorphicIdentity<DeleteCounter*>, Owned(counter));
  ASSERT_EQ(counter, no_capture_cb.Run());
  ASSERT_EQ(counter, no_capture_cb.Run());
  EXPECT_EQ(0, deletes);
  no_capture_cb.Reset();  // This should trigger a delete.
  EXPECT_EQ(1, deletes);

  deletes = 0;
  counter = new DeleteCounter(&deletes);
  RepeatingClosure own_object_cb =
      BindRepeating(&DeleteCounter::VoidMethod0, Owned(counter));
  own_object_cb.Run();
  EXPECT_EQ(0, deletes);
  own_object_cb.Reset();
  EXPECT_EQ(1, deletes);
}

TEST_F(BindTest, OwnedForOnceRawPtr) {
  int deletes = 0;
  DeleteCounter* counter = new DeleteCounter(&deletes);

  // If we don't capture, delete happens on Callback destruction/reset.
  // return the same value.
  OnceCallback<DeleteCounter*()> no_capture_cb =
      BindOnce(&PolymorphicIdentity<DeleteCounter*>, Owned(counter));
  EXPECT_EQ(0, deletes);
  no_capture_cb.Reset();  // This should trigger a delete.
  EXPECT_EQ(1, deletes);

  deletes = 0;
  counter = new DeleteCounter(&deletes);
  OnceClosure own_object_cb =
      BindOnce(&DeleteCounter::VoidMethod0, Owned(counter));
  EXPECT_EQ(0, deletes);
  own_object_cb.Reset();
  EXPECT_EQ(1, deletes);
}

TEST_F(BindTest, OwnedForRepeatingUniquePtr) {
  int deletes = 0;
  auto counter = std::make_unique<DeleteCounter>(&deletes);
  DeleteCounter* raw_counter = counter.get();

  // If we don't capture, delete happens on Callback destruction/reset.
  // return the same value.
  RepeatingCallback<DeleteCounter*()> no_capture_cb = BindRepeating(
      &PolymorphicIdentity<DeleteCounter*>, Owned(std::move(counter)));
  ASSERT_EQ(raw_counter, no_capture_cb.Run());
  ASSERT_EQ(raw_counter, no_capture_cb.Run());
  EXPECT_EQ(0, deletes);
  no_capture_cb.Reset();  // This should trigger a delete.
  EXPECT_EQ(1, deletes);

  deletes = 0;
  counter = std::make_unique<DeleteCounter>(&deletes);
  RepeatingClosure own_object_cb =
      BindRepeating(&DeleteCounter::VoidMethod0, Owned(std::move(counter)));
  own_object_cb.Run();
  EXPECT_EQ(0, deletes);
  own_object_cb.Reset();
  EXPECT_EQ(1, deletes);
}

TEST_F(BindTest, OwnedForOnceUniquePtr) {
  int deletes = 0;
  auto counter = std::make_unique<DeleteCounter>(&deletes);

  // If we don't capture, delete happens on Callback destruction/reset.
  // return the same value.
  OnceCallback<DeleteCounter*()> no_capture_cb =
      BindOnce(&PolymorphicIdentity<DeleteCounter*>, Owned(std::move(counter)));
  EXPECT_EQ(0, deletes);
  no_capture_cb.Reset();  // This should trigger a delete.
  EXPECT_EQ(1, deletes);

  deletes = 0;
  counter = std::make_unique<DeleteCounter>(&deletes);
  OnceClosure own_object_cb =
      BindOnce(&DeleteCounter::VoidMethod0, Owned(std::move(counter)));
  EXPECT_EQ(0, deletes);
  own_object_cb.Reset();
  EXPECT_EQ(1, deletes);
}

// Tests OwnedRef
TEST_F(BindTest, OwnedRefForCounter) {
  int counter = 0;
  RepeatingCallback<int()> counter_callback =
      BindRepeating([](int& counter) { return ++counter; }, OwnedRef(counter));

  EXPECT_EQ(1, counter_callback.Run());
  EXPECT_EQ(2, counter_callback.Run());
  EXPECT_EQ(3, counter_callback.Run());
  EXPECT_EQ(4, counter_callback.Run());

  EXPECT_EQ(0, counter);  // counter should remain unchanged.
}

TEST_F(BindTest, OwnedRefForIgnoringArguments) {
  OnceCallback<std::string(std::string)> echo_callback =
      BindOnce([](int& ignore, std::string s) { return s; }, OwnedRef(0));

  EXPECT_EQ("Hello World", std::move(echo_callback).Run("Hello World"));
}

template <typename T>
class BindVariantsTest : public ::testing::Test {
};

struct RepeatingTestConfig {
  template <typename Signature>
  using CallbackType = RepeatingCallback<Signature>;
  using ClosureType = RepeatingClosure;

  template <typename F, typename... Args>
  static CallbackType<internal::MakeUnboundRunType<F, Args...>> Bind(
      F&& f,
      Args&&... args) {
    return BindRepeating(std::forward<F>(f), std::forward<Args>(args)...);
  }
};

struct OnceTestConfig {
  template <typename Signature>
  using CallbackType = OnceCallback<Signature>;
  using ClosureType = OnceClosure;

  template <typename F, typename... Args>
  static CallbackType<internal::MakeUnboundRunType<F, Args...>> Bind(
      F&& f,
      Args&&... args) {
    return BindOnce(std::forward<F>(f), std::forward<Args>(args)...);
  }
};

using BindVariantsTestConfig = ::testing::Types<
  RepeatingTestConfig, OnceTestConfig>;
TYPED_TEST_SUITE(BindVariantsTest, BindVariantsTestConfig);

template <typename TypeParam, typename Signature>
using CallbackType = typename TypeParam::template CallbackType<Signature>;

// Function type support.
//   - Normal function.
//   - Normal function bound with non-refcounted first argument.
//   - Method bound to non-const object.
//   - Method bound to scoped_refptr.
//   - Const method bound to non-const object.
//   - Const method bound to const object.
//   - Derived classes can be used with pointers to non-virtual base functions.
//   - Derived classes can be used with pointers to virtual base functions (and
//     preserve virtual dispatch).
TYPED_TEST(BindVariantsTest, FunctionTypeSupport) {
  using ClosureType = typename TypeParam::ClosureType;

  StrictMock<HasRef> has_ref;
  StrictMock<NoRef> no_ref;
  StrictMock<NoRef> static_func_mock;
  const HasRef* const_has_ref_ptr = &has_ref;
  g_func_mock_ptr = &static_func_mock;

  EXPECT_CALL(static_func_mock, VoidMethod0());
  EXPECT_CALL(has_ref, AddRef()).Times(4);
  EXPECT_CALL(has_ref, Release()).Times(4);
  EXPECT_CALL(has_ref, HasAtLeastOneRef()).WillRepeatedly(Return(true));
  EXPECT_CALL(has_ref, VoidMethod0()).Times(2);
  EXPECT_CALL(has_ref, VoidConstMethod0()).Times(2);

  ClosureType normal_cb = TypeParam::Bind(&VoidFunc0);
  CallbackType<TypeParam, NoRef*()> normal_non_refcounted_cb =
      TypeParam::Bind(&PolymorphicIdentity<NoRef*>, &no_ref);
  std::move(normal_cb).Run();
  EXPECT_EQ(&no_ref, std::move(normal_non_refcounted_cb).Run());

  ClosureType method_cb = TypeParam::Bind(&HasRef::VoidMethod0, &has_ref);
  ClosureType method_refptr_cb =
      TypeParam::Bind(&HasRef::VoidMethod0, WrapRefCounted(&has_ref));
  ClosureType const_method_nonconst_obj_cb =
      TypeParam::Bind(&HasRef::VoidConstMethod0, &has_ref);
  ClosureType const_method_const_obj_cb =
      TypeParam::Bind(&HasRef::VoidConstMethod0, const_has_ref_ptr);
  std::move(method_cb).Run();
  std::move(method_refptr_cb).Run();
  std::move(const_method_nonconst_obj_cb).Run();
  std::move(const_method_const_obj_cb).Run();

  Child child;
  child.value = 0;
  ClosureType virtual_set_cb = TypeParam::Bind(&Parent::VirtualSet, &child);
  std::move(virtual_set_cb).Run();
  EXPECT_EQ(kChildValue, child.value);

  child.value = 0;
  ClosureType non_virtual_set_cb =
      TypeParam::Bind(&Parent::NonVirtualSet, &child);
  std::move(non_virtual_set_cb).Run();
  EXPECT_EQ(kParentValue, child.value);
}

// Return value support.
//   - Function with return value.
//   - Method with return value.
//   - Const method with return value.
//   - Move-only return value.
TYPED_TEST(BindVariantsTest, ReturnValues) {
  StrictMock<NoRef> static_func_mock;
  StrictMock<HasRef> has_ref;
  g_func_mock_ptr = &static_func_mock;
  const HasRef* const_has_ref_ptr = &has_ref;

  EXPECT_CALL(static_func_mock, IntMethod0()).WillOnce(Return(1337));
  EXPECT_CALL(has_ref, AddRef()).Times(4);
  EXPECT_CALL(has_ref, Release()).Times(4);
  EXPECT_CALL(has_ref, HasAtLeastOneRef()).WillRepeatedly(Return(true));
  EXPECT_CALL(has_ref, IntMethod0()).WillOnce(Return(31337));
  EXPECT_CALL(has_ref, IntConstMethod0())
      .WillOnce(Return(41337))
      .WillOnce(Return(51337));
  EXPECT_CALL(has_ref, UniquePtrMethod0())
      .WillOnce(Return(ByMove(std::make_unique<int>(42))));

  CallbackType<TypeParam, int()> normal_cb = TypeParam::Bind(&IntFunc0);
  CallbackType<TypeParam, int()> method_cb =
      TypeParam::Bind(&HasRef::IntMethod0, &has_ref);
  CallbackType<TypeParam, int()> const_method_nonconst_obj_cb =
      TypeParam::Bind(&HasRef::IntConstMethod0, &has_ref);
  CallbackType<TypeParam, int()> const_method_const_obj_cb =
      TypeParam::Bind(&HasRef::IntConstMethod0, const_has_ref_ptr);
  CallbackType<TypeParam, std::unique_ptr<int>()> move_only_rv_cb =
      TypeParam::Bind(&HasRef::UniquePtrMethod0, &has_ref);
  EXPECT_EQ(1337, std::move(normal_cb).Run());
  EXPECT_EQ(31337, std::move(method_cb).Run());
  EXPECT_EQ(41337, std::move(const_method_nonconst_obj_cb).Run());
  EXPECT_EQ(51337, std::move(const_method_const_obj_cb).Run());
  EXPECT_EQ(42, *std::move(move_only_rv_cb).Run());
}

// Argument binding tests.
//   - Argument binding to primitive.
//   - Argument binding to primitive pointer.
//   - Argument binding to a literal integer.
//   - Argument binding to a literal string.
//   - Argument binding with template function.
//   - Argument binding to an object.
//   - Argument binding to pointer to incomplete type.
//   - Argument gets type converted.
//   - Pointer argument gets converted.
//   - Const Reference forces conversion.
TYPED_TEST(BindVariantsTest, ArgumentBinding) {
  int n = 2;

  EXPECT_EQ(n, TypeParam::Bind(&Identity, n).Run());
  EXPECT_EQ(&n, TypeParam::Bind(&PolymorphicIdentity<int*>, &n).Run());
  EXPECT_EQ(3, TypeParam::Bind(&Identity, 3).Run());
  EXPECT_STREQ("hi", TypeParam::Bind(&CStringIdentity, "hi").Run());
  EXPECT_EQ(4, TypeParam::Bind(&PolymorphicIdentity<int>, 4).Run());

  NoRefParent p;
  p.value = 5;
  EXPECT_EQ(5, TypeParam::Bind(&UnwrapNoRefParent, p).Run());

  IncompleteType* incomplete_ptr = reinterpret_cast<IncompleteType*>(123);
  EXPECT_EQ(incomplete_ptr,
            TypeParam::Bind(&PolymorphicIdentity<IncompleteType*>,
                            incomplete_ptr).Run());

  NoRefChild c;
  c.value = 6;
  EXPECT_EQ(6, TypeParam::Bind(&UnwrapNoRefParent, c).Run());

  c.value = 7;
  EXPECT_EQ(7, TypeParam::Bind(&UnwrapNoRefParentPtr, &c).Run());

  c.value = 8;
  EXPECT_EQ(8, TypeParam::Bind(&UnwrapNoRefParentConstRef, c).Run());
}

// Unbound argument type support tests.
//   - Unbound value.
//   - Unbound pointer.
//   - Unbound reference.
//   - Unbound const reference.
//   - Unbound unsized array.
//   - Unbound sized array.
//   - Unbound array-of-arrays.
TYPED_TEST(BindVariantsTest, UnboundArgumentTypeSupport) {
  CallbackType<TypeParam, void(int)> unbound_value_cb =
      TypeParam::Bind(&VoidPolymorphic<int>::Run);
  CallbackType<TypeParam, void(int*)> unbound_pointer_cb =
      TypeParam::Bind(&VoidPolymorphic<int*>::Run);
  CallbackType<TypeParam, void(int&)> unbound_ref_cb =
      TypeParam::Bind(&VoidPolymorphic<int&>::Run);
  CallbackType<TypeParam, void(const int&)> unbound_const_ref_cb =
      TypeParam::Bind(&VoidPolymorphic<const int&>::Run);
  CallbackType<TypeParam, void(int[])> unbound_unsized_array_cb =
      TypeParam::Bind(&VoidPolymorphic<int[]>::Run);
  CallbackType<TypeParam, void(int[2])> unbound_sized_array_cb =
      TypeParam::Bind(&VoidPolymorphic<int[2]>::Run);
  CallbackType<TypeParam, void(int[][2])> unbound_array_of_arrays_cb =
      TypeParam::Bind(&VoidPolymorphic<int[][2]>::Run);
  CallbackType<TypeParam, void(int&)> unbound_ref_with_bound_arg =
      TypeParam::Bind(&VoidPolymorphic<int, int&>::Run, 1);
}

// Function with unbound reference parameter.
//   - Original parameter is modified by callback.
TYPED_TEST(BindVariantsTest, UnboundReferenceSupport) {
  int n = 0;
  CallbackType<TypeParam, void(int&)> unbound_ref_cb =
      TypeParam::Bind(&RefArgSet);
  std::move(unbound_ref_cb).Run(n);
  EXPECT_EQ(2, n);
}

// Unretained() wrapper support.
//   - Method bound to Unretained() non-const object.
//   - Const method bound to Unretained() non-const object.
//   - Const method bound to Unretained() const object.
TYPED_TEST(BindVariantsTest, Unretained) {
  StrictMock<NoRef> no_ref;
  const NoRef* const_no_ref_ptr = &no_ref;

  EXPECT_CALL(no_ref, VoidMethod0());
  EXPECT_CALL(no_ref, VoidConstMethod0()).Times(2);

  TypeParam::Bind(&NoRef::VoidMethod0, Unretained(&no_ref)).Run();
  TypeParam::Bind(&NoRef::VoidConstMethod0, Unretained(&no_ref)).Run();
  TypeParam::Bind(&NoRef::VoidConstMethod0, Unretained(const_no_ref_ptr)).Run();
}

TYPED_TEST(BindVariantsTest, ScopedRefptr) {
  StrictMock<HasRef> has_ref;
  EXPECT_CALL(has_ref, AddRef()).Times(1);
  EXPECT_CALL(has_ref, Release()).Times(1);
  EXPECT_CALL(has_ref, HasAtLeastOneRef()).WillRepeatedly(Return(true));

  const scoped_refptr<HasRef> refptr(&has_ref);
  CallbackType<TypeParam, int()> scoped_refptr_const_ref_cb = TypeParam::Bind(
      &FunctionWithScopedRefptrFirstParam, std::cref(refptr), 1);
  EXPECT_EQ(1, std::move(scoped_refptr_const_ref_cb).Run());
}

TYPED_TEST(BindVariantsTest, UniquePtrReceiver) {
  std::unique_ptr<StrictMock<NoRef>> no_ref(new StrictMock<NoRef>);
  EXPECT_CALL(*no_ref, VoidMethod0()).Times(1);
  TypeParam::Bind(&NoRef::VoidMethod0, std::move(no_ref)).Run();
}

// Tests for Passed() wrapper support:
//   - Passed() can be constructed from a pointer to scoper.
//   - Passed() can be constructed from a scoper rvalue.
//   - Using Passed() gives Callback Ownership.
//   - Ownership is transferred from Callback to callee on the first Run().
//   - Callback supports unbound arguments.
template <typename T>
class BindMoveOnlyTypeTest : public ::testing::Test {
};

struct CustomDeleter {
  void operator()(DeleteCounter* c) { delete c; }
};

using MoveOnlyTypesToTest =
    ::testing::Types<std::unique_ptr<DeleteCounter>,
                     std::unique_ptr<DeleteCounter, CustomDeleter>>;
TYPED_TEST_SUITE(BindMoveOnlyTypeTest, MoveOnlyTypesToTest);

TYPED_TEST(BindMoveOnlyTypeTest, PassedToBoundCallback) {
  int deletes = 0;

  TypeParam ptr(new DeleteCounter(&deletes));
  RepeatingCallback<TypeParam()> callback =
      BindRepeating(&PassThru<TypeParam>, Passed(&ptr));
  EXPECT_FALSE(ptr.get());
  EXPECT_EQ(0, deletes);

  // If we never invoke the Callback, it retains ownership and deletes.
  callback.Reset();
  EXPECT_EQ(1, deletes);
}

TYPED_TEST(BindMoveOnlyTypeTest, PassedWithRvalue) {
  int deletes = 0;
  RepeatingCallback<TypeParam()> callback = BindRepeating(
      &PassThru<TypeParam>, Passed(TypeParam(new DeleteCounter(&deletes))));
  EXPECT_EQ(0, deletes);

  // If we never invoke the Callback, it retains ownership and deletes.
  callback.Reset();
  EXPECT_EQ(1, deletes);
}

// Check that ownership can be transferred back out.
TYPED_TEST(BindMoveOnlyTypeTest, ReturnMoveOnlyType) {
  int deletes = 0;
  DeleteCounter* counter = new DeleteCounter(&deletes);
  RepeatingCallback<TypeParam()> callback =
      BindRepeating(&PassThru<TypeParam>, Passed(TypeParam(counter)));
  TypeParam result = callback.Run();
  ASSERT_EQ(counter, result.get());
  EXPECT_EQ(0, deletes);

  // Resetting does not delete since ownership was transferred.
  callback.Reset();
  EXPECT_EQ(0, deletes);

  // Ensure that we actually did get ownership.
  result.reset();
  EXPECT_EQ(1, deletes);
}

TYPED_TEST(BindMoveOnlyTypeTest, UnboundForwarding) {
  int deletes = 0;
  TypeParam ptr(new DeleteCounter(&deletes));
  // Test unbound argument forwarding.
  RepeatingCallback<TypeParam(TypeParam)> cb_unbound =
      BindRepeating(&PassThru<TypeParam>);
  cb_unbound.Run(std::move(ptr));
  EXPECT_EQ(1, deletes);
}

void VerifyVector(const std::vector<std::unique_ptr<int>>& v) {
  ASSERT_EQ(1u, v.size());
  EXPECT_EQ(12345, *v[0]);
}

std::vector<std::unique_ptr<int>> AcceptAndReturnMoveOnlyVector(
    std::vector<std::unique_ptr<int>> v) {
  VerifyVector(v);
  return v;
}

// Test that a vector containing move-only types can be used with Callback.
TEST_F(BindTest, BindMoveOnlyVector) {
  using MoveOnlyVector = std::vector<std::unique_ptr<int>>;

  MoveOnlyVector v;
  v.push_back(std::make_unique<int>(12345));

  // Early binding should work:
  base::RepeatingCallback<MoveOnlyVector()> bound_cb =
      base::BindRepeating(&AcceptAndReturnMoveOnlyVector, Passed(&v));
  MoveOnlyVector intermediate_result = bound_cb.Run();
  VerifyVector(intermediate_result);

  // As should passing it as an argument to Run():
  base::RepeatingCallback<MoveOnlyVector(MoveOnlyVector)> unbound_cb =
      base::BindRepeating(&AcceptAndReturnMoveOnlyVector);
  MoveOnlyVector final_result = unbound_cb.Run(std::move(intermediate_result));
  VerifyVector(final_result);
}

// Argument copy-constructor usage for non-reference copy-only parameters.
//   - Bound arguments are only copied once.
//   - Forwarded arguments are only copied once.
//   - Forwarded arguments with coercions are only copied twice (once for the
//     coercion, and one for the final dispatch).
TEST_F(BindTest, ArgumentCopies) {
  int copies = 0;
  int assigns = 0;

  CopyCounter counter(&copies, &assigns);
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run, counter);
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run,
                CopyCounter(&copies, &assigns));
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run).Run(counter);
  EXPECT_EQ(2, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run)
      .Run(CopyCounter(&copies, &assigns));
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  DerivedCopyMoveCounter derived(&copies, &assigns, nullptr, nullptr);
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run).Run(CopyCounter(derived));
  EXPECT_EQ(2, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyCounter>::Run)
      .Run(CopyCounter(
          DerivedCopyMoveCounter(&copies, &assigns, nullptr, nullptr)));
  EXPECT_EQ(2, copies);
  EXPECT_EQ(0, assigns);
}

// Argument move-constructor usage for move-only parameters.
//   - Bound arguments passed by move are not copied.
TEST_F(BindTest, ArgumentMoves) {
  int move_constructs = 0;
  int move_assigns = 0;

  BindRepeating(&VoidPolymorphic<const MoveCounter&>::Run,
                MoveCounter(&move_constructs, &move_assigns));
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  // TODO(tzik): Support binding move-only type into a non-reference parameter
  // of a variant of Callback.

  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<MoveCounter>::Run)
      .Run(MoveCounter(&move_constructs, &move_assigns));
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<MoveCounter>::Run)
      .Run(MoveCounter(DerivedCopyMoveCounter(
          nullptr, nullptr, &move_constructs, &move_assigns)));
  EXPECT_EQ(2, move_constructs);
  EXPECT_EQ(0, move_assigns);
}

// Argument constructor usage for non-reference movable-copyable
// parameters.
//   - Bound arguments passed by move are not copied.
//   - Forwarded arguments are only copied once.
//   - Forwarded arguments with coercions are only copied once and moved once.
TEST_F(BindTest, ArgumentCopiesAndMoves) {
  int copies = 0;
  int assigns = 0;
  int move_constructs = 0;
  int move_assigns = 0;

  CopyMoveCounter counter(&copies, &assigns, &move_constructs, &move_assigns);
  BindRepeating(&VoidPolymorphic<CopyMoveCounter>::Run, counter);
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(0, move_constructs);
  EXPECT_EQ(0, move_assigns);

  copies = 0;
  assigns = 0;
  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(
      &VoidPolymorphic<CopyMoveCounter>::Run,
      CopyMoveCounter(&copies, &assigns, &move_constructs, &move_assigns));
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  copies = 0;
  assigns = 0;
  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyMoveCounter>::Run).Run(counter);
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  copies = 0;
  assigns = 0;
  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyMoveCounter>::Run)
      .Run(CopyMoveCounter(&copies, &assigns, &move_constructs, &move_assigns));
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  DerivedCopyMoveCounter derived_counter(&copies, &assigns, &move_constructs,
                                         &move_assigns);
  copies = 0;
  assigns = 0;
  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyMoveCounter>::Run)
      .Run(CopyMoveCounter(derived_counter));
  EXPECT_EQ(1, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(1, move_constructs);
  EXPECT_EQ(0, move_assigns);

  copies = 0;
  assigns = 0;
  move_constructs = 0;
  move_assigns = 0;
  BindRepeating(&VoidPolymorphic<CopyMoveCounter>::Run)
      .Run(CopyMoveCounter(DerivedCopyMoveCounter(
          &copies, &assigns, &move_constructs, &move_assigns)));
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
  EXPECT_EQ(2, move_constructs);
  EXPECT_EQ(0, move_assigns);
}

TEST_F(BindTest, CapturelessLambda) {
  EXPECT_FALSE(internal::IsCallableObject<void>::value);
  EXPECT_FALSE(internal::IsCallableObject<int>::value);
  EXPECT_FALSE(internal::IsCallableObject<void (*)()>::value);
  EXPECT_FALSE(internal::IsCallableObject<void (NoRef::*)()>::value);

  auto f = []() {};
  EXPECT_TRUE(internal::IsCallableObject<decltype(f)>::value);

  int i = 0;
  auto g = [i]() { (void)i; };
  EXPECT_TRUE(internal::IsCallableObject<decltype(g)>::value);

  auto h = [](int, double) { return 'k'; };
  EXPECT_TRUE((std::is_same<
      char(int, double),
      internal::ExtractCallableRunType<decltype(h)>>::value));

  EXPECT_EQ(42, BindRepeating([] { return 42; }).Run());
  EXPECT_EQ(42, BindRepeating([](int i) { return i * 7; }, 6).Run());

  int x = 1;
  base::RepeatingCallback<void(int)> cb =
      BindRepeating([](int* x, int i) { *x *= i; }, Unretained(&x));
  cb.Run(6);
  EXPECT_EQ(6, x);
  cb.Run(7);
  EXPECT_EQ(42, x);
}

TEST_F(BindTest, EmptyFunctor) {
  struct NonEmptyFunctor {
    int operator()() const { return x; }
    int x = 42;
  };

  struct EmptyFunctor {
    int operator()() { return 42; }
  };

  struct EmptyFunctorConst {
    int operator()() const { return 42; }
  };

  EXPECT_TRUE(internal::IsCallableObject<NonEmptyFunctor>::value);
  EXPECT_TRUE(internal::IsCallableObject<EmptyFunctor>::value);
  EXPECT_TRUE(internal::IsCallableObject<EmptyFunctorConst>::value);
  EXPECT_EQ(42, BindOnce(EmptyFunctor()).Run());
  EXPECT_EQ(42, BindOnce(EmptyFunctorConst()).Run());
  EXPECT_EQ(42, BindRepeating(EmptyFunctorConst()).Run());
}

TEST_F(BindTest, CapturingLambdaForTesting) {
  // Test copyable lambdas.
  int x = 6;
  EXPECT_EQ(42, BindLambdaForTesting([=](int y) { return x * y; }).Run(7));
  EXPECT_EQ(42,
            BindLambdaForTesting([=](int y) mutable { return x *= y; }).Run(7));
  auto f = [x](std::unique_ptr<int> y) { return x * *y; };
  EXPECT_EQ(42, BindLambdaForTesting(f).Run(std::make_unique<int>(7)));

  // Test move-only lambdas.
  auto y = std::make_unique<int>(7);
  auto g = [y = std::move(y)](int& x) mutable {
    return x * *std::exchange(y, nullptr);
  };
  EXPECT_EQ(42, BindLambdaForTesting(std::move(g)).Run(x));

  y = std::make_unique<int>(7);
  auto h = [x, y = std::move(y)] { return x * *y; };
  EXPECT_EQ(42, BindLambdaForTesting(std::move(h)).Run());
}

TEST_F(BindTest, Cancellation) {
  EXPECT_CALL(no_ref_, VoidMethodWithIntArg(_)).Times(2);

  WeakPtrFactory<NoRef> weak_factory(&no_ref_);
  RepeatingCallback<void(int)> cb =
      BindRepeating(&NoRef::VoidMethodWithIntArg, weak_factory.GetWeakPtr());
  RepeatingClosure cb2 = BindRepeating(cb, 8);
  OnceClosure cb3 = BindOnce(cb, 8);

  OnceCallback<void(int)> cb4 =
      BindOnce(&NoRef::VoidMethodWithIntArg, weak_factory.GetWeakPtr());
  EXPECT_FALSE(cb4.IsCancelled());

  OnceClosure cb5 = BindOnce(std::move(cb4), 8);

  EXPECT_FALSE(cb.IsCancelled());
  EXPECT_FALSE(cb2.IsCancelled());
  EXPECT_FALSE(cb3.IsCancelled());
  EXPECT_FALSE(cb5.IsCancelled());

  cb.Run(6);
  cb2.Run();

  weak_factory.InvalidateWeakPtrs();

  EXPECT_TRUE(cb.IsCancelled());
  EXPECT_TRUE(cb2.IsCancelled());
  EXPECT_TRUE(cb3.IsCancelled());
  EXPECT_TRUE(cb5.IsCancelled());

  cb.Run(6);
  cb2.Run();
  std::move(cb3).Run();
  std::move(cb5).Run();
}

TEST_F(BindTest, OnceCallback) {
  // Check if Callback variants have declarations of conversions as expected.
  // Copy constructor and assignment of RepeatingCallback.
  static_assert(std::is_constructible<
      RepeatingClosure, const RepeatingClosure&>::value,
      "RepeatingClosure should be copyable.");
  static_assert(
      std::is_assignable<RepeatingClosure, const RepeatingClosure&>::value,
      "RepeatingClosure should be copy-assignable.");

  // Move constructor and assignment of RepeatingCallback.
  static_assert(std::is_constructible<
      RepeatingClosure, RepeatingClosure&&>::value,
      "RepeatingClosure should be movable.");
  static_assert(std::is_assignable<RepeatingClosure, RepeatingClosure&&>::value,
                "RepeatingClosure should be move-assignable");

  // Conversions from OnceCallback to RepeatingCallback.
  static_assert(!std::is_constructible<
      RepeatingClosure, const OnceClosure&>::value,
      "OnceClosure should not be convertible to RepeatingClosure.");
  static_assert(
      !std::is_assignable<RepeatingClosure, const OnceClosure&>::value,
      "OnceClosure should not be convertible to RepeatingClosure.");

  // Destructive conversions from OnceCallback to RepeatingCallback.
  static_assert(!std::is_constructible<
      RepeatingClosure, OnceClosure&&>::value,
      "OnceClosure should not be convertible to RepeatingClosure.");
  static_assert(!std::is_assignable<RepeatingClosure, OnceClosure&&>::value,
                "OnceClosure should not be convertible to RepeatingClosure.");

  // Copy constructor and assignment of OnceCallback.
  static_assert(!std::is_constructible<
      OnceClosure, const OnceClosure&>::value,
      "OnceClosure should not be copyable.");
  static_assert(!std::is_assignable<OnceClosure, const OnceClosure&>::value,
                "OnceClosure should not be copy-assignable");

  // Move constructor and assignment of OnceCallback.
  static_assert(std::is_constructible<
      OnceClosure, OnceClosure&&>::value,
      "OnceClosure should be movable.");
  static_assert(std::is_assignable<OnceClosure, OnceClosure&&>::value,
                "OnceClosure should be move-assignable.");

  // Conversions from RepeatingCallback to OnceCallback.
  static_assert(std::is_constructible<
      OnceClosure, const RepeatingClosure&>::value,
      "RepeatingClosure should be convertible to OnceClosure.");
  static_assert(std::is_assignable<OnceClosure, const RepeatingClosure&>::value,
                "RepeatingClosure should be convertible to OnceClosure.");

  // Destructive conversions from RepeatingCallback to OnceCallback.
  static_assert(std::is_constructible<
      OnceClosure, RepeatingClosure&&>::value,
      "RepeatingClosure should be convertible to OnceClosure.");
  static_assert(std::is_assignable<OnceClosure, RepeatingClosure&&>::value,
                "RepeatingClosure should be covretible to OnceClosure.");

  OnceClosure cb = BindOnce(&VoidPolymorphic<>::Run);
  std::move(cb).Run();

  // RepeatingCallback should be convertible to OnceCallback.
  OnceClosure cb2 = BindRepeating(&VoidPolymorphic<>::Run);
  std::move(cb2).Run();

  RepeatingClosure cb3 = BindRepeating(&VoidPolymorphic<>::Run);
  cb = cb3;
  std::move(cb).Run();

  cb = std::move(cb2);

  OnceCallback<void(int)> cb4 =
      BindOnce(&VoidPolymorphic<std::unique_ptr<int>, int>::Run,
               std::make_unique<int>(0));
  BindOnce(std::move(cb4), 1).Run();
}

// Callback construction and assignment tests.
//   - Construction from an InvokerStorageHolder should not cause ref/deref.
//   - Assignment from other callback should only cause one ref
//
// TODO(ajwong): Is there actually a way to test this?

#if defined(OS_WIN)
int __fastcall FastCallFunc(int n) {
  return n;
}

int __stdcall StdCallFunc(int n) {
  return n;
}

// Windows specific calling convention support.
//   - Can bind a __fastcall function.
//   - Can bind a __stdcall function.
//   - Can bind const and non-const __stdcall methods.
TEST_F(BindTest, WindowsCallingConventions) {
  auto fastcall_cb = BindRepeating(&FastCallFunc, 1);
  EXPECT_EQ(1, fastcall_cb.Run());

  auto stdcall_cb = BindRepeating(&StdCallFunc, 2);
  EXPECT_EQ(2, stdcall_cb.Run());

  class MethodHolder {
   public:
    int __stdcall Func(int n) { return n; }
    int __stdcall ConstFunc(int n) const { return -n; }
  };

  MethodHolder obj;
  auto stdcall_method_cb =
      BindRepeating(&MethodHolder::Func, base::Unretained(&obj), 1);
  EXPECT_EQ(1, stdcall_method_cb.Run());

  const MethodHolder const_obj;
  auto stdcall_const_method_cb =
      BindRepeating(&MethodHolder::ConstFunc, base::Unretained(&const_obj), 1);
  EXPECT_EQ(-1, stdcall_const_method_cb.Run());
}
#endif

// Test unwrapping the various wrapping functions.

TEST_F(BindTest, UnwrapUnretained) {
  int i = 0;
  auto unretained = Unretained(&i);
  EXPECT_EQ(&i, internal::Unwrap(unretained));
  EXPECT_EQ(&i, internal::Unwrap(std::move(unretained)));
}

TEST_F(BindTest, UnwrapRetainedRef) {
  auto p = MakeRefCounted<RefCountedData<int>>();
  auto retained_ref = RetainedRef(p);
  EXPECT_EQ(p.get(), internal::Unwrap(retained_ref));
  EXPECT_EQ(p.get(), internal::Unwrap(std::move(retained_ref)));
}

TEST_F(BindTest, UnwrapOwned) {
  {
    int* p = new int;
    auto owned = Owned(p);
    EXPECT_EQ(p, internal::Unwrap(owned));
    EXPECT_EQ(p, internal::Unwrap(std::move(owned)));
  }

  {
    auto p = std::make_unique<int>();
    int* raw_p = p.get();
    auto owned = Owned(std::move(p));
    EXPECT_EQ(raw_p, internal::Unwrap(owned));
    EXPECT_EQ(raw_p, internal::Unwrap(std::move(owned)));
  }
}

TEST_F(BindTest, UnwrapPassed) {
  int* p = new int;
  auto passed = Passed(WrapUnique(p));
  EXPECT_EQ(p, internal::Unwrap(passed).get());

  p = new int;
  EXPECT_EQ(p, internal::Unwrap(Passed(WrapUnique(p))).get());
}

TEST_F(BindTest, BindNoexcept) {
  EXPECT_EQ(42, base::BindOnce(&Noexcept).Run());
  EXPECT_EQ(
      42,
      base::BindOnce(&BindTest::NoexceptMethod, base::Unretained(this)).Run());
  EXPECT_EQ(
      42, base::BindOnce(&BindTest::ConstNoexceptMethod, base::Unretained(this))
              .Run());
}

// Test null callbacks cause a DCHECK.
TEST(BindDeathTest, NullCallback) {
  base::RepeatingCallback<void(int)> null_cb;
  ASSERT_TRUE(null_cb.is_null());
  EXPECT_CHECK_DEATH(base::BindRepeating(null_cb, 42));
}

TEST(BindDeathTest, NullFunctionPointer) {
  void (*null_function)(int) = nullptr;
  EXPECT_DCHECK_DEATH(base::BindRepeating(null_function, 42));
}

TEST(BindDeathTest, NullCallbackWithoutBoundArgs) {
  base::OnceCallback<void(int)> null_cb;
  ASSERT_TRUE(null_cb.is_null());
  EXPECT_CHECK_DEATH(base::BindOnce(std::move(null_cb)));
}

TEST(BindDeathTest, BanFirstOwnerOfRefCountedType) {
  StrictMock<HasRef> has_ref;
  EXPECT_DCHECK_DEATH({
    EXPECT_CALL(has_ref, HasAtLeastOneRef()).WillOnce(Return(false));
    base::BindOnce(&HasRef::VoidMethod0, &has_ref);
  });
}

}  // namespace
}  // namespace base
