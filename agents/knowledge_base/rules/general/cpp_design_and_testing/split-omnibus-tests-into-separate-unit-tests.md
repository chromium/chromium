---
name: general/split-omnibus-tests-into-separate-unit-tests
rubric: design
severity: should_fix
---

**Rule.** Avoid combining multiple independent test scenarios into a single
omnibus test function using scoped blocks (`{ ... }`) to reuse environment or
fixture setup. Split them into distinct, focused `TEST`, `TEST_F`, or `TEST_P`
test cases.

**Why.** Bundling multiple independent test scenarios into a single test
function via local scoped blocks obscures which scenario failed when a failure
occurs. Splitting them into distinct test cases produces actionable failure
reports, guarantees test isolation, and allows individual scenarios to be
targeted with test filters.

## Examples

Bad:

```cpp
TEST_F(MessagePumpAppleInitialNestingTest, SetInitialNestingLevelBoundaries) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  // Default behavior (no call to SetInitialNestingLevel).
  {
    TestMessagePump pump;
    PlaceholderDelegate delegate;
    pump.Attach(&delegate);
    EXPECT_EQ(1, pump.nesting_level());
    pump.Detach();
  }

  // Test SetInitialNestingLevel(1), which should behave like the default.
  MessagePumpUIApplication::SetInitialNestingLevel(1);
  {
    TestMessagePump pump;
    PlaceholderDelegate delegate;
    pump.Attach(&delegate);
    EXPECT_EQ(1, pump.nesting_level());
    pump.Detach();
  }
}
```

Good:

```cpp
TEST_F(MessagePumpAppleInitialNestingTest, DefaultNestingLevel) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(1, pump.nesting_level());
  pump.Detach();
}

TEST_F(MessagePumpAppleInitialNestingTest, SetInitialNestingLevelOne) {
  test::SingleThreadTaskEnvironment task_environment(
      test::SingleThreadTaskEnvironment::MainThreadType::UI);

  MessagePumpUIApplication::SetInitialNestingLevel(1);
  TestMessagePump pump;
  PlaceholderDelegate delegate;
  pump.Attach(&delegate);
  EXPECT_EQ(1, pump.nesting_level());
  pump.Detach();
}
```
