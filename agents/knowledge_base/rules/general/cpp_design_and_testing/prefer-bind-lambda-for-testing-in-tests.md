---
name: general/prefer-bind-lambda-for-testing-in-tests
rubric: readability
severity: nit
---

**Rule.** In test code, prefer base::BindLambdaForTesting with a capturing
lambda (e.g., [&]) over base::BindOnce or base::BindRepeating with a captureless
lambda and explicitly bound pointer or reference arguments.

**Why.** Capturing local variables directly via base::BindLambdaForTesting
eliminates verbose parameter binding boilerplate, avoids pointer dereferencing
indirection inside the callback body, and prevents confusing variable shadowing
between bound parameter names and enclosing test locals.

## Examples

Bad:

```cpp
auto cb = base::BindOnce(
    [](OnceClosureList* reg, CallbackListSubscription* sub) {
      reg->Notify();
      *sub = {};
    },
    &cb_reg, &sub_b);
```

Good:

```cpp
auto cb = base::BindLambdaForTesting([&]() {
  cb_reg.Notify();
  sub_b = {};
});
```
