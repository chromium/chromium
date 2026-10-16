---
name: general/keep-unit-tests-focused-on-target-operation
rubric: readability
severity: optional
---

**Rule.** In unit tests, keep each test case focused solely on the specific
operation, lifecycle transition, or invariant being verified (such as move
construction or error handling). Avoid introducing extraneous operations—such as
mutating states, invoking unrelated methods, or assigning out-values—solely for
perceived symmetry across test cases when they do not contribute to verifying
the target behavior.

**Why.** Extraneous operations in unit tests add visual clutter, obscure the
primary invariant under test, and create ambiguity about which method or
contract is actually being validated. Keeping test cases minimal and focused on
single operations isolates failure modes and makes tests easier to understand
and maintain without forcing artificial symmetry.

## Examples

Bad:

```cpp
// BAD: Mutating state through `r2` is extraneous in a test focused strictly
// on verifying receiver move construction.
TEST(ScopedGenericTest, ReceiverMoveConstruct) {
  ScopedInt a(0, traits);
  {
    ScopedInt::Receiver r1(a);
    ScopedInt::Receiver r2(std::move(r1));
    EXPECT_TRUE(values_freed.empty());
    EXPECT_EQ(0, a.get());

    *r2.get() = 1;
  }
  EXPECT_THAT(values_freed, testing::ElementsAre(0));
  EXPECT_EQ(1, a.get());
}
```

Good:

```cpp
// GOOD: Test is focused solely on verifying receiver move construction and destruction.
TEST(ScopedGenericTest, ReceiverMoveConstruct) {
  ScopedInt a(0, traits);
  {
    ScopedInt::Receiver r1(a);
    ScopedInt::Receiver r2(std::move(r1));
    EXPECT_TRUE(values_freed.empty());
    EXPECT_EQ(0, a.get());
  }
  EXPECT_THAT(values_freed, testing::ElementsAre(0));
}
```
