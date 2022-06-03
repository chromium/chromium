// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "base/macros.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::ElementsAre;

namespace base {

namespace {

// Object used to test complex object with absl::optional<T> in addition of the
// move semantics.
class TestObject {
 public:
  enum class State {
    DEFAULT_CONSTRUCTED,
    VALUE_CONSTRUCTED,
    COPY_CONSTRUCTED,
    MOVE_CONSTRUCTED,
    MOVED_FROM,
    COPY_ASSIGNED,
    MOVE_ASSIGNED,
    SWAPPED,
  };

  TestObject() : foo_(0), bar_(0.0), state_(State::DEFAULT_CONSTRUCTED) {}

  TestObject(int foo, double bar)
      : foo_(foo), bar_(bar), state_(State::VALUE_CONSTRUCTED) {}

  TestObject(const TestObject& other)
      : foo_(other.foo_),
        bar_(other.bar_),
        state_(State::COPY_CONSTRUCTED),
        move_ctors_count_(other.move_ctors_count_) {}

  TestObject(TestObject&& other)
      : foo_(std::move(other.foo_)),
        bar_(std::move(other.bar_)),
        state_(State::MOVE_CONSTRUCTED),
        move_ctors_count_(other.move_ctors_count_ + 1) {
    other.state_ = State::MOVED_FROM;
  }

  TestObject& operator=(const TestObject& other) {
    foo_ = other.foo_;
    bar_ = other.bar_;
    state_ = State::COPY_ASSIGNED;
    move_ctors_count_ = other.move_ctors_count_;
    return *this;
  }

  TestObject& operator=(TestObject&& other) {
    foo_ = other.foo_;
    bar_ = other.bar_;
    state_ = State::MOVE_ASSIGNED;
    move_ctors_count_ = other.move_ctors_count_;
    other.state_ = State::MOVED_FROM;
    return *this;
  }

  void Swap(TestObject* other) {
    using std::swap;
    swap(foo_, other->foo_);
    swap(bar_, other->bar_);
    swap(move_ctors_count_, other->move_ctors_count_);
    state_ = State::SWAPPED;
    other->state_ = State::SWAPPED;
  }

  bool operator==(const TestObject& other) const {
    return std::tie(foo_, bar_) == std::tie(other.foo_, other.bar_);
  }

  bool operator!=(const TestObject& other) const { return !(*this == other); }

  int foo() const { return foo_; }
  State state() const { return state_; }
  int move_ctors_count() const { return move_ctors_count_; }

 private:
  int foo_;
  double bar_;
  State state_;
  int move_ctors_count_ = 0;
};

// Implementing Swappable concept.
void swap(TestObject& lhs, TestObject& rhs) {
  lhs.Swap(&rhs);
}

class NonTriviallyDestructible {
 public:
  ~NonTriviallyDestructible() {}
};

class DeletedDefaultConstructor {
 public:
  DeletedDefaultConstructor() = delete;
  DeletedDefaultConstructor(int foo) : foo_(foo) {}

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeletedCopy {
 public:
  explicit DeletedCopy(int foo) : foo_(foo) {}
  DeletedCopy(const DeletedCopy&) = delete;
  DeletedCopy(DeletedCopy&&) = default;

  DeletedCopy& operator=(const DeletedCopy&) = delete;
  DeletedCopy& operator=(DeletedCopy&&) = default;

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeletedMove {
 public:
  explicit DeletedMove(int foo) : foo_(foo) {}
  DeletedMove(const DeletedMove&) = default;
  DeletedMove(DeletedMove&&) = delete;

  DeletedMove& operator=(const DeletedMove&) = default;
  DeletedMove& operator=(DeletedMove&&) = delete;

  int foo() const { return foo_; }

 private:
  int foo_;
};

class NonTriviallyDestructibleDeletedCopyConstructor {
 public:
  explicit NonTriviallyDestructibleDeletedCopyConstructor(int foo)
      : foo_(foo) {}
  NonTriviallyDestructibleDeletedCopyConstructor(
      const NonTriviallyDestructibleDeletedCopyConstructor&) = delete;
  NonTriviallyDestructibleDeletedCopyConstructor(
      NonTriviallyDestructibleDeletedCopyConstructor&&) = default;

  ~NonTriviallyDestructibleDeletedCopyConstructor() {}

  int foo() const { return foo_; }

 private:
  int foo_;
};

class DeleteNewOperators {
 public:
  void* operator new(size_t) = delete;
  void* operator new(size_t, void*) = delete;
  void* operator new[](size_t) = delete;
  void* operator new[](size_t, void*) = delete;
};

class TriviallyDestructibleOverloadAddressOf {
 public:
  // Unfortunately, since this can be called as part of placement-new (if it
  // forgets to call std::addressof), we're uninitialized.  So, about the best
  // we can do is signal a test failure here if either operator& is called.
  TriviallyDestructibleOverloadAddressOf* operator&() {
    EXPECT_TRUE(false);
    return this;
  }

  // So we can test the const version of operator->.
  const TriviallyDestructibleOverloadAddressOf* operator&() const {
    EXPECT_TRUE(false);
    return this;
  }

  void const_method() const {}
  void nonconst_method() {}
};

class NonTriviallyDestructibleOverloadAddressOf {
 public:
  ~NonTriviallyDestructibleOverloadAddressOf() {}
  NonTriviallyDestructibleOverloadAddressOf* operator&() {
    EXPECT_TRUE(false);
    return this;
  }
};

}  // anonymous namespace

static_assert(std::is_trivially_destructible<absl::optional<int>>::value,
              "OptionalIsTriviallyDestructible");

static_assert(!std::is_trivially_destructible<
                  absl::optional<NonTriviallyDestructible>>::value,
              "OptionalIsTriviallyDestructible");

TEST(OptionalTest, DefaultConstructor) {
  {
    constexpr absl::optional<float> o;
    EXPECT_FALSE(o);
  }

  {
    absl::optional<std::string> o;
    EXPECT_FALSE(o);
  }

  {
    absl::optional<TestObject> o;
    EXPECT_FALSE(o);
  }
}

TEST(OptionalTest, CopyConstructor) {
  {
    constexpr absl::optional<float> first(0.1f);
    constexpr absl::optional<float> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), 0.1f);
    EXPECT_EQ(first, other);
  }

  {
    absl::optional<std::string> first("foo");
    absl::optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    const absl::optional<std::string> first("foo");
    absl::optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    absl::optional<TestObject> first(TestObject(3, 0.1));
    absl::optional<TestObject> other(first);

    EXPECT_TRUE(!!other);
    EXPECT_TRUE(other.value() == TestObject(3, 0.1));
    EXPECT_TRUE(first == other);
  }
}

TEST(OptionalTest, ValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr absl::optional<float> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    std::string value("foo");
    absl::optional<std::string> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    TestObject value(3, 0.1);
    absl::optional<TestObject> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::COPY_CONSTRUCTED, o->state());
    EXPECT_EQ(value, o.value());
  }
}

TEST(OptionalTest, MoveConstructor) {
  {
    constexpr absl::optional<float> first(0.1f);
    constexpr absl::optional<float> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), 0.1f);

    EXPECT_TRUE(first.has_value());
  }

  {
    absl::optional<std::string> first("foo");
    absl::optional<std::string> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ("foo", second.value());

    EXPECT_TRUE(first.has_value());
  }

  {
    absl::optional<TestObject> first(TestObject(3, 0.1));
    absl::optional<TestObject> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, second->state());
    EXPECT_TRUE(TestObject(3, 0.1) == second.value());

    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(TestObject::State::MOVED_FROM, first->state());
  }

  // Even if copy constructor is deleted, move constructor needs to work.
  // Note that it couldn't be constexpr.
  {
    absl::optional<DeletedCopy> first(absl::in_place, 42);
    absl::optional<DeletedCopy> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    absl::optional<DeletedMove> first(absl::in_place, 42);
    absl::optional<DeletedMove> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    absl::optional<NonTriviallyDestructibleDeletedCopyConstructor> first(
        absl::in_place, 42);
    absl::optional<NonTriviallyDestructibleDeletedCopyConstructor> second(
        std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }
}

TEST(OptionalTest, MoveValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr absl::optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    float value = 0.1f;
    absl::optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    std::string value("foo");
    absl::optional<std::string> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ("foo", o.value());
  }

  {
    TestObject value(3, 0.1);
    absl::optional<TestObject> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, o->state());
    EXPECT_EQ(TestObject(3, 0.1), o.value());
  }
}

TEST(OptionalTest, ConvertingCopyConstructor) {
  {
    absl::optional<int> first(1);
    absl::optional<double> second(first);
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  {
    absl::optional<int> o(1);
    ignore_result<absl::optional<double>>(o);
  }
}

TEST(OptionalTest, ConvertingMoveConstructor) {
  {
    absl::optional<int> first(1);
    absl::optional<double> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  {
    absl::optional<int> o(1);
    ignore_result<absl::optional<double>>(std::move(o));
  }

  {
    class Test1 {
     public:
      explicit Test1(int foo) : foo_(foo) {}

      int foo() const { return foo_; }

     private:
      int foo_;
    };

    // Not copyable but convertible from Test1.
    class Test2 {
     public:
      Test2(const Test2&) = delete;
      explicit Test2(Test1&& other) : bar_(other.foo()) {}

      double bar() const { return bar_; }

     private:
      double bar_;
    };

    absl::optional<Test1> first(absl::in_place, 42);
    absl::optional<Test2> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42.0, second->bar());
  }
}

TEST(OptionalTest, ConstructorForwardArguments) {
  {
    constexpr absl::optional<float> a(absl::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    absl::optional<float> a(absl::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    absl::optional<std::string> a(absl::in_place, "foo");
    EXPECT_TRUE(a);
    EXPECT_EQ("foo", a.value());
  }

  {
    absl::optional<TestObject> a(absl::in_place, 0, 0.1);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(0, 0.1) == a.value());
  }
}

TEST(OptionalTest, ConstructorForwardInitListAndArguments) {
  {
    absl::optional<std::vector<int>> opt(absl::in_place, {3, 1});
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }

  {
    absl::optional<std::vector<int>> opt(absl::in_place, {3, 1},
                                         std::allocator<int>());
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }
}

TEST(OptionalTest, ForwardConstructor) {
  {
    absl::optional<double> a(1);
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(1.0, a.value());
  }

  // Test that default type of 'U' is value_type.
  {
    struct TestData {
      int a;
      double b;
      bool c;
    };

    absl::optional<TestData> a({1, 2.0, true});
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(1, a->a);
    EXPECT_EQ(2.0, a->b);
    EXPECT_TRUE(a->c);
  }

  // If T has a constructor with a param absl::optional<U>, and another ctor
  // with a param U, then T(absl::optional<U>) should be used for
  // absl::optional<T>(absl::optional<U>) constructor.
  {
    enum class ParamType {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED,
      MOVE_CONSTRUCTED,
      INT,
      IN_PLACE,
      OPTIONAL_INT,
    };
    struct Test {
      Test() : param_type(ParamType::DEFAULT_CONSTRUCTED) {}
      Test(const Test& param) : param_type(ParamType::COPY_CONSTRUCTED) {}
      Test(Test&& param) : param_type(ParamType::MOVE_CONSTRUCTED) {}
      explicit Test(int param) : param_type(ParamType::INT) {}
      explicit Test(in_place_t param) : param_type(ParamType::IN_PLACE) {}
      explicit Test(absl::optional<int> param)
          : param_type(ParamType::OPTIONAL_INT) {}

      ParamType param_type;
    };

    // Overload resolution with copy-conversion constructor.
    {
      const absl::optional<int> arg(absl::in_place, 1);
      absl::optional<Test> testee(arg);
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Overload resolution with move conversion constructor.
    {
      absl::optional<Test> testee(absl::optional<int>(absl::in_place, 1));
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Default constructor should be used.
    {
      absl::optional<Test> testee(absl::in_place);
      EXPECT_EQ(ParamType::DEFAULT_CONSTRUCTED, testee->param_type);
    }
  }

  {
    struct Test {
      Test(int a) {}  // NOLINT(runtime/explicit)
    };
    // If T is convertible from U, it is not marked as explicit.
    static_assert(std::is_convertible<int, Test>::value,
                  "Int should be convertible to Test.");
    ([](absl::optional<Test> param) {})(1);
  }
}

TEST(OptionalTest, NulloptConstructor) {
  constexpr absl::optional<int> a(absl::nullopt);
  EXPECT_FALSE(a);
}

TEST(OptionalTest, AssignValue) {
  {
    absl::optional<float> a;
    EXPECT_FALSE(a);
    a = 0.1f;
    EXPECT_TRUE(a);

    absl::optional<float> b(0.1f);
    EXPECT_TRUE(a == b);
  }

  {
    absl::optional<std::string> a;
    EXPECT_FALSE(a);
    a = std::string("foo");
    EXPECT_TRUE(a);

    absl::optional<std::string> b(std::string("foo"));
    EXPECT_EQ(a, b);
  }

  {
    absl::optional<TestObject> a;
    EXPECT_FALSE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    absl::optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    absl::optional<TestObject> a = TestObject(4, 1.0);
    EXPECT_TRUE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    absl::optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }
}

TEST(OptionalTest, AssignObject) {
  {
    absl::optional<float> a;
    absl::optional<float> b(0.1f);
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), 0.1f);
    EXPECT_EQ(a, b);
  }

  {
    absl::optional<std::string> a;
    absl::optional<std::string> b("foo");
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), "foo");
    EXPECT_EQ(a, b);
  }

  {
    absl::optional<TestObject> a;
    absl::optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    absl::optional<TestObject> a(TestObject(4, 1.0));
    absl::optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    absl::optional<DeletedMove> a(absl::in_place, 42);
    absl::optional<DeletedMove> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  {
    absl::optional<DeletedMove> a(absl::in_place, 42);
    absl::optional<DeletedMove> b(absl::in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  // Converting assignment.
  {
    absl::optional<int> a(absl::in_place, 1);
    absl::optional<double> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1, a.value());
    EXPECT_EQ(1.0, b.value());
  }

  {
    absl::optional<int> a(absl::in_place, 42);
    absl::optional<double> b(absl::in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, a.value());
    EXPECT_EQ(42.0, b.value());
  }

  {
    absl::optional<int> a;
    absl::optional<double> b(absl::in_place, 1);
    b = a;
    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
}

TEST(OptionalTest, AssignObject_rvalue) {
  {
    absl::optional<float> a;
    absl::optional<float> b(0.1f);
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    absl::optional<std::string> a;
    absl::optional<std::string> b("foo");
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ("foo", a.value());
  }

  {
    absl::optional<TestObject> a;
    absl::optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    absl::optional<TestObject> a(TestObject(4, 1.0));
    absl::optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    absl::optional<DeletedMove> a(absl::in_place, 42);
    absl::optional<DeletedMove> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  {
    absl::optional<DeletedMove> a(absl::in_place, 42);
    absl::optional<DeletedMove> b(absl::in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  // Converting assignment.
  {
    absl::optional<int> a(absl::in_place, 1);
    absl::optional<double> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1.0, b.value());
  }

  {
    absl::optional<int> a(absl::in_place, 42);
    absl::optional<double> b(absl::in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42.0, b.value());
  }

  {
    absl::optional<int> a;
    absl::optional<double> b(absl::in_place, 1);
    b = std::move(a);

    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
}

TEST(OptionalTest, AssignNull) {
  {
    absl::optional<float> a(0.1f);
    absl::optional<float> b(0.2f);
    a = absl::nullopt;
    b = absl::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    absl::optional<std::string> a("foo");
    absl::optional<std::string> b("bar");
    a = absl::nullopt;
    b = absl::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    absl::optional<TestObject> a(TestObject(3, 0.1));
    absl::optional<TestObject> b(TestObject(4, 1.0));
    a = absl::nullopt;
    b = absl::nullopt;
    EXPECT_TRUE(a == b);
  }
}

TEST(OptionalTest, AssignOverload) {
  struct Test1 {
    enum class State {
      CONSTRUCTED,
      MOVED,
    };
    State state = State::CONSTRUCTED;
  };

  // Here, absl::optional<Test2> can be assigned from absl::optional<Test1>.  In
  // case of move, marks MOVED to Test1 instance.
  struct Test2 {
    enum class State {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED_FROM_TEST1,
      MOVE_CONSTRUCTED_FROM_TEST1,
      COPY_ASSIGNED_FROM_TEST1,
      MOVE_ASSIGNED_FROM_TEST1,
    };

    Test2() = default;
    explicit Test2(const Test1& test1)
        : state(State::COPY_CONSTRUCTED_FROM_TEST1) {}
    explicit Test2(Test1&& test1) : state(State::MOVE_CONSTRUCTED_FROM_TEST1) {
      test1.state = Test1::State::MOVED;
    }
    Test2& operator=(const Test1& test1) {
      state = State::COPY_ASSIGNED_FROM_TEST1;
      return *this;
    }
    Test2& operator=(Test1&& test1) {
      state = State::MOVE_ASSIGNED_FROM_TEST1;
      test1.state = Test1::State::MOVED;
      return *this;
    }

    State state = State::DEFAULT_CONSTRUCTED;
  };

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test2> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test2> b(absl::in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_ASSIGNED_FROM_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test2> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test2> b(absl::in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_ASSIGNED_FROM_TEST1, b->state);
  }

  // Similar to Test2, but Test3 also has copy/move ctor and assign operators
  // from absl::optional<Test1>, too. In this case, for a = b where a is
  // absl::optional<Test3> and b is absl::optional<Test1>,
  // absl::optional<T>::operator=(U&&) where U is absl::optional<Test1> should
  // be used rather than absl::optional<T>::operator=(absl::optional<U>&&) where
  // U is Test1.
  struct Test3 {
    enum class State {
      DEFAULT_CONSTRUCTED,
      COPY_CONSTRUCTED_FROM_TEST1,
      MOVE_CONSTRUCTED_FROM_TEST1,
      COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1,
      MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1,
      COPY_ASSIGNED_FROM_TEST1,
      MOVE_ASSIGNED_FROM_TEST1,
      COPY_ASSIGNED_FROM_OPTIONAL_TEST1,
      MOVE_ASSIGNED_FROM_OPTIONAL_TEST1,
    };

    Test3() = default;
    explicit Test3(const Test1& test1)
        : state(State::COPY_CONSTRUCTED_FROM_TEST1) {}
    explicit Test3(Test1&& test1) : state(State::MOVE_CONSTRUCTED_FROM_TEST1) {
      test1.state = Test1::State::MOVED;
    }
    explicit Test3(const absl::optional<Test1>& test1)
        : state(State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1) {}
    explicit Test3(absl::optional<Test1>&& test1)
        : state(State::MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1) {
      // In the following senarios, given |test1| should always have value.
      DCHECK(test1.has_value());
      test1->state = Test1::State::MOVED;
    }
    Test3& operator=(const Test1& test1) {
      state = State::COPY_ASSIGNED_FROM_TEST1;
      return *this;
    }
    Test3& operator=(Test1&& test1) {
      state = State::MOVE_ASSIGNED_FROM_TEST1;
      test1.state = Test1::State::MOVED;
      return *this;
    }
    Test3& operator=(const absl::optional<Test1>& test1) {
      state = State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1;
      return *this;
    }
    Test3& operator=(absl::optional<Test1>&& test1) {
      state = State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1;
      // In the following senarios, given |test1| should always have value.
      DCHECK(test1.has_value());
      test1->state = Test1::State::MOVED;
      return *this;
    }

    State state = State::DEFAULT_CONSTRUCTED;
  };

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test3> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test3> b(absl::in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test3> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    absl::optional<Test1> a(absl::in_place);
    absl::optional<Test3> b(absl::in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }
}

TEST(OptionalTest, OperatorStar) {
  {
    absl::optional<float> a(0.1f);
    EXPECT_EQ(a.value(), *a);
  }

  {
    absl::optional<std::string> a("foo");
    EXPECT_EQ(a.value(), *a);
  }

  {
    absl::optional<TestObject> a(TestObject(3, 0.1));
    EXPECT_EQ(a.value(), *a);
  }
}

TEST(OptionalTest, OperatorStar_rvalue) {
  EXPECT_EQ(0.1f, *absl::optional<float>(0.1f));
  EXPECT_EQ(std::string("foo"), *absl::optional<std::string>("foo"));
  EXPECT_TRUE(TestObject(3, 0.1) ==
              *absl::optional<TestObject>(TestObject(3, 0.1)));
}

TEST(OptionalTest, OperatorArrow) {
  absl::optional<TestObject> a(TestObject(3, 0.1));
  EXPECT_EQ(a->foo(), 3);
}

TEST(OptionalTest, Value_rvalue) {
  EXPECT_EQ(0.1f, absl::optional<float>(0.1f).value());
  EXPECT_EQ(std::string("foo"), absl::optional<std::string>("foo").value());
  EXPECT_TRUE(TestObject(3, 0.1) ==
              absl::optional<TestObject>(TestObject(3, 0.1)).value());
}

TEST(OptionalTest, ValueOr) {
  {
    absl::optional<float> a;
    EXPECT_EQ(0.0f, a.value_or(0.0f));

    a = 0.1f;
    EXPECT_EQ(0.1f, a.value_or(0.0f));

    a = absl::nullopt;
    EXPECT_EQ(0.0f, a.value_or(0.0f));
  }

  // value_or() can be constexpr.
  {
    constexpr absl::optional<int> a(absl::in_place, 1);
    constexpr int value = a.value_or(10);
    EXPECT_EQ(1, value);
  }
  {
    constexpr absl::optional<int> a;
    constexpr int value = a.value_or(10);
    EXPECT_EQ(10, value);
  }

  {
    absl::optional<std::string> a;
    EXPECT_EQ("bar", a.value_or("bar"));

    a = std::string("foo");
    EXPECT_EQ(std::string("foo"), a.value_or("bar"));

    a = absl::nullopt;
    EXPECT_EQ(std::string("bar"), a.value_or("bar"));
  }

  {
    absl::optional<TestObject> a;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));

    a = TestObject(3, 0.1);
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(3, 0.1));

    a = absl::nullopt;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));
  }
}

TEST(OptionalTest, Swap_bothNoValue) {
  absl::optional<TestObject> a, b;
  a.swap(b);

  EXPECT_FALSE(a);
  EXPECT_FALSE(b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_inHasValue) {
  absl::optional<TestObject> a(TestObject(1, 0.3));
  absl::optional<TestObject> b;
  a.swap(b);

  EXPECT_FALSE(a);

  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_outHasValue) {
  absl::optional<TestObject> a;
  absl::optional<TestObject> b(TestObject(1, 0.3));
  a.swap(b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_bothValue) {
  absl::optional<TestObject> a(TestObject(0, 0.1));
  absl::optional<TestObject> b(TestObject(1, 0.3));
  a.swap(b);

  EXPECT_TRUE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(0, 0.1) == b.value_or(TestObject(42, 0.42)));
  EXPECT_EQ(TestObject::State::SWAPPED, a->state());
  EXPECT_EQ(TestObject::State::SWAPPED, b->state());
}

TEST(OptionalTest, Emplace) {
  {
    absl::optional<float> a(0.1f);
    EXPECT_EQ(0.3f, a.emplace(0.3f));

    EXPECT_TRUE(a);
    EXPECT_EQ(0.3f, a.value());
  }

  {
    absl::optional<std::string> a("foo");
    EXPECT_EQ("bar", a.emplace("bar"));

    EXPECT_TRUE(a);
    EXPECT_EQ("bar", a.value());
  }

  {
    absl::optional<TestObject> a(TestObject(0, 0.1));
    EXPECT_EQ(TestObject(1, 0.2), a.emplace(TestObject(1, 0.2)));

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(1, 0.2) == a.value());
  }

  {
    absl::optional<std::vector<int>> a;
    auto& ref = a.emplace({2, 3});
    static_assert(std::is_same<std::vector<int>&, decltype(ref)>::value, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(2, 3));
    EXPECT_EQ(&ref, &*a);
  }

  {
    absl::optional<std::vector<int>> a;
    auto& ref = a.emplace({4, 5}, std::allocator<int>());
    static_assert(std::is_same<std::vector<int>&, decltype(ref)>::value, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(4, 5));
    EXPECT_EQ(&ref, &*a);
  }
}

TEST(OptionalTest, Equals_TwoEmpty) {
  absl::optional<int> a;
  absl::optional<int> b;

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_TwoEquals) {
  absl::optional<int> a(1);
  absl::optional<int> b(1);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_OneEmpty) {
  absl::optional<int> a;
  absl::optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_TwoDifferent) {
  absl::optional<int> a(0);
  absl::optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_DifferentType) {
  absl::optional<int> a(0);
  absl::optional<double> b(0);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, NotEquals_TwoEmpty) {
  absl::optional<int> a;
  absl::optional<int> b;

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_TwoEquals) {
  absl::optional<int> a(1);
  absl::optional<int> b(1);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_OneEmpty) {
  absl::optional<int> a;
  absl::optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_TwoDifferent) {
  absl::optional<int> a(0);
  absl::optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_DifferentType) {
  absl::optional<int> a(0);
  absl::optional<double> b(0.0);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, Less_LeftEmpty) {
  absl::optional<int> l;
  absl::optional<int> r(1);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, Less_RightEmpty) {
  absl::optional<int> l(1);
  absl::optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothEmpty) {
  absl::optional<int> l;
  absl::optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothValues) {
  {
    absl::optional<int> l(1);
    absl::optional<int> r(2);

    EXPECT_TRUE(l < r);
  }
  {
    absl::optional<int> l(2);
    absl::optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
  {
    absl::optional<int> l(1);
    absl::optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
}

TEST(OptionalTest, Less_DifferentType) {
  absl::optional<int> l(1);
  absl::optional<double> r(2.0);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, LessEq_LeftEmpty) {
  absl::optional<int> l;
  absl::optional<int> r(1);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_RightEmpty) {
  absl::optional<int> l(1);
  absl::optional<int> r;

  EXPECT_FALSE(l <= r);
}

TEST(OptionalTest, LessEq_BothEmpty) {
  absl::optional<int> l;
  absl::optional<int> r;

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_BothValues) {
  {
    absl::optional<int> l(1);
    absl::optional<int> r(2);

    EXPECT_TRUE(l <= r);
  }
  {
    absl::optional<int> l(2);
    absl::optional<int> r(1);

    EXPECT_FALSE(l <= r);
  }
  {
    absl::optional<int> l(1);
    absl::optional<int> r(1);

    EXPECT_TRUE(l <= r);
  }
}

TEST(OptionalTest, LessEq_DifferentType) {
  absl::optional<int> l(1);
  absl::optional<double> r(2.0);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, Greater_BothEmpty) {
  absl::optional<int> l;
  absl::optional<int> r;

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_LeftEmpty) {
  absl::optional<int> l;
  absl::optional<int> r(1);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_RightEmpty) {
  absl::optional<int> l(1);
  absl::optional<int> r;

  EXPECT_TRUE(l > r);
}

TEST(OptionalTest, Greater_BothValue) {
  {
    absl::optional<int> l(1);
    absl::optional<int> r(2);

    EXPECT_FALSE(l > r);
  }
  {
    absl::optional<int> l(2);
    absl::optional<int> r(1);

    EXPECT_TRUE(l > r);
  }
  {
    absl::optional<int> l(1);
    absl::optional<int> r(1);

    EXPECT_FALSE(l > r);
  }
}

TEST(OptionalTest, Greater_DifferentType) {
  absl::optional<int> l(1);
  absl::optional<double> r(2.0);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, GreaterEq_BothEmpty) {
  absl::optional<int> l;
  absl::optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_LeftEmpty) {
  absl::optional<int> l;
  absl::optional<int> r(1);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, GreaterEq_RightEmpty) {
  absl::optional<int> l(1);
  absl::optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_BothValue) {
  {
    absl::optional<int> l(1);
    absl::optional<int> r(2);

    EXPECT_FALSE(l >= r);
  }
  {
    absl::optional<int> l(2);
    absl::optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
  {
    absl::optional<int> l(1);
    absl::optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
}

TEST(OptionalTest, GreaterEq_DifferentType) {
  absl::optional<int> l(1);
  absl::optional<double> r(2.0);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, OptNullEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(opt == absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt == absl::nullopt);
  }
}

TEST(OptionalTest, NullOptEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(absl::nullopt == opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(absl::nullopt == opt);
  }
}

TEST(OptionalTest, OptNullNotEq) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(opt != absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt != absl::nullopt);
  }
}

TEST(OptionalTest, NullOptNotEq) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(absl::nullopt != opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(absl::nullopt != opt);
  }
}

TEST(OptionalTest, OptNullLower) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(opt < absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt < absl::nullopt);
  }
}

TEST(OptionalTest, NullOptLower) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(absl::nullopt < opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(absl::nullopt < opt);
  }
}

TEST(OptionalTest, OptNullLowerEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(opt <= absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt <= absl::nullopt);
  }
}

TEST(OptionalTest, NullOptLowerEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(absl::nullopt <= opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(absl::nullopt <= opt);
  }
}

TEST(OptionalTest, OptNullGreater) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(opt > absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt > absl::nullopt);
  }
}

TEST(OptionalTest, NullOptGreater) {
  {
    absl::optional<int> opt;
    EXPECT_FALSE(absl::nullopt > opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(absl::nullopt > opt);
  }
}

TEST(OptionalTest, OptNullGreaterEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(opt >= absl::nullopt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt >= absl::nullopt);
  }
}

TEST(OptionalTest, NullOptGreaterEq) {
  {
    absl::optional<int> opt;
    EXPECT_TRUE(absl::nullopt >= opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(absl::nullopt >= opt);
  }
}

TEST(OptionalTest, ValueEq_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(opt == 1);
}

TEST(OptionalTest, ValueEq_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(opt == 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt == 1);
  }
}

TEST(OptionalTest, ValueEq_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(opt == 0.0);
}

TEST(OptionalTest, EqValue_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(1 == opt);
}

TEST(OptionalTest, EqValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(1 == opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(1 == opt);
  }
}

TEST(OptionalTest, EqValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(0.0 == opt);
}

TEST(OptionalTest, ValueNotEq_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(opt != 1);
}

TEST(OptionalTest, ValueNotEq_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(opt != 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt != 1);
  }
}

TEST(OptionalTest, ValueNotEq_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_FALSE(opt != 0.0);
}

TEST(OptionalTest, NotEqValue_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(1 != opt);
}

TEST(OptionalTest, NotEqValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(1 != opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(1 != opt);
  }
}

TEST(OptionalTest, NotEqValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_FALSE(0.0 != opt);
}

TEST(OptionalTest, ValueLess_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(opt < 1);
}

TEST(OptionalTest, ValueLess_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(opt < 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt < 1);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_FALSE(opt < 1);
  }
}

TEST(OptionalTest, ValueLess_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(opt < 1.0);
}

TEST(OptionalTest, LessValue_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(1 < opt);
}

TEST(OptionalTest, LessValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(1 < opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(1 < opt);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_TRUE(1 < opt);
  }
}

TEST(OptionalTest, LessValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_FALSE(0.0 < opt);
}

TEST(OptionalTest, ValueLessEq_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(opt <= 1);
}

TEST(OptionalTest, ValueLessEq_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(opt <= 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt <= 1);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_FALSE(opt <= 1);
  }
}

TEST(OptionalTest, ValueLessEq_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, LessEqValue_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(1 <= opt);
}

TEST(OptionalTest, LessEqValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(1 <= opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(1 <= opt);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_TRUE(1 <= opt);
  }
}

TEST(OptionalTest, LessEqValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(0.0 <= opt);
}

TEST(OptionalTest, ValueGreater_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(opt > 1);
}

TEST(OptionalTest, ValueGreater_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(opt > 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(opt > 1);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_TRUE(opt > 1);
  }
}

TEST(OptionalTest, ValueGreater_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_FALSE(opt > 0.0);
}

TEST(OptionalTest, GreaterValue_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(1 > opt);
}

TEST(OptionalTest, GreaterValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(1 > opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_FALSE(1 > opt);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_FALSE(1 > opt);
  }
}

TEST(OptionalTest, GreaterValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_FALSE(0.0 > opt);
}

TEST(OptionalTest, ValueGreaterEq_Empty) {
  absl::optional<int> opt;
  EXPECT_FALSE(opt >= 1);
}

TEST(OptionalTest, ValueGreaterEq_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_FALSE(opt >= 1);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(opt >= 1);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_TRUE(opt >= 1);
  }
}

TEST(OptionalTest, ValueGreaterEq_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, GreaterEqValue_Empty) {
  absl::optional<int> opt;
  EXPECT_TRUE(1 >= opt);
}

TEST(OptionalTest, GreaterEqValue_NotEmpty) {
  {
    absl::optional<int> opt(0);
    EXPECT_TRUE(1 >= opt);
  }
  {
    absl::optional<int> opt(1);
    EXPECT_TRUE(1 >= opt);
  }
  {
    absl::optional<int> opt(2);
    EXPECT_FALSE(1 >= opt);
  }
}

TEST(OptionalTest, GreaterEqValue_DifferentType) {
  absl::optional<int> opt(0);
  EXPECT_TRUE(0.0 >= opt);
}

TEST(OptionalTest, NotEquals) {
  {
    absl::optional<float> a(0.1f);
    absl::optional<float> b(0.2f);
    EXPECT_NE(a, b);
  }

  {
    absl::optional<std::string> a("foo");
    absl::optional<std::string> b("bar");
    EXPECT_NE(a, b);
  }

  {
    absl::optional<int> a(1);
    absl::optional<double> b(2);
    EXPECT_NE(a, b);
  }

  {
    absl::optional<TestObject> a(TestObject(3, 0.1));
    absl::optional<TestObject> b(TestObject(4, 1.0));
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, NotEqualsNull) {
  {
    absl::optional<float> a(0.1f);
    absl::optional<float> b(0.1f);
    b = absl::nullopt;
    EXPECT_NE(a, b);
  }

  {
    absl::optional<std::string> a("foo");
    absl::optional<std::string> b("foo");
    b = absl::nullopt;
    EXPECT_NE(a, b);
  }

  {
    absl::optional<TestObject> a(TestObject(3, 0.1));
    absl::optional<TestObject> b(TestObject(3, 0.1));
    b = absl::nullopt;
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, MakeOptional) {
  {
    absl::optional<float> o = absl::make_optional(32.f);
    EXPECT_TRUE(o);
    EXPECT_EQ(32.f, *o);

    float value = 3.f;
    o = absl::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(3.f, *o);
  }

  {
    absl::optional<std::string> o = absl::make_optional(std::string("foo"));
    EXPECT_TRUE(o);
    EXPECT_EQ("foo", *o);

    std::string value = "bar";
    o = absl::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(std::string("bar"), *o);
  }

  {
    absl::optional<TestObject> o = absl::make_optional(TestObject(3, 0.1));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(3, 0.1) == *o);

    TestObject value = TestObject(0, 0.42);
    o = absl::make_optional(std::move(value));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(0, 0.42) == *o);
    EXPECT_EQ(TestObject::State::MOVED_FROM, value.state());
    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, o->state());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED,
              absl::make_optional(std::move(value))->state());
  }

  {
    struct Test {
      Test(int a, double b, bool c) : a(a), b(b), c(c) {}

      int a;
      double b;
      bool c;
    };

    absl::optional<Test> o = absl::make_optional<Test>(1, 2.0, true);
    EXPECT_TRUE(!!o);
    EXPECT_EQ(1, o->a);
    EXPECT_EQ(2.0, o->b);
    EXPECT_TRUE(o->c);
  }

  {
    auto str1 = absl::make_optional<std::string>({'1', '2', '3'});
    EXPECT_EQ("123", *str1);

    auto str2 = absl::make_optional<std::string>({'a', 'b', 'c'},
                                                 std::allocator<char>());
    EXPECT_EQ("abc", *str2);
  }
}

TEST(OptionalTest, NonMemberSwap_bothNoValue) {
  absl::optional<TestObject> a, b;
  absl::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_inHasValue) {
  absl::optional<TestObject> a(TestObject(1, 0.3));
  absl::optional<TestObject> b;
  absl::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_outHasValue) {
  absl::optional<TestObject> a;
  absl::optional<TestObject> b(TestObject(1, 0.3));
  absl::swap(a, b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_bothValue) {
  absl::optional<TestObject> a(TestObject(0, 0.1));
  absl::optional<TestObject> b(TestObject(1, 0.3));
  absl::swap(a, b);

  EXPECT_TRUE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(0, 0.1) == b.value_or(TestObject(42, 0.42)));
  EXPECT_EQ(TestObject::State::SWAPPED, a->state());
  EXPECT_EQ(TestObject::State::SWAPPED, b->state());
}

TEST(OptionalTest, Hash_OptionalReflectsInternal) {
  {
    std::hash<int> int_hash;
    std::hash<absl::optional<int>> opt_int_hash;

    EXPECT_EQ(int_hash(1), opt_int_hash(absl::optional<int>(1)));
  }

  {
    std::hash<std::string> str_hash;
    std::hash<absl::optional<std::string>> opt_str_hash;

    EXPECT_EQ(str_hash(std::string("foobar")),
              opt_str_hash(absl::optional<std::string>(std::string("foobar"))));
  }
}

TEST(OptionalTest, Hash_NullOptEqualsNullOpt) {
  std::hash<absl::optional<int>> opt_int_hash;
  std::hash<absl::optional<std::string>> opt_str_hash;

  EXPECT_EQ(opt_str_hash(absl::optional<std::string>()),
            opt_int_hash(absl::optional<int>()));
}

TEST(OptionalTest, Hash_UseInSet) {
  std::set<absl::optional<int>> setOptInt;

  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));

  setOptInt.insert(absl::optional<int>(3));
  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));
  EXPECT_NE(setOptInt.end(), setOptInt.find(3));
}

TEST(OptionalTest, HasValue) {
  absl::optional<int> a;
  EXPECT_FALSE(a.has_value());

  a = 42;
  EXPECT_TRUE(a.has_value());

  a = absl::nullopt;
  EXPECT_FALSE(a.has_value());

  a = 0;
  EXPECT_TRUE(a.has_value());

  a = absl::optional<int>();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, Reset_int) {
  absl::optional<int> a(0);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(0, a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(-1, a.value_or(-1));
}

TEST(OptionalTest, Reset_Object) {
  absl::optional<TestObject> a(TestObject(0, 0.1));
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(TestObject(0, 0.1), a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(TestObject(42, 0.0), a.value_or(TestObject(42, 0.0)));
}

TEST(OptionalTest, Reset_NoOp) {
  absl::optional<int> a;
  EXPECT_FALSE(a.has_value());

  a.reset();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, AssignFromRValue) {
  absl::optional<TestObject> a;
  EXPECT_FALSE(a.has_value());

  TestObject obj;
  a = std::move(obj);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(1, a->move_ctors_count());
}

TEST(OptionalTest, DontCallDefaultCtor) {
  absl::optional<DeletedDefaultConstructor> a;
  EXPECT_FALSE(a.has_value());

  a = absl::make_optional<DeletedDefaultConstructor>(42);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(42, a->foo());
}

TEST(OptionalTest, DontCallNewMemberFunction) {
  absl::optional<DeleteNewOperators> a;
  EXPECT_FALSE(a.has_value());

  a = DeleteNewOperators();
  EXPECT_TRUE(a.has_value());
}

TEST(OptionalTest, DereferencingNoValueCrashes) {
  class C {
   public:
    void Method() const {}
  };

  {
    const absl::optional<C> const_optional;
    EXPECT_DEATH_IF_SUPPORTED(const_optional.value(), "");
    EXPECT_DEATH_IF_SUPPORTED(const_optional->Method(), "");
    EXPECT_DEATH_IF_SUPPORTED(*const_optional, "");
    EXPECT_DEATH_IF_SUPPORTED(*std::move(const_optional), "");
  }

  {
    absl::optional<C> non_const_optional;
    EXPECT_DEATH_IF_SUPPORTED(non_const_optional.value(), "");
    EXPECT_DEATH_IF_SUPPORTED(non_const_optional->Method(), "");
    EXPECT_DEATH_IF_SUPPORTED(*non_const_optional, "");
    EXPECT_DEATH_IF_SUPPORTED(*std::move(non_const_optional), "");
  }
}

TEST(OptionalTest, Noexcept) {
  // Trivial copy ctor, non-trivial move ctor, nothrow move assign.
  struct Test1 {
    Test1(const Test1&) = default;
    Test1(Test1&&) {}
    Test1& operator=(Test1&&) = default;
  };
  // Non-trivial copy ctor, trivial move ctor, throw move assign.
  struct Test2 {
    Test2(const Test2&) {}
    Test2(Test2&&) = default;
    Test2& operator=(Test2&&) { return *this; }
  };
  // Trivial copy ctor, non-trivial nothrow move ctor.
  struct Test3 {
    Test3(const Test3&) = default;
    Test3(Test3&&) noexcept {}
  };
  // Non-trivial copy ctor, non-trivial nothrow move ctor.
  struct Test4 {
    Test4(const Test4&) {}
    Test4(Test4&&) noexcept {}
  };
  // Non-trivial copy ctor, non-trivial move ctor.
  struct Test5 {
    Test5(const Test5&) {}
    Test5(Test5&&) {}
  };

  static_assert(
      noexcept(absl::optional<int>(std::declval<absl::optional<int>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, trivial move)");
  static_assert(
      !noexcept(absl::optional<Test1>(std::declval<absl::optional<Test1>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (trivial copy)");
  static_assert(
      noexcept(absl::optional<Test2>(std::declval<absl::optional<Test2>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, trivial move)");
  static_assert(
      noexcept(absl::optional<Test3>(std::declval<absl::optional<Test3>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, non-trivial move)");
  static_assert(
      noexcept(absl::optional<Test4>(std::declval<absl::optional<Test4>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, non-trivial move)");
  static_assert(
      !noexcept(absl::optional<Test5>(std::declval<absl::optional<Test5>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (non-trivial copy)");

  static_assert(noexcept(std::declval<absl::optional<int>>() =
                             std::declval<absl::optional<int>>()),
                "move assign for noexcept move-constructible/move-assignable T "
                "must be noexcept");
  static_assert(
      !noexcept(std::declval<absl::optional<Test1>>() =
                    std::declval<absl::optional<Test1>>()),
      "move assign for non-noexcept move-constructible T must not be noexcept");
  static_assert(
      !noexcept(std::declval<absl::optional<Test2>>() =
                    std::declval<absl::optional<Test2>>()),
      "move assign for non-noexcept move-assignable T must not be noexcept");
}

TEST(OptionalTest, OverrideAddressOf) {
  // Objects with an overloaded address-of should not trigger the overload for
  // arrow or copy assignment.
  static_assert(std::is_trivially_destructible<
                    TriviallyDestructibleOverloadAddressOf>::value,
                "Trivially...AddressOf must be trivially destructible.");
  absl::optional<TriviallyDestructibleOverloadAddressOf> optional;
  TriviallyDestructibleOverloadAddressOf n;
  optional = n;

  // operator->() should not call address-of either, for either const or non-
  // const calls.  It's not strictly necessary that we call a nonconst method
  // to test the non-const operator->(), but it makes it very clear that the
  // compiler can't chose the const operator->().
  optional->nonconst_method();
  const auto& const_optional = optional;
  const_optional->const_method();

  static_assert(!std::is_trivially_destructible<
                    NonTriviallyDestructibleOverloadAddressOf>::value,
                "NotTrivially...AddressOf must not be trivially destructible.");
  absl::optional<NonTriviallyDestructibleOverloadAddressOf> nontrivial_optional;
  NonTriviallyDestructibleOverloadAddressOf n1;
  nontrivial_optional = n1;
}

}  // namespace base
