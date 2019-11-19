// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"

#include "base/strings/string_piece.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class CheckedBase : public CheckedObserver {
 public:
  virtual void Observe(int x) = 0;
  ~CheckedBase() override = default;
  virtual int GetValue() const { return 0; }
};

class UncheckedBase {
 public:
  virtual void Observe(int x) = 0;
  virtual ~UncheckedBase() = default;
  virtual int GetValue() const { return 0; }
};

// Helper for TYPED_TEST_SUITE machinery to pick the ObserverList under test.
// Keyed off the observer type since ObserverList has too many template args and
// it gets ugly.
template <class Foo>
struct PickObserverList {};
template <>
struct PickObserverList<CheckedBase> {
  template <class TypeParam,
            bool check_empty = false,
            bool allow_reentrancy = true>
  using ObserverListType =
      ObserverList<TypeParam, check_empty, allow_reentrancy>;
};
template <>
struct PickObserverList<UncheckedBase> {
  template <class TypeParam,
            bool check_empty = false,
            bool allow_reentrancy = true>
  using ObserverListType = typename ObserverList<TypeParam,
                                                 check_empty,
                                                 allow_reentrancy>::Unchecked;
};

template <class Foo>
class AdderT : public Foo {
 public:
  explicit AdderT(int scaler) : total(0), scaler_(scaler) {}
  ~AdderT() override = default;

  void Observe(int x) override { total += x * scaler_; }
  int GetValue() const override { return total; }

  int total;

 private:
  int scaler_;
};

template <class ObserverListType,
          class Foo = typename ObserverListType::value_type>
class DisrupterT : public Foo {
 public:
  DisrupterT(ObserverListType* list, Foo* doomed, bool remove_self)
      : list_(list), doomed_(doomed), remove_self_(remove_self) {}
  DisrupterT(ObserverListType* list, Foo* doomed)
      : DisrupterT(list, doomed, false) {}
  DisrupterT(ObserverListType* list, bool remove_self)
      : DisrupterT(list, nullptr, remove_self) {}

  ~DisrupterT() override = default;

  void Observe(int x) override {
    if (remove_self_)
      list_->RemoveObserver(this);
    if (doomed_)
      list_->RemoveObserver(doomed_);
  }

  void SetDoomed(Foo* doomed) { doomed_ = doomed; }

 private:
  ObserverListType* list_;
  Foo* doomed_;
  bool remove_self_;
};

template <class ObserverListType,
          class Foo = typename ObserverListType::value_type>
class AddInObserve : public Foo {
 public:
  explicit AddInObserve(ObserverListType* observer_list)
      : observer_list(observer_list), to_add_() {}

  void SetToAdd(Foo* to_add) { to_add_ = to_add; }

  void Observe(int x) override {
    if (to_add_) {
      observer_list->AddObserver(to_add_);
      to_add_ = nullptr;
    }
  }

  ObserverListType* observer_list;
  Foo* to_add_;
};

template <class ObserverListType>
class ObserverListCreator : public DelegateSimpleThread::Delegate {
 public:
  std::unique_ptr<ObserverListType> Create(
      base::Optional<base::ObserverListPolicy> policy = nullopt) {
    policy_ = policy;
    DelegateSimpleThread thread(this, "ListCreator");
    thread.Start();
    thread.Join();
    return std::move(observer_list_);
  }

 private:
  void Run() override {
    if (policy_) {
      observer_list_ = std::make_unique<ObserverListType>(*policy_);
    } else {
      observer_list_ = std::make_unique<ObserverListType>();
    }
  }

  std::unique_ptr<ObserverListType> observer_list_;
  base::Optional<base::ObserverListPolicy> policy_;
};

}  // namespace

class ObserverListTestBase {
 public:
  ObserverListTestBase() {}

  template <class T>
  const decltype(T::list_.get()) list(const T& iter) {
    return iter.list_.get();
  }

  template <class T>
  typename T::value_type* GetCurrent(T* iter) {
    return iter->GetCurrent();
  }

  // Override GetCurrent() for CheckedObserver. When StdIteratorRemoveFront
  // tries to simulate a sequence to see if it "would" crash, CheckedObservers
  // do, actually, crash with a DCHECK(). Note this check is different to the
  // check during an observer _iteration_. Hence, DCHECK(), not CHECK().
  CheckedBase* GetCurrent(ObserverList<CheckedBase>::iterator* iter) {
    EXPECT_DCHECK_DEATH(return iter->GetCurrent());
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverListTestBase);
};

// Templatized test fixture that can pick between CheckedBase and UncheckedBase.
template <class ObserverType>
class ObserverListTest : public ObserverListTestBase, public ::testing::Test {
 public:
  template <class T>
  using ObserverList =
      typename PickObserverList<ObserverType>::template ObserverListType<T>;

  using iterator = typename ObserverList<ObserverType>::iterator;
  using const_iterator = typename ObserverList<ObserverType>::const_iterator;

  ObserverListTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverListTest);
};

using ObserverTypes = ::testing::Types<CheckedBase, UncheckedBase>;
TYPED_TEST_SUITE(ObserverListTest, ObserverTypes);

// TYPED_TEST causes the test parent class to be a template parameter, which
// makes the syntax for referring to the types awkward. Create aliases in local
// scope with clearer names. Unfortunately, we also need some trailing cruft to
// avoid "unused local type alias" warnings.
#define DECLARE_TYPES                                                       \
  using Foo = TypeParam;                                                    \
  using ObserverListFoo =                                                   \
      typename PickObserverList<TypeParam>::template ObserverListType<Foo>; \
  using Adder = AdderT<Foo>;                                                \
  using Disrupter = DisrupterT<ObserverListFoo>;                            \
  using const_iterator = typename TestFixture::const_iterator;              \
  using iterator = typename TestFixture::iterator;                          \
  (void)(Disrupter*)(0);                                                    \
  (void)(Adder*)(0);                                                        \
  (void)(const_iterator*)(0);                                               \
  (void)(iterator*)(0)

TYPED_TEST(ObserverListTest, BasicTest) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  const ObserverListFoo& const_observer_list = observer_list;

  {
    const const_iterator it1 = const_observer_list.begin();
    EXPECT_EQ(it1, const_observer_list.end());
    // Iterator copy.
    const const_iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    // Iterator assignment.
    const_iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
  }

  {
    const iterator it1 = observer_list.begin();
    EXPECT_EQ(it1, observer_list.end());
    // Iterator copy.
    const iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    // Iterator assignment.
    iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
  }

  Adder a(1), b(-1), c(1), d(-1), e(-1);
  Disrupter evil(&observer_list, &c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  EXPECT_TRUE(const_observer_list.HasObserver(&a));
  EXPECT_FALSE(const_observer_list.HasObserver(&c));

  {
    const const_iterator it1 = const_observer_list.begin();
    EXPECT_NE(it1, const_observer_list.end());
    // Iterator copy.
    const const_iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    EXPECT_NE(it2, const_observer_list.end());
    // Iterator assignment.
    const_iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Iterator post increment.
    const_iterator it4 = it3++;
    EXPECT_EQ(it4, it1);
    EXPECT_EQ(it4, it2);
    EXPECT_NE(it4, it3);
  }

  {
    const iterator it1 = observer_list.begin();
    EXPECT_NE(it1, observer_list.end());
    // Iterator copy.
    const iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    EXPECT_NE(it2, observer_list.end());
    // Iterator assignment.
    iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Iterator post increment.
    iterator it4 = it3++;
    EXPECT_EQ(it4, it1);
    EXPECT_EQ(it4, it2);
    EXPECT_NE(it4, it3);
  }

  for (auto& observer : observer_list)
    observer.Observe(10);

  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  // Removing an observer not in the list should do nothing.
  observer_list.RemoveObserver(&e);

  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-10, d.total);
  EXPECT_EQ(0, e.total);
}

TYPED_TEST(ObserverListTest, CreatedAndUsedOnDifferentThreads) {
  DECLARE_TYPES;

  ObserverListCreator<ObserverListFoo> list_creator;
  Adder a(1);
  // Check with default constructor
  {
    std::unique_ptr<ObserverListFoo> observer_list = list_creator.Create();
    observer_list->AddObserver(&a);
    for (auto& observer : *observer_list) {
      observer.Observe(1);
    }
    EXPECT_EQ(1, a.GetValue());
  }

  // Check with constructor taking explicit policy
  {
    std::unique_ptr<ObserverListFoo> observer_list =
        list_creator.Create(base::ObserverListPolicy::EXISTING_ONLY);
    observer_list->AddObserver(&a);
    for (auto& observer : *observer_list) {
      observer.Observe(1);
    }
    EXPECT_EQ(2, a.GetValue());
  }
}

TYPED_TEST(ObserverListTest, CompactsWhenNoActiveIterator) {
  DECLARE_TYPES;
  using ObserverListConstFoo =
      typename TestFixture::template ObserverList<const Foo>;

  ObserverListConstFoo ol;
  const ObserverListConstFoo& col = ol;

  const Adder a(1);
  const Adder b(2);
  const Adder c(3);

  ol.AddObserver(&a);
  ol.AddObserver(&b);

  EXPECT_TRUE(col.HasObserver(&a));
  EXPECT_FALSE(col.HasObserver(&c));

  EXPECT_TRUE(col.might_have_observers());

  using It = typename ObserverListConstFoo::const_iterator;

  {
    It it = col.begin();
    EXPECT_NE(it, col.end());
    It ita = it;
    EXPECT_EQ(ita, it);
    EXPECT_NE(++it, col.end());
    EXPECT_NE(ita, it);
    It itb = it;
    EXPECT_EQ(itb, it);
    EXPECT_EQ(++it, col.end());

    EXPECT_TRUE(col.might_have_observers());
    EXPECT_EQ(&*ita, &a);
    EXPECT_EQ(&*itb, &b);

    ol.RemoveObserver(&a);
    EXPECT_TRUE(col.might_have_observers());
    EXPECT_FALSE(col.HasObserver(&a));
    EXPECT_EQ(&*itb, &b);

    ol.RemoveObserver(&b);
    EXPECT_TRUE(col.might_have_observers());
    EXPECT_FALSE(col.HasObserver(&a));
    EXPECT_FALSE(col.HasObserver(&b));

    it = It();
    ita = It();
    EXPECT_TRUE(col.might_have_observers());
    ita = itb;
    itb = It();
    EXPECT_TRUE(col.might_have_observers());
    ita = It();
    EXPECT_FALSE(col.might_have_observers());
  }

  ol.AddObserver(&a);
  ol.AddObserver(&b);
  EXPECT_TRUE(col.might_have_observers());
  ol.Clear();
  EXPECT_FALSE(col.might_have_observers());

  ol.AddObserver(&a);
  ol.AddObserver(&b);
  EXPECT_TRUE(col.might_have_observers());
  {
    const It it = col.begin();
    ol.Clear();
    EXPECT_TRUE(col.might_have_observers());
  }
  EXPECT_FALSE(col.might_have_observers());
}

TYPED_TEST(ObserverListTest, DisruptSelf) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter evil(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& observer : observer_list)
    observer.Observe(10);

  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(10, c.total);
  EXPECT_EQ(-10, d.total);
}

TYPED_TEST(ObserverListTest, DisruptBefore) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter evil(&observer_list, &b);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& observer : observer_list)
    observer.Observe(10);
  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-10, b.total);
  EXPECT_EQ(20, c.total);
  EXPECT_EQ(-20, d.total);
}

TYPED_TEST(ObserverListTest, Existing) {
  DECLARE_TYPES;
  ObserverListFoo observer_list(ObserverListPolicy::EXISTING_ONLY);
  Adder a(1);
  AddInObserve<ObserverListFoo> b(&observer_list);
  Adder c(1);
  b.SetToAdd(&c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& observer : observer_list)
    observer.Observe(1);

  EXPECT_FALSE(b.to_add_);
  // B's adder should not have been notified because it was added during
  // notification.
  EXPECT_EQ(0, c.total);

  // Notify again to make sure b's adder is notified.
  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_EQ(1, c.total);
}

template <class ObserverListType,
          class Foo = typename ObserverListType::value_type>
class AddInClearObserve : public Foo {
 public:
  explicit AddInClearObserve(ObserverListType* list)
      : list_(list), added_(false), adder_(1) {}

  void Observe(int /* x */) override {
    list_->Clear();
    list_->AddObserver(&adder_);
    added_ = true;
  }

  bool added() const { return added_; }
  const AdderT<Foo>& adder() const { return adder_; }

 private:
  ObserverListType* const list_;

  bool added_;
  AdderT<Foo> adder_;
};

TYPED_TEST(ObserverListTest, ClearNotifyAll) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  AddInClearObserve<ObserverListFoo> a(&observer_list);

  observer_list.AddObserver(&a);

  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_TRUE(a.added());
  EXPECT_EQ(1, a.adder().total)
      << "Adder should observe once and have sum of 1.";
}

TYPED_TEST(ObserverListTest, ClearNotifyExistingOnly) {
  DECLARE_TYPES;
  ObserverListFoo observer_list(ObserverListPolicy::EXISTING_ONLY);
  AddInClearObserve<ObserverListFoo> a(&observer_list);

  observer_list.AddObserver(&a);

  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_TRUE(a.added());
  EXPECT_EQ(0, a.adder().total)
      << "Adder should not observe, so sum should still be 0.";
}

template <class ObserverListType,
          class Foo = typename ObserverListType::value_type>
class ListDestructor : public Foo {
 public:
  explicit ListDestructor(ObserverListType* list) : list_(list) {}
  ~ListDestructor() override = default;

  void Observe(int x) override { delete list_; }

 private:
  ObserverListType* list_;
};

TYPED_TEST(ObserverListTest, IteratorOutlivesList) {
  DECLARE_TYPES;
  ObserverListFoo* observer_list = new ObserverListFoo;
  ListDestructor<ObserverListFoo> a(observer_list);
  observer_list->AddObserver(&a);

  for (auto& observer : *observer_list)
    observer.Observe(0);

  // There are no EXPECT* statements for this test, if we catch
  // use-after-free errors for observer_list (eg with ASan) then
  // this test has failed.  See http://crbug.com/85296.
}

TYPED_TEST(ObserverListTest, BasicStdIterator) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;

  // An optimization: begin() and end() do not involve weak pointers on
  // empty list.
  EXPECT_FALSE(this->list(observer_list.begin()));
  EXPECT_FALSE(this->list(observer_list.end()));

  // Iterate over empty list: no effect, no crash.
  for (auto& i : observer_list)
    i.Observe(10);

  Adder a(1), b(-1), c(1), d(-1);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (iterator i = observer_list.begin(), e = observer_list.end(); i != e; ++i)
    i->Observe(1);

  EXPECT_EQ(1, a.total);
  EXPECT_EQ(-1, b.total);
  EXPECT_EQ(1, c.total);
  EXPECT_EQ(-1, d.total);

  // Check an iteration over a 'const view' for a given container.
  const ObserverListFoo& const_list = observer_list;
  for (const_iterator i = const_list.begin(), e = const_list.end(); i != e;
       ++i) {
    EXPECT_EQ(1, std::abs(i->GetValue()));
  }

  for (const auto& o : const_list)
    EXPECT_EQ(1, std::abs(o.GetValue()));
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveItself) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveBefore) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &b);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-1, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveAfter) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveAfterFront) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &a);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(1, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveBeforeBack) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &d);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(0, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveFront) {
  DECLARE_TYPES;
  using iterator = typename TestFixture::iterator;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  bool test_disruptor = true;
  for (iterator i = observer_list.begin(), e = observer_list.end(); i != e;
       ++i) {
    i->Observe(1);
    // Check that second call to i->Observe() would crash here.
    if (test_disruptor) {
      EXPECT_FALSE(this->GetCurrent(&i));
      test_disruptor = false;
    }
  }

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, StdIteratorRemoveBack) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);
  observer_list.AddObserver(&disrupter);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TYPED_TEST(ObserverListTest, NestedLoop) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& observer : observer_list) {
    observer.Observe(10);

    for (auto& nested_observer : observer_list)
      nested_observer.Observe(1);
  }

  EXPECT_EQ(15, a.total);
  EXPECT_EQ(-15, b.total);
  EXPECT_EQ(15, c.total);
  EXPECT_EQ(-15, d.total);
}

TYPED_TEST(ObserverListTest, NonCompactList) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1);

  Disrupter disrupter1(&observer_list, true);
  Disrupter disrupter2(&observer_list, true);

  // Disrupt itself and another one.
  disrupter1.SetDoomed(&disrupter2);

  observer_list.AddObserver(&disrupter1);
  observer_list.AddObserver(&disrupter2);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& observer : observer_list) {
    // Get the { nullptr, nullptr, &a, &b } non-compact list
    // on the first inner pass.
    observer.Observe(10);

    for (auto& nested_observer : observer_list)
      nested_observer.Observe(1);
  }

  EXPECT_EQ(13, a.total);
  EXPECT_EQ(-13, b.total);
}

TYPED_TEST(ObserverListTest, BecomesEmptyThanNonEmpty) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;
  Adder a(1), b(-1);

  Disrupter disrupter1(&observer_list, true);
  Disrupter disrupter2(&observer_list, true);

  // Disrupt itself and another one.
  disrupter1.SetDoomed(&disrupter2);

  observer_list.AddObserver(&disrupter1);
  observer_list.AddObserver(&disrupter2);

  bool add_observers = true;
  for (auto& observer : observer_list) {
    // Get the { nullptr, nullptr } empty list on the first inner pass.
    observer.Observe(10);

    for (auto& nested_observer : observer_list)
      nested_observer.Observe(1);

    if (add_observers) {
      observer_list.AddObserver(&a);
      observer_list.AddObserver(&b);
      add_observers = false;
    }
  }

  EXPECT_EQ(12, a.total);
  EXPECT_EQ(-12, b.total);
}

TYPED_TEST(ObserverListTest, AddObserverInTheLastObserve) {
  DECLARE_TYPES;
  ObserverListFoo observer_list;

  AddInObserve<ObserverListFoo> a(&observer_list);
  Adder b(-1);

  a.SetToAdd(&b);
  observer_list.AddObserver(&a);

  auto it = observer_list.begin();
  while (it != observer_list.end()) {
    auto& observer = *it;
    // Intentionally increment the iterator before calling Observe(). The
    // ObserverList starts with only one observer, and it == observer_list.end()
    // should be true after the next line.
    ++it;
    // However, the first Observe() call will add a second observer: at this
    // point, it != observer_list.end() should be true, and Observe() should be
    // called on the newly added observer on the next iteration of the loop.
    observer.Observe(10);
  }

  EXPECT_EQ(-10, b.total);
}

class MockLogAssertHandler {
 public:
  MOCK_METHOD4(
      HandleLogAssert,
      void(const char*, int, const base::StringPiece, const base::StringPiece));
};

#if DCHECK_IS_ON()
TYPED_TEST(ObserverListTest, NonReentrantObserverList) {
  DECLARE_TYPES;
  using NonReentrantObserverListFoo = typename PickObserverList<
      Foo>::template ObserverListType<Foo, /*check_empty=*/false,
                                      /*allow_reentrancy=*/false>;
  NonReentrantObserverListFoo non_reentrant_observer_list;
  Adder a(1);
  non_reentrant_observer_list.AddObserver(&a);

  EXPECT_DCHECK_DEATH({
    for (const Foo& observer : non_reentrant_observer_list) {
      for (const Foo& nested_observer : non_reentrant_observer_list) {
        std::ignore = observer;
        std::ignore = nested_observer;
      }
    }
  });
}

TYPED_TEST(ObserverListTest, ReentrantObserverList) {
  DECLARE_TYPES;
  using ReentrantObserverListFoo = typename PickObserverList<
      Foo>::template ObserverListType<Foo, /*check_empty=*/false,
                                      /*allow_reentrancy=*/true>;
  ReentrantObserverListFoo reentrant_observer_list;
  Adder a(1);
  reentrant_observer_list.AddObserver(&a);
  bool passed = false;
  for (const Foo& observer : reentrant_observer_list) {
    for (const Foo& nested_observer : reentrant_observer_list) {
      std::ignore = observer;
      std::ignore = nested_observer;
      passed = true;
    }
  }
  EXPECT_TRUE(passed);
}
#endif

class TestCheckedObserver : public CheckedObserver {
 public:
  explicit TestCheckedObserver(int* count) : count_(count) {}

  void Observe() { ++(*count_); }

 private:
  int* count_;

  DISALLOW_COPY_AND_ASSIGN(TestCheckedObserver);
};

// A second, identical observer, used to test multiple inheritance.
class TestCheckedObserver2 : public CheckedObserver {
 public:
  explicit TestCheckedObserver2(int* count) : count_(count) {}

  void Observe() { ++(*count_); }

 private:
  int* count_;

  DISALLOW_COPY_AND_ASSIGN(TestCheckedObserver2);
};

using CheckedObserverListTest = ::testing::Test;

// Test Observers that CHECK() when a UAF might otherwise occur.
TEST_F(CheckedObserverListTest, CheckedObserver) {
  // See comments below about why this is unique_ptr.
  auto list = std::make_unique<ObserverList<TestCheckedObserver>>();
  int count1 = 0;
  int count2 = 0;
  TestCheckedObserver l1(&count1);
  list->AddObserver(&l1);
  {
    TestCheckedObserver l2(&count2);
    list->AddObserver(&l2);
    for (auto& observer : *list)
      observer.Observe();
    EXPECT_EQ(1, count1);
    EXPECT_EQ(1, count2);
  }
  {
    auto it = list->begin();
    it->Observe();
    // For CheckedObservers, a CHECK() occurs when advancing the iterator. (On
    // calling the observer method would be too late since the pointer would
    // already be null by then).
    EXPECT_CHECK_DEATH(it++);

    // On the non-death fork, no UAF occurs since the deleted observer is never
    // notified, but also the observer list still has |l2| in it. Check that.
    list->RemoveObserver(&l1);
    EXPECT_TRUE(list->might_have_observers());

    // Now (in the non-death fork()) there's a problem. To delete |it|, we need
    // to compact the list, but that needs to iterate, which would CHECK again.
    // We can't remove |l2| (it's null). But we can delete |list|, which makes
    // the weak pointer in the iterator itself null.
    list.reset();
  }
  EXPECT_EQ(2, count1);
  EXPECT_EQ(1, count2);
}

class MultiObserver : public TestCheckedObserver,
                      public TestCheckedObserver2,
                      public AdderT<UncheckedBase> {
 public:
  MultiObserver(int* checked_count, int* two_count)
      : TestCheckedObserver(checked_count),
        TestCheckedObserver2(two_count),
        AdderT(1) {}
};

// Test that observers behave as expected when observing multiple interfaces
// with different traits.
TEST_F(CheckedObserverListTest, MultiObserver) {
  // Observe two checked observer lists. This is to ensure the WeakPtrFactory
  // in CheckedObserver can correctly service multiple ObserverLists.
  ObserverList<TestCheckedObserver> checked_list;
  ObserverList<TestCheckedObserver2> two_list;

  ObserverList<UncheckedBase>::Unchecked unsafe_list;

  int counts[2] = {};

  auto multi_observer = std::make_unique<MultiObserver>(&counts[0], &counts[1]);
  two_list.AddObserver(multi_observer.get());
  checked_list.AddObserver(multi_observer.get());
  unsafe_list.AddObserver(multi_observer.get());

  auto iterate_over = [](auto* list) {
    for (auto& observer : *list)
      observer.Observe();
  };
  iterate_over(&two_list);
  iterate_over(&checked_list);
  for (auto& observer : unsafe_list)
    observer.Observe(10);

  EXPECT_EQ(10, multi_observer->GetValue());
  for (const auto& count : counts)
    EXPECT_EQ(1, count);

  unsafe_list.RemoveObserver(multi_observer.get());  // Avoid a use-after-free.

  multi_observer.reset();
  EXPECT_CHECK_DEATH(iterate_over(&checked_list));

  for (const auto& count : counts)
    EXPECT_EQ(1, count);
}

}  // namespace base
