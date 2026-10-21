---
name: base/test-construction-and-assignment-for-callback-helpers
rubric: testing
severity: should_fix
---

**Rule.** When adding or unit testing callback helpers, tags, wrappers, or
binding adapters (such as `base::DoNothingWithBoundArgs`, `base::NullCallback`,
or `base::DoNothing`), write tests exercising all four combinations: direct
construction and assignment for both `base::OnceCallback` and
`base::RepeatingCallback`. Additionally, when the helper captures or binds
arguments, verify object lifetime and cleanup by passing a test struct that
observes its own destruction (e.g., via a boolean flag in its destructor) rather
than testing solely with primitive scalars.

**Why.** `base::OnceCallback` and `base::RepeatingCallback` implement
independent templated conversion constructors and assignment operators. Testing
only one callback variant or only assignment leaves remaining template
definitions uninstantiated during compilation, which can conceal syntax errors,
missing `std::move`s, or invalid return values (such as returning `this` instead
of `*this`). Furthermore, testing binding utilities only with primitive scalar
types cannot detect argument retention bugs, premature destruction, or memory
leaks; a destruction-observing helper struct verifies that bound arguments are
properly released at the expected point in their lifecycle.

## Examples

Bad:

```cpp
// Incomplete: tests only RepeatingCallback assignment with a scalar integer,
// leaving OnceCallback, direct construction, and bound argument cleanup unverified.
TEST(CallbackHelpersTest, DoNothingWithBoundArgsAssignToRepeating) {
  base::RepeatingCallback<void(int)> cb;
  cb = base::DoNothingWithBoundArgs(42);
  EXPECT_TRUE(cb);
  cb.Run(0);
}
```

Good:

```cpp
struct DestructionObserver {
  explicit DestructionObserver(bool* destroyed) : destroyed_(destroyed) {}
  ~DestructionObserver() { *destroyed_ = true; }
  raw_ptr<bool> destroyed_;
};

TEST(CallbackHelpersTest, DoNothingWithBoundArgs) {
  // Test all 4 combinations: direct construction and assignment for both OnceCallback
  // and RepeatingCallback, verifying argument destruction in each case.
  bool was_destroyed = false;
  {
    base::OnceCallback<void(int)> cb(
        base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed)));
    EXPECT_FALSE(was_destroyed);
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);
  }
  was_destroyed = false;
  {
    base::OnceCallback<void(int)> cb;
    cb = base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed));
    EXPECT_FALSE(was_destroyed);
    std::move(cb).Run(4);
    EXPECT_TRUE(was_destroyed);
  }
  was_destroyed = false;
  {
    base::RepeatingCallback<void(int)> cb(
        base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed)));
    EXPECT_FALSE(was_destroyed);
    cb.Run(4);
    EXPECT_FALSE(was_destroyed);
    cb.Reset();
    EXPECT_TRUE(was_destroyed);
  }
  was_destroyed = false;
  {
    base::RepeatingCallback<void(int)> cb;
    cb = base::DoNothingWithBoundArgs(DestructionObserver(&was_destroyed));
    EXPECT_FALSE(was_destroyed);
    cb.Run(4);
    EXPECT_FALSE(was_destroyed);
    cb.Reset();
    EXPECT_TRUE(was_destroyed);
  }
}
```
