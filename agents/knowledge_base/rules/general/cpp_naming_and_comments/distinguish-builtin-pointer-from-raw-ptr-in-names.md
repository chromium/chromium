---
name: general/distinguish-builtin-pointer-from-raw-ptr-in-names
rubric: readability
severity: nit
---

**Rule.** When naming identifiers, type aliases, template parameters, or test
cases that handle or compare built-in C++ pointers (T\*), refer to them as
BuiltinPtr, CppPtr, or PlainPtr rather than RawPtr—especially in contexts where
base::raw_ptr<T> also appears. Reserve RawPtr and raw_ptr for Chromium's
base::raw_ptr<T> smart pointer.

**Why.** In Chromium, raw_ptr<T> (MiraclePtr) is a widely used smart pointer
class with specific memory-safety semantics. Referring to built-in C++ pointers
(T\*) as 'RawPtr' creates naming collisions and ambiguity (e.g., test cases
named RawPtrVsRawPtr), making it unclear whether a type or variable represents a
built-in pointer or a base::raw_ptr<T>.

## Examples

Bad:

```cpp
// Ambiguous test name and type aliases comparing raw_ptr<T> and T*:
using RawPtr = int*;
using SmartRawPtr = raw_ptr<int>;

TEST(TransparentHashTest, RawPtrVsRawPtr) {
  int val = 42;
  raw_ptr<int> smart_ptr(&val);
  int* raw_ptr_val = &val;
  EXPECT_EQ(TransparentHash()(smart_ptr), TransparentHash()(raw_ptr_val));
}
```

Good:

```cpp
// Clearly disambiguated naming between built-in T* and base::raw_ptr<T>:
using BuiltinPtr = int*;
using RawPtr = raw_ptr<int>;

TEST(TransparentHashTest, RawPtrVsBuiltinPtr) {
  int val = 42;
  raw_ptr<int> smart_ptr(&val);
  int* builtin_ptr = &val;
  EXPECT_EQ(TransparentHash()(smart_ptr), TransparentHash()(builtin_ptr));
}
```
