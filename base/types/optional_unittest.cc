// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

namespace {

// Object used to test complex object with std::optional<T> in addition of the
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

  ~NonTriviallyDestructibleDeletedCopyConstructor() = default;

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

static_assert(std::is_trivially_destructible_v<std::optional<int>>,
              "OptionalIsTriviallyDestructible");

static_assert(
    !std::is_trivially_destructible_v<std::optional<NonTriviallyDestructible>>,
    "OptionalIsTriviallyDestructible");

TEST(OptionalTest, DefaultConstructor) {
  {
    constexpr std::optional<float> o;
    EXPECT_FALSE(o);
  }

  {
    std::optional<std::string> o;
    EXPECT_FALSE(o);
  }

  {
    std::optional<TestObject> o;
    EXPECT_FALSE(o);
  }
}

TEST(OptionalTest, CopyConstructor) {
  {
    constexpr std::optional<float> first(0.1f);
    constexpr std::optional<float> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), 0.1f);
    EXPECT_EQ(first, other);
  }

  {
    std::optional<std::string> first("foo");
    std::optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    const std::optional<std::string> first("foo");
    std::optional<std::string> other(first);

    EXPECT_TRUE(other);
    EXPECT_EQ(other.value(), "foo");
    EXPECT_EQ(first, other);
  }

  {
    std::optional<TestObject> first(TestObject(3, 0.1));
    std::optional<TestObject> other(first);

    EXPECT_TRUE(!!other);
    EXPECT_TRUE(other.value() == TestObject(3, 0.1));
    EXPECT_TRUE(first == other);
  }
}

TEST(OptionalTest, ValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr std::optional<float> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    std::string value("foo");
    std::optional<std::string> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(value, o.value());
  }

  {
    TestObject value(3, 0.1);
    std::optional<TestObject> o(value);

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::COPY_CONSTRUCTED, o->state());
    EXPECT_EQ(value, o.value());
  }
}

TEST(OptionalTest, MoveConstructor) {
  // NOLINTBEGIN(bugprone-use-after-move)
  {
    constexpr std::optional<float> first(0.1f);
    constexpr std::optional<float> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), 0.1f);

    EXPECT_TRUE(first.has_value());
  }

  {
    std::optional<std::string> first("foo");
    std::optional<std::string> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ("foo", second.value());

    EXPECT_TRUE(first.has_value());
  }

  {
    std::optional<TestObject> first(TestObject(3, 0.1));
    std::optional<TestObject> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, second->state());
    EXPECT_TRUE(TestObject(3, 0.1) == second.value());

    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(TestObject::State::MOVED_FROM, first->state());
  }

  // Even if copy constructor is deleted, move constructor needs to work.
  // Note that it couldn't be constexpr.
  {
    std::optional<DeletedCopy> first(std::in_place, 42);
    std::optional<DeletedCopy> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    std::optional<DeletedMove> first(std::in_place, 42);
    std::optional<DeletedMove> second(std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }

  {
    std::optional<NonTriviallyDestructibleDeletedCopyConstructor> first(
        std::in_place, 42);
    std::optional<NonTriviallyDestructibleDeletedCopyConstructor> second(
        std::move(first));

    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42, second->foo());

    EXPECT_TRUE(first.has_value());
  }
  // NOLINTEND(bugprone-use-after-move)
}

TEST(OptionalTest, MoveValueConstructor) {
  {
    constexpr float value = 0.1f;
    constexpr std::optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    float value = 0.1f;
    std::optional<float> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(0.1f, o.value());
  }

  {
    std::string value("foo");
    std::optional<std::string> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ("foo", o.value());
  }

  {
    TestObject value(3, 0.1);
    std::optional<TestObject> o(std::move(value));

    EXPECT_TRUE(o);
    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, o->state());
    EXPECT_EQ(TestObject(3, 0.1), o.value());
  }
}

TEST(OptionalTest, ConvertingCopyConstructor) {
  {
    std::optional<int> first(1);
    std::optional<double> second(first);
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  { [[maybe_unused]] std::optional<int> o(1); }
}

TEST(OptionalTest, ConvertingMoveConstructor) {
  {
    std::optional<int> first(1);
    std::optional<double> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(1.0, second.value());
  }

  // Make sure explicit is not marked for convertible case.
  { [[maybe_unused]] std::optional<int> o(1); }

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

    std::optional<Test1> first(std::in_place, 42);
    std::optional<Test2> second(std::move(first));
    EXPECT_TRUE(second.has_value());
    EXPECT_EQ(42.0, second->bar());
  }
}

TEST(OptionalTest, ConstructorForwardArguments) {
  {
    constexpr std::optional<float> a(std::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    std::optional<float> a(std::in_place, 0.1f);
    EXPECT_TRUE(a);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    std::optional<std::string> a(std::in_place, "foo");
    EXPECT_TRUE(a);
    EXPECT_EQ("foo", a.value());
  }

  {
    std::optional<TestObject> a(std::in_place, 0, 0.1);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(0, 0.1) == a.value());
  }
}

TEST(OptionalTest, ConstructorForwardInitListAndArguments) {
  {
    std::optional<std::vector<int>> opt(std::in_place, {3, 1});
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }

  {
    std::optional<std::vector<int>> opt(std::in_place, {3, 1},
                                        std::allocator<int>());
    EXPECT_TRUE(opt);
    EXPECT_THAT(*opt, ElementsAre(3, 1));
    EXPECT_EQ(2u, opt->size());
  }
}

TEST(OptionalTest, ForwardConstructor) {
  {
    std::optional<double> a(1);
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

    std::optional<TestData> a({1, 2.0, true});
    EXPECT_TRUE(a.has_value());
    EXPECT_EQ(1, a->a);
    EXPECT_EQ(2.0, a->b);
    EXPECT_TRUE(a->c);
  }

  // If T has a constructor with a param std::optional<U>, and another ctor
  // with a param U, then T(std::optional<U>) should be used for
  // std::optional<T>(std::optional<U>) constructor.
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
      explicit Test(std::in_place_t param) : param_type(ParamType::IN_PLACE) {}
      explicit Test(std::optional<int> param)
          : param_type(ParamType::OPTIONAL_INT) {}

      ParamType param_type;
    };

    // Overload resolution with copy-conversion constructor.
    {
      const std::optional<int> arg(std::in_place, 1);
      std::optional<Test> testee(arg);
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Overload resolution with move conversion constructor.
    {
      std::optional<Test> testee(std::optional<int>(std::in_place, 1));
      EXPECT_EQ(ParamType::OPTIONAL_INT, testee->param_type);
    }

    // Default constructor should be used.
    {
      std::optional<Test> testee(std::in_place);
      EXPECT_EQ(ParamType::DEFAULT_CONSTRUCTED, testee->param_type);
    }
  }

  {
    struct Test {
      Test(int a) {}  // NOLINT(runtime/explicit)
    };
    // If T is convertible from U, it is not marked as explicit.
    static_assert(std::is_convertible_v<int, Test>,
                  "Int should be convertible to Test.");
    ([](std::optional<Test> param) {})(1);
  }
}

TEST(OptionalTest, NulloptConstructor) {
  constexpr std::optional<int> a(std::nullopt);
  EXPECT_FALSE(a);
}

TEST(OptionalTest, AssignValue) {
  {
    std::optional<float> a;
    EXPECT_FALSE(a);
    a = 0.1f;
    EXPECT_TRUE(a);

    std::optional<float> b(0.1f);
    EXPECT_TRUE(a == b);
  }

  {
    std::optional<std::string> a;
    EXPECT_FALSE(a);
    a = std::string("foo");
    EXPECT_TRUE(a);

    std::optional<std::string> b(std::string("foo"));
    EXPECT_EQ(a, b);
  }

  {
    std::optional<TestObject> a;
    EXPECT_FALSE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    std::optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    std::optional<TestObject> a = TestObject(4, 1.0);
    EXPECT_TRUE(!!a);
    a = TestObject(3, 0.1);
    EXPECT_TRUE(!!a);

    std::optional<TestObject> b(TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }
}

TEST(OptionalTest, AssignObject) {
  {
    std::optional<float> a;
    std::optional<float> b(0.1f);
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), 0.1f);
    EXPECT_EQ(a, b);
  }

  {
    std::optional<std::string> a;
    std::optional<std::string> b("foo");
    a = b;

    EXPECT_TRUE(a);
    EXPECT_EQ(a.value(), "foo");
    EXPECT_EQ(a, b);
  }

  {
    std::optional<TestObject> a;
    std::optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    std::optional<TestObject> a(TestObject(4, 1.0));
    std::optional<TestObject> b(TestObject(3, 0.1));
    a = b;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(a.value() == TestObject(3, 0.1));
    EXPECT_TRUE(a == b);
  }

  {
    std::optional<DeletedMove> a(std::in_place, 42);
    std::optional<DeletedMove> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  {
    std::optional<DeletedMove> a(std::in_place, 42);
    std::optional<DeletedMove> b(std::in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(a->foo(), b->foo());
  }

  // Converting assignment.
  {
    std::optional<int> a(std::in_place, 1);
    std::optional<double> b;
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1, a.value());
    EXPECT_EQ(1.0, b.value());
  }

  {
    std::optional<int> a(std::in_place, 42);
    std::optional<double> b(std::in_place, 1);
    b = a;

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, a.value());
    EXPECT_EQ(42.0, b.value());
  }

  {
    std::optional<int> a;
    std::optional<double> b(std::in_place, 1);
    b = a;
    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
}

TEST(OptionalTest, AssignObject_rvalue) {
  // NOLINTBEGIN(bugprone-use-after-move)
  {
    std::optional<float> a;
    std::optional<float> b(0.1f);
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ(0.1f, a.value());
  }

  {
    std::optional<std::string> a;
    std::optional<std::string> b("foo");
    a = std::move(b);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ("foo", a.value());
  }

  {
    std::optional<TestObject> a;
    std::optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    std::optional<TestObject> a(TestObject(4, 1.0));
    std::optional<TestObject> b(TestObject(3, 0.1));
    a = std::move(b);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_TRUE(TestObject(3, 0.1) == a.value());

    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, a->state());
    EXPECT_EQ(TestObject::State::MOVED_FROM, b->state());
  }

  {
    std::optional<DeletedMove> a(std::in_place, 42);
    std::optional<DeletedMove> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  {
    std::optional<DeletedMove> a(std::in_place, 42);
    std::optional<DeletedMove> b(std::in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42, b->foo());
  }

  // Converting assignment.
  {
    std::optional<int> a(std::in_place, 1);
    std::optional<double> b;
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(1.0, b.value());
  }

  {
    std::optional<int> a(std::in_place, 42);
    std::optional<double> b(std::in_place, 1);
    b = std::move(a);

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(42.0, b.value());
  }

  {
    std::optional<int> a;
    std::optional<double> b(std::in_place, 1);
    b = std::move(a);

    EXPECT_FALSE(!!a);
    EXPECT_FALSE(!!b);
  }
  // NOLINTEND(bugprone-use-after-move)
}

TEST(OptionalTest, AssignNull) {
  {
    std::optional<float> a(0.1f);
    std::optional<float> b(0.2f);
    a = std::nullopt;
    b = std::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    std::optional<std::string> a("foo");
    std::optional<std::string> b("bar");
    a = std::nullopt;
    b = std::nullopt;
    EXPECT_EQ(a, b);
  }

  {
    std::optional<TestObject> a(TestObject(3, 0.1));
    std::optional<TestObject> b(TestObject(4, 1.0));
    a = std::nullopt;
    b = std::nullopt;
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

  // Here, std::optional<Test2> can be assigned from std::optional<Test1>.  In
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
    std::optional<Test1> a(std::in_place);
    std::optional<Test2> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test2> b(std::in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test2::State::COPY_ASSIGNED_FROM_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test2> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_CONSTRUCTED_FROM_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test2> b(std::in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test2::State::MOVE_ASSIGNED_FROM_TEST1, b->state);
  }

  // Similar to Test2, but Test3 also has copy/move ctor and assign operators
  // from std::optional<Test1>, too. In this case, for a = b where a is
  // std::optional<Test3> and b is std::optional<Test1>,
  // std::optional<T>::operator=(U&&) where U is std::optional<Test1> should
  // be used rather than std::optional<T>::operator=(std::optional<U>&&) where
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
    explicit Test3(const std::optional<Test1>& test1)
        : state(State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1) {}
    explicit Test3(std::optional<Test1>&& test1)
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
    Test3& operator=(const std::optional<Test1>& test1) {
      state = State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1;
      return *this;
    }
    Test3& operator=(std::optional<Test1>&& test1) {
      state = State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1;
      // In the following senarios, given |test1| should always have value.
      DCHECK(test1.has_value());
      test1->state = Test1::State::MOVED;
      return *this;
    }

    State state = State::DEFAULT_CONSTRUCTED;
  };

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test3> b;

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test3> b(std::in_place);

    b = a;
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::CONSTRUCTED, a->state);
    EXPECT_EQ(Test3::State::COPY_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test3> b;

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_CONSTRUCTED_FROM_OPTIONAL_TEST1, b->state);
  }

  {
    std::optional<Test1> a(std::in_place);
    std::optional<Test3> b(std::in_place);

    b = std::move(a);
    EXPECT_TRUE(!!a);
    EXPECT_TRUE(!!b);
    EXPECT_EQ(Test1::State::MOVED, a->state);
    EXPECT_EQ(Test3::State::MOVE_ASSIGNED_FROM_OPTIONAL_TEST1, b->state);
  }
}

TEST(OptionalTest, OperatorStar) {
  {
    std::optional<float> a(0.1f);
    EXPECT_EQ(a.value(), *a);
  }

  {
    std::optional<std::string> a("foo");
    EXPECT_EQ(a.value(), *a);
  }

  {
    std::optional<TestObject> a(TestObject(3, 0.1));
    EXPECT_EQ(a.value(), *a);
  }
}

TEST(OptionalTest, OperatorStar_rvalue) {
  EXPECT_EQ(0.1f, *std::optional<float>(0.1f));
  EXPECT_EQ(std::string("foo"), *std::optional<std::string>("foo"));
  EXPECT_TRUE(TestObject(3, 0.1) ==
              *std::optional<TestObject>(TestObject(3, 0.1)));
}

TEST(OptionalTest, OperatorArrow) {
  std::optional<TestObject> a(TestObject(3, 0.1));
  EXPECT_EQ(a->foo(), 3);
}

TEST(OptionalTest, Value_rvalue) {
  EXPECT_EQ(0.1f, std::optional<float>(0.1f).value());
  EXPECT_EQ(std::string("foo"), std::optional<std::string>("foo").value());
  EXPECT_TRUE(TestObject(3, 0.1) ==
              std::optional<TestObject>(TestObject(3, 0.1)).value());
}

TEST(OptionalTest, ValueOr) {
  {
    std::optional<float> a;
    EXPECT_EQ(0.0f, a.value_or(0.0f));

    a = 0.1f;
    EXPECT_EQ(0.1f, a.value_or(0.0f));

    a = std::nullopt;
    EXPECT_EQ(0.0f, a.value_or(0.0f));
  }

  // value_or() can be constexpr.
  {
    constexpr std::optional<int> a(std::in_place, 1);
    constexpr int value = a.value_or(10);
    EXPECT_EQ(1, value);
  }
  {
    constexpr std::optional<int> a;
    constexpr int value = a.value_or(10);
    EXPECT_EQ(10, value);
  }

  {
    std::optional<std::string> a;
    EXPECT_EQ("bar", a.value_or("bar"));

    a = std::string("foo");
    EXPECT_EQ(std::string("foo"), a.value_or("bar"));

    a = std::nullopt;
    EXPECT_EQ(std::string("bar"), a.value_or("bar"));
  }

  {
    std::optional<TestObject> a;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));

    a = TestObject(3, 0.1);
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(3, 0.1));

    a = std::nullopt;
    EXPECT_TRUE(a.value_or(TestObject(1, 0.3)) == TestObject(1, 0.3));
  }
}

TEST(OptionalTest, Swap_bothNoValue) {
  std::optional<TestObject> a, b;
  a.swap(b);

  EXPECT_FALSE(a);
  EXPECT_FALSE(b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_inHasValue) {
  std::optional<TestObject> a(TestObject(1, 0.3));
  std::optional<TestObject> b;
  a.swap(b);

  EXPECT_FALSE(a);

  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_outHasValue) {
  std::optional<TestObject> a;
  std::optional<TestObject> b(TestObject(1, 0.3));
  a.swap(b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, Swap_bothValue) {
  std::optional<TestObject> a(TestObject(0, 0.1));
  std::optional<TestObject> b(TestObject(1, 0.3));
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
    std::optional<float> a(0.1f);
    EXPECT_EQ(0.3f, a.emplace(0.3f));

    EXPECT_TRUE(a);
    EXPECT_EQ(0.3f, a.value());
  }

  {
    std::optional<std::string> a("foo");
    EXPECT_EQ("bar", a.emplace("bar"));

    EXPECT_TRUE(a);
    EXPECT_EQ("bar", a.value());
  }

  {
    std::optional<TestObject> a(TestObject(0, 0.1));
    EXPECT_EQ(TestObject(1, 0.2), a.emplace(TestObject(1, 0.2)));

    EXPECT_TRUE(!!a);
    EXPECT_TRUE(TestObject(1, 0.2) == a.value());
  }

  {
    std::optional<std::vector<int>> a;
    auto& ref = a.emplace({2, 3});
    static_assert(std::is_same_v<std::vector<int>&, decltype(ref)>, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(2, 3));
    EXPECT_EQ(&ref, &*a);
  }

  {
    std::optional<std::vector<int>> a;
    auto& ref = a.emplace({4, 5}, std::allocator<int>());
    static_assert(std::is_same_v<std::vector<int>&, decltype(ref)>, "");
    EXPECT_TRUE(a);
    EXPECT_THAT(*a, ElementsAre(4, 5));
    EXPECT_EQ(&ref, &*a);
  }
}

TEST(OptionalTest, Equals_TwoEmpty) {
  std::optional<int> a;
  std::optional<int> b;

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_TwoEquals) {
  std::optional<int> a(1);
  std::optional<int> b(1);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_OneEmpty) {
  std::optional<int> a;
  std::optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_TwoDifferent) {
  std::optional<int> a(0);
  std::optional<int> b(1);

  EXPECT_FALSE(a == b);
}

TEST(OptionalTest, Equals_DifferentType) {
  std::optional<int> a(0);
  std::optional<double> b(0);

  EXPECT_TRUE(a == b);
}

TEST(OptionalTest, Equals_Value) {
  std::optional<int> a(0);
  std::optional<int> b;

  EXPECT_TRUE(a == 0);
  EXPECT_FALSE(b == 0);
}

TEST(OptionalTest, NotEquals_TwoEmpty) {
  std::optional<int> a;
  std::optional<int> b;

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_TwoEquals) {
  std::optional<int> a(1);
  std::optional<int> b(1);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_OneEmpty) {
  std::optional<int> a;
  std::optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_TwoDifferent) {
  std::optional<int> a(0);
  std::optional<int> b(1);

  EXPECT_TRUE(a != b);
}

TEST(OptionalTest, NotEquals_DifferentType) {
  std::optional<int> a(0);
  std::optional<double> b(0.0);

  EXPECT_FALSE(a != b);
}

TEST(OptionalTest, NotEquals_Value) {
  std::optional<int> a(0);
  std::optional<int> b;

  EXPECT_TRUE(a != 1);
  EXPECT_FALSE(a == 1);

  EXPECT_TRUE(b != 1);
  EXPECT_FALSE(b == 1);
}

TEST(OptionalTest, Less_LeftEmpty) {
  std::optional<int> l;
  std::optional<int> r(1);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, Less_RightEmpty) {
  std::optional<int> l(1);
  std::optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothEmpty) {
  std::optional<int> l;
  std::optional<int> r;

  EXPECT_FALSE(l < r);
}

TEST(OptionalTest, Less_BothValues) {
  {
    std::optional<int> l(1);
    std::optional<int> r(2);

    EXPECT_TRUE(l < r);
  }
  {
    std::optional<int> l(2);
    std::optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
  {
    std::optional<int> l(1);
    std::optional<int> r(1);

    EXPECT_FALSE(l < r);
  }
}

TEST(OptionalTest, Less_DifferentType) {
  std::optional<int> l(1);
  std::optional<double> r(2.0);

  EXPECT_TRUE(l < r);
}

TEST(OptionalTest, LessEq_LeftEmpty) {
  std::optional<int> l;
  std::optional<int> r(1);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_RightEmpty) {
  std::optional<int> l(1);
  std::optional<int> r;

  EXPECT_FALSE(l <= r);
}

TEST(OptionalTest, LessEq_BothEmpty) {
  std::optional<int> l;
  std::optional<int> r;

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, LessEq_BothValues) {
  {
    std::optional<int> l(1);
    std::optional<int> r(2);

    EXPECT_TRUE(l <= r);
  }
  {
    std::optional<int> l(2);
    std::optional<int> r(1);

    EXPECT_FALSE(l <= r);
  }
  {
    std::optional<int> l(1);
    std::optional<int> r(1);

    EXPECT_TRUE(l <= r);
  }
}

TEST(OptionalTest, LessEq_DifferentType) {
  std::optional<int> l(1);
  std::optional<double> r(2.0);

  EXPECT_TRUE(l <= r);
}

TEST(OptionalTest, Greater_BothEmpty) {
  std::optional<int> l;
  std::optional<int> r;

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_LeftEmpty) {
  std::optional<int> l;
  std::optional<int> r(1);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, Greater_RightEmpty) {
  std::optional<int> l(1);
  std::optional<int> r;

  EXPECT_TRUE(l > r);
}

TEST(OptionalTest, Greater_BothValue) {
  {
    std::optional<int> l(1);
    std::optional<int> r(2);

    EXPECT_FALSE(l > r);
  }
  {
    std::optional<int> l(2);
    std::optional<int> r(1);

    EXPECT_TRUE(l > r);
  }
  {
    std::optional<int> l(1);
    std::optional<int> r(1);

    EXPECT_FALSE(l > r);
  }
}

TEST(OptionalTest, Greater_DifferentType) {
  std::optional<int> l(1);
  std::optional<double> r(2.0);

  EXPECT_FALSE(l > r);
}

TEST(OptionalTest, GreaterEq_BothEmpty) {
  std::optional<int> l;
  std::optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_LeftEmpty) {
  std::optional<int> l;
  std::optional<int> r(1);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, GreaterEq_RightEmpty) {
  std::optional<int> l(1);
  std::optional<int> r;

  EXPECT_TRUE(l >= r);
}

TEST(OptionalTest, GreaterEq_BothValue) {
  {
    std::optional<int> l(1);
    std::optional<int> r(2);

    EXPECT_FALSE(l >= r);
  }
  {
    std::optional<int> l(2);
    std::optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
  {
    std::optional<int> l(1);
    std::optional<int> r(1);

    EXPECT_TRUE(l >= r);
  }
}

TEST(OptionalTest, GreaterEq_DifferentType) {
  std::optional<int> l(1);
  std::optional<double> r(2.0);

  EXPECT_FALSE(l >= r);
}

TEST(OptionalTest, OptNullEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(opt == std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt == std::nullopt);
  }
}

TEST(OptionalTest, NullOptEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(std::nullopt == opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(std::nullopt == opt);
  }
}

TEST(OptionalTest, OptNullNotEq) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(opt != std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt != std::nullopt);
  }
}

TEST(OptionalTest, NullOptNotEq) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(std::nullopt != opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(std::nullopt != opt);
  }
}

TEST(OptionalTest, OptNullLower) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(opt < std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt < std::nullopt);
  }
}

TEST(OptionalTest, NullOptLower) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(std::nullopt < opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(std::nullopt < opt);
  }
}

TEST(OptionalTest, OptNullLowerEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(opt <= std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt <= std::nullopt);
  }
}

TEST(OptionalTest, NullOptLowerEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(std::nullopt <= opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(std::nullopt <= opt);
  }
}

TEST(OptionalTest, OptNullGreater) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(opt > std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt > std::nullopt);
  }
}

TEST(OptionalTest, NullOptGreater) {
  {
    std::optional<int> opt;
    EXPECT_FALSE(std::nullopt > opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(std::nullopt > opt);
  }
}

TEST(OptionalTest, OptNullGreaterEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(opt >= std::nullopt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt >= std::nullopt);
  }
}

TEST(OptionalTest, NullOptGreaterEq) {
  {
    std::optional<int> opt;
    EXPECT_TRUE(std::nullopt >= opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(std::nullopt >= opt);
  }
}

TEST(OptionalTest, ValueEq_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(opt == 1);
}

TEST(OptionalTest, ValueEq_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(opt == 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt == 1);
  }
}

TEST(OptionalTest, ValueEq_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(opt == 0.0);
}

TEST(OptionalTest, EqValue_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(1 == opt);
}

TEST(OptionalTest, EqValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(1 == opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(1 == opt);
  }
}

TEST(OptionalTest, EqValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(0.0 == opt);
}

TEST(OptionalTest, ValueNotEq_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(opt != 1);
}

TEST(OptionalTest, ValueNotEq_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(opt != 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt != 1);
  }
}

TEST(OptionalTest, ValueNotEq_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_FALSE(opt != 0.0);
}

TEST(OptionalTest, NotEqValue_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(1 != opt);
}

TEST(OptionalTest, NotEqValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(1 != opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(1 != opt);
  }
}

TEST(OptionalTest, NotEqValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_FALSE(0.0 != opt);
}

TEST(OptionalTest, ValueLess_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(opt < 1);
}

TEST(OptionalTest, ValueLess_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(opt < 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt < 1);
  }
  {
    std::optional<int> opt(2);
    EXPECT_FALSE(opt < 1);
  }
}

TEST(OptionalTest, ValueLess_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(opt < 1.0);
}

TEST(OptionalTest, LessValue_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(1 < opt);
}

TEST(OptionalTest, LessValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(1 < opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(1 < opt);
  }
  {
    std::optional<int> opt(2);
    EXPECT_TRUE(1 < opt);
  }
}

TEST(OptionalTest, LessValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_FALSE(0.0 < opt);
}

TEST(OptionalTest, ValueLessEq_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(opt <= 1);
}

TEST(OptionalTest, ValueLessEq_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(opt <= 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt <= 1);
  }
  {
    std::optional<int> opt(2);
    EXPECT_FALSE(opt <= 1);
  }
}

TEST(OptionalTest, ValueLessEq_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, LessEqValue_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(1 <= opt);
}

TEST(OptionalTest, LessEqValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(1 <= opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(1 <= opt);
  }
  {
    std::optional<int> opt(2);
    EXPECT_TRUE(1 <= opt);
  }
}

TEST(OptionalTest, LessEqValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(0.0 <= opt);
}

TEST(OptionalTest, ValueGreater_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(opt > 1);
}

TEST(OptionalTest, ValueGreater_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(opt > 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(opt > 1);
  }
  {
    std::optional<int> opt(2);
    EXPECT_TRUE(opt > 1);
  }
}

TEST(OptionalTest, ValueGreater_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_FALSE(opt > 0.0);
}

TEST(OptionalTest, GreaterValue_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(1 > opt);
}

TEST(OptionalTest, GreaterValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(1 > opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_FALSE(1 > opt);
  }
  {
    std::optional<int> opt(2);
    EXPECT_FALSE(1 > opt);
  }
}

TEST(OptionalTest, GreaterValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_FALSE(0.0 > opt);
}

TEST(OptionalTest, ValueGreaterEq_Empty) {
  std::optional<int> opt;
  EXPECT_FALSE(opt >= 1);
}

TEST(OptionalTest, ValueGreaterEq_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_FALSE(opt >= 1);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(opt >= 1);
  }
  {
    std::optional<int> opt(2);
    EXPECT_TRUE(opt >= 1);
  }
}

TEST(OptionalTest, ValueGreaterEq_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(opt <= 0.0);
}

TEST(OptionalTest, GreaterEqValue_Empty) {
  std::optional<int> opt;
  EXPECT_TRUE(1 >= opt);
}

TEST(OptionalTest, GreaterEqValue_NotEmpty) {
  {
    std::optional<int> opt(0);
    EXPECT_TRUE(1 >= opt);
  }
  {
    std::optional<int> opt(1);
    EXPECT_TRUE(1 >= opt);
  }
  {
    std::optional<int> opt(2);
    EXPECT_FALSE(1 >= opt);
  }
}

TEST(OptionalTest, GreaterEqValue_DifferentType) {
  std::optional<int> opt(0);
  EXPECT_TRUE(0.0 >= opt);
}

TEST(OptionalTest, NotEquals) {
  {
    std::optional<float> a(0.1f);
    std::optional<float> b(0.2f);
    EXPECT_NE(a, b);
  }

  {
    std::optional<std::string> a("foo");
    std::optional<std::string> b("bar");
    EXPECT_NE(a, b);
  }

  {
    std::optional<int> a(1);
    std::optional<double> b(2);
    EXPECT_NE(a, b);
  }

  {
    std::optional<TestObject> a(TestObject(3, 0.1));
    std::optional<TestObject> b(TestObject(4, 1.0));
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, NotEqualsNull) {
  {
    std::optional<float> a(0.1f);
    std::optional<float> b(0.1f);
    b = std::nullopt;
    EXPECT_NE(a, b);
  }

  {
    std::optional<std::string> a("foo");
    std::optional<std::string> b("foo");
    b = std::nullopt;
    EXPECT_NE(a, b);
  }

  {
    std::optional<TestObject> a(TestObject(3, 0.1));
    std::optional<TestObject> b(TestObject(3, 0.1));
    b = std::nullopt;
    EXPECT_TRUE(a != b);
  }
}

TEST(OptionalTest, MakeOptional) {
  {
    std::optional<float> o = std::make_optional(32.f);
    EXPECT_TRUE(o);
    EXPECT_EQ(32.f, *o);

    float value = 3.f;
    o = std::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(3.f, *o);
  }

  {
    std::optional<std::string> o = std::make_optional(std::string("foo"));
    EXPECT_TRUE(o);
    EXPECT_EQ("foo", *o);

    std::string value = "bar";
    o = std::make_optional(std::move(value));
    EXPECT_TRUE(o);
    EXPECT_EQ(std::string("bar"), *o);
  }

  {
    // NOLINTBEGIN(bugprone-use-after-move)
    std::optional<TestObject> o = std::make_optional(TestObject(3, 0.1));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(3, 0.1) == *o);

    TestObject value = TestObject(0, 0.42);
    o = std::make_optional(std::move(value));
    EXPECT_TRUE(!!o);
    EXPECT_TRUE(TestObject(0, 0.42) == *o);
    EXPECT_EQ(TestObject::State::MOVED_FROM, value.state());
    EXPECT_EQ(TestObject::State::MOVE_ASSIGNED, o->state());

    EXPECT_EQ(TestObject::State::MOVE_CONSTRUCTED,
              std::make_optional(std::move(value))->state());
    // NOLINTEND(bugprone-use-after-move)
  }

  {
    struct Test {
      Test(int a, double b, bool c) : a(a), b(b), c(c) {}

      int a;
      double b;
      bool c;
    };

    std::optional<Test> o = std::make_optional<Test>(1, 2.0, true);
    EXPECT_TRUE(!!o);
    EXPECT_EQ(1, o->a);
    EXPECT_EQ(2.0, o->b);
    EXPECT_TRUE(o->c);
  }

  {
    auto str1 = std::make_optional<std::string>({'1', '2', '3'});
    EXPECT_EQ("123", *str1);

    auto str2 = std::make_optional<std::string>({'a', 'b', 'c'},
                                                std::allocator<char>());
    EXPECT_EQ("abc", *str2);
  }
}

TEST(OptionalTest, NonMemberSwap_bothNoValue) {
  std::optional<TestObject> a, b;
  std::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_inHasValue) {
  std::optional<TestObject> a(TestObject(1, 0.3));
  std::optional<TestObject> b;
  std::swap(a, b);

  EXPECT_FALSE(!!a);
  EXPECT_TRUE(!!b);
  EXPECT_TRUE(TestObject(42, 0.42) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(1, 0.3) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_outHasValue) {
  std::optional<TestObject> a;
  std::optional<TestObject> b(TestObject(1, 0.3));
  std::swap(a, b);

  EXPECT_TRUE(!!a);
  EXPECT_FALSE(!!b);
  EXPECT_TRUE(TestObject(1, 0.3) == a.value_or(TestObject(42, 0.42)));
  EXPECT_TRUE(TestObject(42, 0.42) == b.value_or(TestObject(42, 0.42)));
}

TEST(OptionalTest, NonMemberSwap_bothValue) {
  std::optional<TestObject> a(TestObject(0, 0.1));
  std::optional<TestObject> b(TestObject(1, 0.3));
  std::swap(a, b);

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
    std::hash<std::optional<int>> opt_int_hash;

    EXPECT_EQ(int_hash(1), opt_int_hash(std::optional<int>(1)));
  }

  {
    std::hash<std::string> str_hash;
    std::hash<std::optional<std::string>> opt_str_hash;

    EXPECT_EQ(str_hash(std::string("foobar")),
              opt_str_hash(std::optional<std::string>(std::string("foobar"))));
  }
}

TEST(OptionalTest, Hash_NullOptEqualsNullOpt) {
  std::hash<std::optional<int>> opt_int_hash;
  std::hash<std::optional<std::string>> opt_str_hash;

  EXPECT_EQ(opt_str_hash(std::optional<std::string>()),
            opt_int_hash(std::optional<int>()));
}

TEST(OptionalTest, Hash_UseInSet) {
  std::set<std::optional<int>> setOptInt;

  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));

  setOptInt.insert(std::optional<int>(3));
  EXPECT_EQ(setOptInt.end(), setOptInt.find(42));
  EXPECT_NE(setOptInt.end(), setOptInt.find(3));
}

TEST(OptionalTest, HasValue) {
  std::optional<int> a;
  EXPECT_FALSE(a.has_value());

  a = 42;
  EXPECT_TRUE(a.has_value());

  a = std::nullopt;
  EXPECT_FALSE(a.has_value());

  a = 0;
  EXPECT_TRUE(a.has_value());

  a = std::optional<int>();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, Reset_int) {
  std::optional<int> a(0);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(0, a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(-1, a.value_or(-1));
}

TEST(OptionalTest, Reset_Object) {
  std::optional<TestObject> a(TestObject(0, 0.1));
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(TestObject(0, 0.1), a.value());

  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(TestObject(42, 0.0), a.value_or(TestObject(42, 0.0)));
}

TEST(OptionalTest, Reset_NoOp) {
  std::optional<int> a;
  EXPECT_FALSE(a.has_value());

  a.reset();
  EXPECT_FALSE(a.has_value());
}

TEST(OptionalTest, AssignFromRValue) {
  std::optional<TestObject> a;
  EXPECT_FALSE(a.has_value());

  TestObject obj;
  a = std::move(obj);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(1, a->move_ctors_count());
}

TEST(OptionalTest, DontCallDefaultCtor) {
  std::optional<DeletedDefaultConstructor> a;
  EXPECT_FALSE(a.has_value());

  a = std::make_optional<DeletedDefaultConstructor>(42);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(42, a->foo());
}

TEST(OptionalTest, DontCallNewMemberFunction) {
  std::optional<DeleteNewOperators> a;
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
    const std::optional<C> const_optional;
    EXPECT_DEATH_IF_SUPPORTED(const_optional.value(), "");
    EXPECT_DEATH_IF_SUPPORTED(const_optional->Method(), "");
    EXPECT_DEATH_IF_SUPPORTED(*const_optional, "");
    EXPECT_DEATH_IF_SUPPORTED(*std::move(const_optional), "");
  }

  {
    std::optional<C> non_const_optional;
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
      noexcept(std::optional<int>(std::declval<std::optional<int>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, trivial move)");
  static_assert(
      !noexcept(std::optional<Test1>(std::declval<std::optional<Test1>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (trivial copy)");
  static_assert(
      noexcept(std::optional<Test2>(std::declval<std::optional<Test2>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, trivial move)");
  static_assert(
      noexcept(std::optional<Test3>(std::declval<std::optional<Test3>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(trivial copy, non-trivial move)");
  static_assert(
      noexcept(std::optional<Test4>(std::declval<std::optional<Test4>>())),
      "move constructor for noexcept move-constructible T must be noexcept "
      "(non-trivial copy, non-trivial move)");
  static_assert(
      !noexcept(std::optional<Test5>(std::declval<std::optional<Test5>>())),
      "move constructor for non-noexcept move-constructible T must not be "
      "noexcept (non-trivial copy)");

  static_assert(noexcept(std::declval<std::optional<int>>() =
                             std::declval<std::optional<int>>()),
                "move assign for noexcept move-constructible/move-assignable T "
                "must be noexcept");
  static_assert(
      !noexcept(std::declval<std::optional<Test1>>() =
                    std::declval<std::optional<Test1>>()),
      "move assign for non-noexcept move-constructible T must not be noexcept");
  static_assert(
      !noexcept(std::declval<std::optional<Test2>>() =
                    std::declval<std::optional<Test2>>()),
      "move assign for non-noexcept move-assignable T must not be noexcept");
}

TEST(OptionalTest, OverrideAddressOf) {
  // Objects with an overloaded address-of should not trigger the overload for
  // arrow or copy assignment.
  static_assert(
      std::is_trivially_destructible_v<TriviallyDestructibleOverloadAddressOf>,
      "Trivially...AddressOf must be trivially destructible.");
  std::optional<TriviallyDestructibleOverloadAddressOf> optional;
  TriviallyDestructibleOverloadAddressOf n;
  optional = n;

  // operator->() should not call address-of either, for either const or non-
  // const calls.  It's not strictly necessary that we call a nonconst method
  // to test the non-const operator->(), but it makes it very clear that the
  // compiler can't chose the const operator->().
  optional->nonconst_method();
  const auto& const_optional = optional;
  const_optional->const_method();

  static_assert(!std::is_trivially_destructible_v<
                    NonTriviallyDestructibleOverloadAddressOf>,
                "NotTrivially...AddressOf must not be trivially destructible.");
  std::optional<NonTriviallyDestructibleOverloadAddressOf> nontrivial_optional;
  NonTriviallyDestructibleOverloadAddressOf n1;
  nontrivial_optional = n1;
}

}  // namespace base
