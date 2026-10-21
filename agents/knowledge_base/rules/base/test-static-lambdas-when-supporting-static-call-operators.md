---
name: base/test-static-lambdas-when-supporting-static-call-operators
rubric: testing
severity: should_fix
---

**Rule.** When adding or extending callable binding utilities, traits, or
invocable adapters (such as `base::BindOnce`, `base::BindRepeating`,
`base::FunctionRef`, or `ExtractCallableRunType`) to support static
`operator()`, include unit test coverage for C++23 static captureless lambdas
(`[]() static { ... }`) alongside named functor structs with
`static operator()`.

**Why.** C++23 static captureless lambdas have compiler-generated closure types
whose template argument deduction and invocation semantics differ from regular
structs. Testing both named structs and static captureless lambdas ensures type
traits, parameter decay, and invocations operate correctly across all supported
static call operator forms.

## Examples

Bad:

```cpp
// Only testing static call operators using named functor structs:
struct StaticFunctor {
  static int operator()(int i) { return i * 7; }
};
EXPECT_EQ(42, BindRepeating(StaticFunctor(), 6).Run());
```

Good:

```cpp
// Also verifying C++23 static captureless lambdas alongside structs:
struct StaticFunctor {
  static int operator()(int i) { return i * 7; }
};
EXPECT_EQ(42, BindRepeating(StaticFunctor(), 6).Run());
EXPECT_EQ(42, BindRepeating([](int i) static { return i * 7; }, 6).Run());
```
