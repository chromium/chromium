---
name: general/prefer-test-future-over-runloop
rubric: readability
severity: optional
---

**Rule.** Prefer base::test::TestFuture over manual base::RunLoop instances and
callback lambdas that invoke QuitClosure() when awaiting asynchronous results or
signals in tests. Use future.GetCallback(), future.GetSequenceBoundCallback(),
future.Wait(), or future.Get() to observe results concisely and safely.

**Why.** base::test::TestFuture provides a standardized, type-safe API for
capturing callback values and awaiting completion, eliminating boilerplate
RunLoop management, lambda wrappers, captured local state, and manual
quit-closure signaling.

## Examples

Bad:

```cpp
base::RunLoop run_loop;
std::optional<Result> captured_result;
service->FetchData(base::BindLambdaForTesting(
    [&](Result result) {
      captured_result = result;
      run_loop.QuitClosure().Run();
    }));
run_loop.Run();
EXPECT_TRUE(captured_result.has_value());
```

Good:

```cpp
base::test::TestFuture<Result> future;
service->FetchData(future.GetCallback());
EXPECT_TRUE(future.Wait());
const Result& result = future.Get();
```
