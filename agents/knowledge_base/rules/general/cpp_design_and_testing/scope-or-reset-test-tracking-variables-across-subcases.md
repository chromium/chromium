---
name: general/scope-or-reset-test-tracking-variables-across-subcases
rubric: testing
severity: should_fix
---

**Rule.** When testing multiple sequential variations, operations, or side
effects within a single unit test function, wrap each distinct step or scenario
in an explicit scoped block (`{ ... }`) and declare tracking variables (such as
boolean flags, call counters, observers, or callbacks) locally within each
block, or explicitly reinitialize them between steps. Do not reuse unreset
tracking variables across subcases or rely on artificial numbered variable names
(e.g., `cb1`, `cb2`, `was_destroyed1`, `was_destroyed2`) in a flat function
scope.

**Why.** Reusing unreset tracking variables across sequential subcases allows
mutated state (such as `true` or a non-zero count) to persist, causing
subsequent assertions to pass vacuously even if an operation fails. Conversely,
keeping all sequential steps in a flat scope forces clumsy numbered identifiers
and keeps objects alive longer than intended. Scoping each scenario into its own
`{ ... }` block isolates variable lifetimes, triggers destructors
deterministically at block exit, guarantees fresh tracking state for assertions,
and allows clean, concise variable reuse.

## Examples

Bad:

```cpp
// Bad: Flat scope reuses an unreset variable (causing vacuous passes)
// or uses artificial numbered variable names to avoid redeclaration.
TEST(CallbackHelpersTest, DoNothingWithBoundArgs) {
  // Variant 1: Unreset variable persists mutated state into subcase 2.
  bool was_destroyed = false;
  {
    base::OnceCallback<void(int)> cb(
        base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed)));
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);
  }
  {
    base::OnceCallback<void(int)> cb;
    cb = base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed));
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);  // Passes vacuously even if destruction fails!
  }

  // Variant 2: Flat scope requiring numbered variable names.
  bool was_destroyed1 = false;
  base::OnceCallback<void(int)> cb1(
      base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed1)));
  EXPECT_FALSE(was_destroyed1);
  bool was_destroyed2 = false;
  base::OnceCallback<void(int)> cb2;
  cb2 = base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed2));
  EXPECT_FALSE(was_destroyed2);
}
```

Good:

```cpp
// Good: Wrapping each subcase in a scoped block isolates lifetimes and
// tracking state while allowing clean variable reuse.
TEST(CallbackHelpersTest, DoNothingWithBoundArgs) {
  {
    bool was_destroyed = false;
    base::OnceCallback<void(int)> cb(
        base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed)));
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);
  }
  {
    bool was_destroyed = false;
    base::OnceCallback<void(int)> cb;
    cb = base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed));
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);
  }
}
```
