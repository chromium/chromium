---
name: general/use-assert-ok-and-assign-for-expected-values-in-tests
rubric: readability
severity: nit
---

**Rule.** In test code, use ASSERT_OK_AND_ASSIGN (or ASSERT_OK/EXPECT_OK from
base/test/gmock_expected_support.h) when asserting and unwrapping a
base::expected or std::optional return value, rather than manually checking
has_value() and calling .value().

**Why.** ASSERT_OK_AND_ASSIGN eliminates boilerplate and ensures that if an
error or disengaged state occurs, the test halts immediately with descriptive
error diagnostics rather than continuing past a non-fatal assertion failure into
an invalid .value() dereference.

## Examples

Bad:

```cpp
auto result = LoadAllImportsForDll("not-a-module-on-windows.dll");
EXPECT_TRUE(result.has_value());
EXPECT_FALSE(result.value());
```

Good:

```cpp
ASSERT_OK_AND_ASSIGN(bool loaded,
                     LoadAllImportsForDll("not-a-module-on-windows.dll"));
EXPECT_FALSE(loaded);
```
