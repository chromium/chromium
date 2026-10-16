---
name: general/name-tests-after-conditions-not-incidental-mechanics
rubric: readability
severity: should_fix
---

**Rule.** When naming unit tests or test cases (e.g., with `TEST`, `TEST_F`,
`TEST_P`, or `@Test`), name them to describe the specific scenario,
precondition, boundary condition, or state under test (such as `NeverSet`,
`Empty`, `InvalidInput`, or `AlreadyExists`). Do not name test cases after
incidental implementation mechanics, execution steps, or call repetition counts
(such as `Twice` or `MultipleCalls` when verifying behavior on uninitialized or
unset state).

**Why.** Test names serve as living documentation and identify failing test
cases in CI logs and failure reports. Naming tests after the condition or
invariant under test immediately communicates what scenario failed, whereas
naming tests after incidental implementation details obscures the actual test
intent and misleads readers when test implementations evolve.

## Examples

Bad:

```cpp
// Bad: Named after the incidental mechanic of calling the function twice,
// rather than the actual condition under test (entity has no attribute set).
TEST_F(MacUtilTest, TestRemoveQuarantineAttributeTwice) {
  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
}
```

Good:

```cpp
// Good: Named after the specific initial state/condition being verified.
TEST_F(MacUtilTest, TestRemoveQuarantineAttributeNeverSet) {
  VerifyNoQuarantineAttribute(example_folder);
  EXPECT_TRUE(RemoveQuarantineAttribute(example_folder_path));
  VerifyNoQuarantineAttribute(example_folder);
}
```
