---
name: general/avoid-subclassing-only-for-static-methods
rubric: design
severity: should_fix
---

**Rule.** Do not introduce a subclass or inheritance hierarchy when the derived
class only defines static factory, singleton accessor, or utility methods
without adding or overriding instance state or behavior. Parameterize,
configure, or directly instantiate the class instead of creating an empty
subclass layer.

**Why.** In Java, static methods are resolved at compile time and cannot be
overridden polymorphically. Subclassing a base class solely to house static
accessor or factory methods creates redundant abstraction layers and unnecessary
class proliferation without adding polymorphic value.

## Examples

Bad:

```cpp
// Subclass only defines static factory methods without adding instance fields or overriding methods
public class SharedPreferencesManager extends SharedPreferencesManagerBase {
    public static SharedPreferencesManager getInstanceForRegistry(@Nullable PreferenceKeyRegistry registry) {
        return new SharedPreferencesManager(registry);
    }
}
```

Good:

```cpp
// Parameterize or directly instantiate the class without an empty inheritance hierarchy
public class SharedPreferencesManager {
    private final PreferenceKeyRegistry mRegistry;

    public static SharedPreferencesManager getInstanceForRegistry(@Nullable PreferenceKeyRegistry registry) {
        return new SharedPreferencesManager(registry);
    }
}
```
