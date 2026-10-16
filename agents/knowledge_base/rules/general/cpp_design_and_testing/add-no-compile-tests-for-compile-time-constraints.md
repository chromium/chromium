---
name: general/add-no-compile-tests-for-compile-time-constraints
rubric: testing
severity: should_fix
---

**Rule.** When introducing or extending compile-time traits, template
constraints, concept requirements, traits bags (such as `TaskTraits` or
`traits_bag`), or access-control types (such as `base::PassKey`), add or update
no-compile tests (`.nc`) with Clang diagnostic annotations (`// expected-error`)
to verify that forbidden types, conflicting traits, unauthorized default
construction, uniform initialization via `{}`, and constraint violations fail
compilation. Conversely, when a compile-time restriction is intentionally
removed or relaxed, delete obsolete `.nc` tests rather than keeping them with
artificial diagnostics (such as unused function warnings), and verify the newly
supported behavior with positive unit tests.

**Why.** Compile-time constraints and access controls rely on static compiler
rejection for safety and encapsulation; positive tests cannot verify that
invalid constructions are rejected. Conversely, retaining obsolete no-compile
tests with unrelated diagnostics provides false confidence, creates dead test
code, and masks that the underlying constraint has been lifted.

## Examples

Bad:

```cpp
// 1. Missing .nc test when introducing access-control / constraints:
// Only testing valid constructions with positive tests:
static_assert(std::is_constructible_v<RestrictedMulti, PassKey<A, B>>);
constexpr TaskTraits kTraits = {TaskPriority::USER_VISIBLE};

// 2. Retaining obsolete .nc test after lifting compile-time restriction:
// In .nc file when EnumSet becomes constexpr-constructible:
size_t LargeSparseEnum() {  // expected-error {{unused function 'LargeSparseEnum'}}
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;
  constexpr auto set = TestEnumSparseSet::All();
  return set.size();
}
```

Good:

```cpp
// 1. Add .nc tests verifying static compilation failures:
// In base/task/task_traits_nocompile.nc:
constexpr TaskTraits traits1 = {MaxThreadType(ThreadType::kDefault)};  // expected-error@*:* {{no matching constructor for initialization}}

// In base/types/pass_key_nocompile.nc:
void CannotCreateFromBraces() {
  Restricted r({});  // expected-error@*:* {{no matching constructor for initialization}}
}

// 2. Delete obsolete .nc test and verify newly supported behavior in unit test:
// In enum_set_unittest.cc:
TEST(EnumSetTest, LargeSparseEnumConstexpr) {
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;
  constexpr auto set = TestEnumSparseSet::All();
  EXPECT_EQ(set.size(), 100u);
}
```
