---
name: general/prefer-assert-over-expect-before-dereference
rubric: readability
severity: should_fix
---

**Rule.** In test code, use fatal assertions (ASSERT\_*, such as ASSERT_TRUE,
ASSERT_NE, or ASSERT_THAT) instead of non-fatal assertions (EXPECT\_*) when
verifying preconditions—such as ensuring a pointer is non-null, a callback is
valid, an index is in-bounds, or an optional is engaged—before immediately
dereferencing, subscripting, or invoking the tested object.

**Why.** EXPECT\_\* records a test failure but continues executing subsequent
statements. If a precondition fails, subsequent operations (such as calling
.Run(), dereferencing *ptr or ptr->, or indexing out-of-bounds) crash the test
process with a segfault or trigger undefined behavior, obscuring the root cause
and aborting the test suite uncleanly. ASSERT\_* aborts the test function
immediately upon failure.

## Examples

Bad:

```cpp
base::OnceCallback<void(int)> cb =
    base::DoNothingWithBoundArgs(DestructionObserver(was_destroyed));
EXPECT_TRUE(cb);
std::move(cb).Run(4);

Widget* widget = controller->GetActiveWidget();
EXPECT_NE(widget, nullptr);
widget->Layout();
```

Good:

```cpp
base::OnceCallback<void(int)> cb =
    base::DoNothingWithBoundArgs(DestructionObserver(was_destroyed));
ASSERT_TRUE(cb);
std::move(cb).Run(4);

Widget* widget = controller->GetActiveWidget();
ASSERT_NE(widget, nullptr);
widget->Layout();
```
