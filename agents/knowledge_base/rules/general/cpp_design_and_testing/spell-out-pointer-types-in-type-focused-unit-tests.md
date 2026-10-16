---
name: general/spell-out-pointer-types-in-type-focused-unit-tests
rubric: readability
severity: nit
---

**Rule.** In unit tests specifically exercising traits, heterogeneous container
lookup, or operations across distinct pointer or wrapper types, explicitly spell
out the pointer types (such as scoped_refptr<T>, std::unique_ptr<T>, or custom
pointer aliases) rather than deducing them with auto. In production code and
general test logic, continue declaring locals initialized with std::make_unique
or base::MakeRefCounted using auto.

**Why.** While auto is standard with make_unique and MakeRefCounted in general
code, unit tests verifying heterogeneous lookups, trait dispatch, or conversions
across distinct pointer types are clearer and document test intent when the
wrapper and pointer types are explicitly stated.

## Examples

Bad:

```cpp
TEST(TransparentHashTest, SharedPtrVsBuiltinPtr) {
  auto s1 = std::make_shared<int>(1);
  auto s2 = s1;
  auto s3 = std::make_shared<int>(2);
  EXPECT_EQ(TransparentHash()(s1), TransparentHash()(s2));
}
```

Good:

```cpp
TEST(TransparentHashTest, SharedPtrVsBuiltinPtr) {
  using SharedPtrInt = decltype(std::make_shared<int>());
  SharedPtrInt s1 = std::make_shared<int>(1);
  SharedPtrInt s2 = s1;
  SharedPtrInt s3 = std::make_shared<int>(2);
  EXPECT_EQ(TransparentHash()(s1), TransparentHash()(s2));
}
```
