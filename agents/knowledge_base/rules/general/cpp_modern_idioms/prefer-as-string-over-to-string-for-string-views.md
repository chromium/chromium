---
name: general/prefer-as-string-over-to-string-for-string-views
rubric: readability
severity: should_fix
---

**Rule.** Do not name methods or functions returning non-allocating
std::string_view ToString() or to_string(). Reserve To<Type>() / ToString() for
conversions that construct and return newly allocated or owned std::string
objects, and use AsStringView(), AsString(), or dedicated view accessors for
functions returning a borrowed std::string_view.

**Why.** The To<Type>() naming convention signals an allocating, copying, or
constructing conversion that creates an independent owned object. Using
ToString() or ambiguous AsString() for non-allocating view types like
std::string_view misleads callers about ownership, lifetime, and performance
cost, whereas AsStringView() or AsString() accurately conveys a borrowed view.

## Examples

Bad:

```cpp
class Token {
 public:
  // Misleadingly implies an owned std::string is constructed:
  std::string_view ToString() const { return value_; }

 private:
  std::string value_;
};
```

Good:

```cpp
class Token {
 public:
  std::string_view AsStringView() const { return value_; }

 private:
  std::string value_;
};
```
