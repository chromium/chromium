---
name: general/use-create-for-testing-over-public-constructor
rubric: readability
severity: should_fix
---

**Rule.** Keep constructors private and declare the intended producer class as a
friend when an object should only be instantiated by that specific class in
production. If unit tests require direct instantiation, provide a static
CreateForTesting() (or Create...ForTesting()) factory method instead of exposing
a public constructor.

**Why.** Exposing a public constructor allows arbitrary production callers to
create unvetted or improperly initialized instances, bypassing intended class
invariants. Providing a static CreateForTesting() factory preserves
encapsulation in production while enabling PRESUBMIT checks against accidental
production use in test-only paths.

## Examples

Bad:

```cpp
class BASE_EXPORT SecurityAttribute {
 public:
  // BAD: Public constructor allows arbitrary production callers to instantiate
  // instances directly rather than routing through AccessToken.
  SecurityAttribute(std::wstring_view name,
                    ULONG type,
                    ULONG flags,
                    std::vector<std::wstring> values);
};
```

Good:

```cpp
class BASE_EXPORT SecurityAttribute {
 public:
  // GOOD: Dedicated static factory for test instantiation with ForTesting suffix.
  static SecurityAttribute CreateForTesting(
      std::wstring_view name,
      bool is_string,
      ULONG flags,
      base::span<std::wstring_view> values);

 private:
  friend class AccessToken;
  SecurityAttribute(std::wstring_view name,
                    ULONG type,
                    ULONG flags,
                    std::vector<std::wstring> values);
};
```
