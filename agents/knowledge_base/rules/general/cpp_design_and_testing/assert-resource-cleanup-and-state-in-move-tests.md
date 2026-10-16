---
name: general/assert-resource-cleanup-and-state-in-move-tests
rubric: testing
severity: should_fix
---

**Rule.** When unit testing RAII types, smart pointers, or resource management
operations (such as move assignment/construction, copy, swap, comparison, or
reset):

1. Initialize test instances with distinct values rather than identical dummy
   values so that value transfer, replacement, or operand order is unambiguously
   distinguishable.
2. Assert observable post-conditions and side effects: verify that overwritten
   targets release previous resources on move assignment, moved-from sources
   reset to empty or invalid states, and destination instances receive expected
   values.
3. Enclose tested instances in explicit nested scope blocks (`{ ... }`) to
   assert that remaining resources are properly freed upon destruction (e.g.
   verifying tracking structures such as freed-value lists or mock deleters).

**Why.** Initializing multiple test instances with identical values masks
whether an operation actually transferred state, swapped operands, or was a
no-op. Furthermore, testing only getters or return values without asserting
post-conditions and destruction side effects fails to detect memory leaks,
double frees, dangling handles, or unreset source states. Using distinct initial
values and nested scope blocks enables unambiguous verification of resource
transfers and post-destruction cleanup.

## Examples

Bad:

```cpp
// BAD: Instances use identical initial values (hiding no-ops), move assignment
// does not assert target cleanup or source reset, and swap lacks scope destruction checks.
TEST(ScopedGenericTest, ReceiverMoveAssign) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  ScopedInt a(0, traits), b(0, traits);  // Identical values
  ScopedInt::Receiver r1(a), r2(b);

  r2 = std::move(r1);
  *r2.get() = 42;
}

TEST(ScopedGenericTest, Swap) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  ScopedInt a(123, traits), b(456, traits);

  swap(a, b);
  EXPECT_EQ(456, a.get());
  EXPECT_EQ(123, b.get());
}
```

Good:

```cpp
// GOOD: Instances initialize distinct values, move assignment asserts target release
// and source reset, and swap test uses a nested scope block to verify destruction side effects.
TEST(ScopedGenericTest, ReceiverMoveAssign) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  ScopedInt a(0, traits), b(1, traits);  // Distinct values
  ScopedInt::Receiver r1(a), r2(b);

  r2 = std::move(r1);
  EXPECT_THAT(values_freed, testing::ElementsAre(1));
  EXPECT_EQ(IntTraits::InvalidValue(), b.get());
}

TEST(ScopedGenericTest, Swap) {
  std::vector<int> values_freed;
  IntTraits traits(&values_freed);
  {
    ScopedInt a(0, traits), b(1, traits);
    swap(a, b);
    EXPECT_TRUE(values_freed.empty());
    EXPECT_EQ(1, a.get());
    EXPECT_EQ(0, b.get());
  }
  EXPECT_THAT(values_freed, testing::ElementsAre(0, 1));
}
```
