---
name: context-hub/prefer-test-future-over-run-loop
rubric: testing
severity: should_fix
---

**Rule.** In Context Hub C++ unit and browser tests, use
`base::test::TestFuture` instead of `base::RunLoop` with manual quit closures
to wait on asynchronous callbacks.

**Why.** `base::test::TestFuture` eliminates boilerplate capture lambdas,
prevents hanging tests when a callback is not invoked, and provides direct
type-safe access to callback results via `Get()` and `Take()`.

## Examples

Bad:

```cpp
TEST_F(ContextHubPageHandlerTest, DeleteTopic) {
  base::RunLoop run_loop;
  bool result = false;
  handler()->DeleteTopic("topic-1", base::BindLambdaForTesting([&](bool ok) {
    result = ok;
    run_loop.Quit();
  }));
  run_loop.Run();
  EXPECT_TRUE(result);
}
```

Good:

```cpp
TEST_F(ContextHubPageHandlerTest, DeleteTopic) {
  base::test::TestFuture<bool> future;
  handler()->DeleteTopic("topic-1", future.GetCallback());
  EXPECT_TRUE(future.Get());
}
```
