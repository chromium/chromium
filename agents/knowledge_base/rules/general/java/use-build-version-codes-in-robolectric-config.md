---
name: general/use-build-version-codes-in-robolectric-config
rubric: readability
severity: should_fix
---

**Rule.** In Android Robolectric JUnit tests, specify target SDK versions in
`@Config(sdk = ...)` using named `Build.VERSION_CODES.<VERSION>` (or
`VERSION_CODES.<VERSION>`) constants rather than raw integer literals. Include a
preceding comment explaining the specific platform APIs, behaviors, or version
constraints that require pinning that SDK level.

**Why.** Using named `Build.VERSION_CODES` constants eliminates magic numbers
and makes test SDK configurations easily searchable across the codebase when
older Android versions are deprecated or removed. Preceding comments document
the platform capability or behavioral divergence being tested, clarifying why
the test cannot run under default or current SDK levels.

## Examples

Bad:

```cpp
// Bad: Raw integer literal for SDK level without explanatory comment
@Config(sdk = 30)
public class KeyboardUtilsTest {
    @Test
    public void testKeyboardHeight() {
        // ...
    }
}
```

Good:

```cpp
// Good: Named Build.VERSION_CODES constant with a comment explaining the requirement
// WindowInsets.Type.ime() and WindowInsets#getInsets(int) were added in Android R (API 30).
@Config(sdk = Build.VERSION_CODES.R)
public class KeyboardUtilsTest {
    @Test
    public void testKeyboardHeight() {
        // ...
    }
}
```
