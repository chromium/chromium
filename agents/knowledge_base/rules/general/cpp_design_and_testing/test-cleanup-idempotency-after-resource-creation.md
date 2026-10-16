---
name: general/test-cleanup-idempotency-after-resource-creation
rubric: testing
severity: should_fix
---

**Rule.** When testing cleanup, teardown, or removal functions for idempotency,
verify that repeated removal succeeds after an actual resource, attribute, or
item has been created and deleted (set-remove-remove). Do not rely solely on
calling removal twice on an unpopulated or default target where the resource was
never set.

**Why.** Invoking a cleanup or removal function repeatedly on an unpopulated
target only exercises trivial early-return or missing-key branches. Testing
repeated deletion after an initial creation and successful deletion exercises
the active deletion logic first and verifies that subsequent invocations safely
handle post-deletion state without errors, crashes, or leaks.

## Examples

Bad:

```cpp
// Only tests repeated removal on an item where the attribute was never set:
TEST_F(MacUtilTest, RemoveQuarantineAttributeTwice) {
  base::FilePath folder_path = CreateTempDir();
  EXPECT_TRUE(RemoveQuarantineAttribute(folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(folder_path));
}
```

Good:

```cpp
// Populates the attribute first, then verifies repeated removal:
TEST_F(MacUtilTest, RemoveQuarantineAttributeTwice) {
  base::FilePath folder_path = CreateTempDir();
  ASSERT_TRUE(SetQuarantineAttribute(folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(folder_path));
  EXPECT_TRUE(RemoveQuarantineAttribute(folder_path));
  EXPECT_FALSE(HasQuarantineAttribute(folder_path));
}
```
