---
name: general/document-test-hooks-in-production-classes
rubric: readability
severity: nit
---

**Rule.** When adding test-only hooks, static state overrides, backdoor fields,
or @VisibleForTesting members (\*ForTesting) to production classes, document why
the hook is necessary and what test scenarios or harnesses require it. Do not
leave test-only accessors or overrides undocumented or with generic comments
that obscure why production encapsulation was bypassed.

**Why.** Test hooks and static overrides in production classes bypass normal
encapsulation and couple production code to specific test harnesses. Documenting
the rationale and required test scenarios clarifies why standard dependency
injection could not be used and prevents accidental misuse or breakage during
refactoring.

## Examples

Bad:

```cpp
// Java: Undocumented test backdoor fields
public static @Nullable Runnable sDoFinchInitHookForTesting;
public static boolean sSkipFinchInitForTesting;

// C++: Undocumented test hook method
void SetHttpServerPortForTesting(uint16_t port);
```

Good:

```cpp
// Java: Document specific test scenario requiring the hook
// Allows test suites to set command-line flags before Finch is initialized.
public static @Nullable Runnable sDoFinchInitHookForTesting;

// C++: Document why the test override is needed over standard initialization
// Overrides the bound HTTP port during embedded browser tests where the test
// server starts on a dynamic port before the network service initializes.
void SetHttpServerPortForTesting(uint16_t port);
```
