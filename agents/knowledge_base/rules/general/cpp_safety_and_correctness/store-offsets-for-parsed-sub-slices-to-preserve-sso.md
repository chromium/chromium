---
name: general/store-offsets-for-parsed-sub-slices-to-preserve-sso
rubric: safety
severity: should_fix
---

**Rule.** When designing a movable class that owns a string buffer and
represents parsed sub-slices, store relative offsets and lengths rather than
storing string views or switching to std::vector for pointer stability.
Synthesize string views on demand via iterators or accessors to preserve Small
String Optimization (SSO) while preventing dangling self-referential pointers
across moves.

**Why.** Storing string_view or raw pointer members into an owned string buffer
creates self-referential pointers that dangle when moved if Small String
Optimization (SSO) inline storage is active. Switching to std::vector prevents
dangling pointers across moves but pessimistically forces heap allocations even
for short strings; storing integer offsets allows using std::string while
remaining safely movable.

## Examples

Bad:

```cpp
// BAD: Switching to std::vector to ensure pointer stability across moves
// pessimistically forces heap allocations even for short strings.
struct ParsedNativeEnvironmentString {
  std::vector<wchar_t> unparsed;
  std::vector<base::wcstring_view> parsed;
};
```

Good:

```cpp
// GOOD: Store std::wstring to leverage Small String Optimization (SSO) and record
// offset/length pairs, synthesizing string views on demand so moves remain safe.
class ParsedNativeEnvironmentString {
 public:
  class Iterator {
   public:
    std::wstring_view operator*() const {
      return std::wstring_view(unparsed_).substr(it_->first, it_->second);
    }
    // ...
   private:
    std::wstring_view unparsed_;
    std::vector<std::pair<size_t, size_t>>::const_iterator it_;
  };
  Iterator begin() const;
  Iterator end() const;
 private:
  std::wstring unparsed_;
  std::vector<std::pair<size_t, size_t>> offsets_;
};
```
